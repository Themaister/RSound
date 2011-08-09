/*  RSound - A PCM audio client/server
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *  Copyright (C) 2011 - Chris Moeller
 * 
 *  RSound is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RSound is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RSound.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "coreaudio.h"
#include "../rsound.h"

static void coreaudio_close(void *data)
{
   coreaudio_t *interface = data;

   if (!interface)
      return;

   if (interface->audio_unit_inited)
   {
      AudioOutputUnitStop(interface->audio_unit);
      CloseComponent(interface->audio_unit);
   }

   if (interface->buffer)
      free(interface->buffer);

   pthread_mutex_destroy(&interface->mutex);
   pthread_cond_destroy(&interface->cond);
   free(interface);
}

static OSStatus audio_callback(void *userdata, AudioUnitRenderActionFlags *action_flags,
      const AudioTimeStamp *time_stamp, UInt32 bus_number, UInt32 num_frames, AudioBufferList *io_data)
{
   (void)time_stamp;
   (void)bus_number;
   (void)num_frames;

   coreaudio_t *interface = userdata;

   if (!io_data)
      return noErr;
   if (io_data->mNumberBuffers != 1)
      return noErr;

   static unsigned cnt = 0;
   cnt++;
   size_t to_write = io_data->mBuffers[0].mDataByteSize;
   uint8_t *output = io_data->mBuffers[0].mData;

   pthread_mutex_lock(&interface->mutex);

   size_t read_avail = interface->valid_size;

   if (read_avail < to_write)
   {
      *action_flags = kAudioUnitRenderAction_OutputIsSilence;
      memset(output, 0, to_write);
      pthread_mutex_unlock(&interface->mutex);
      pthread_cond_signal(&interface->cond); // Fixes technically possible deadlock.
      return noErr;
   }

   const uint8_t *sample1 = interface->buffer + interface->read_ptr;
   const uint8_t *sample2 = interface->buffer;
   size_t to_write1 = to_write;
   size_t to_write2 = 0;

   // Buffer circles, write in two parts.
   if (interface->read_ptr + to_write > interface->buffer_size)
   {
      to_write1 = interface->buffer_size - interface->read_ptr;
      to_write2 = to_write - to_write1;
   }

   memcpy(output, sample1, to_write1);
   memcpy(output + to_write1, sample2, to_write2);

   interface->valid_size -= to_write;
   interface->read_ptr = (interface->read_ptr + to_write) & interface->buffer_mask;

   pthread_mutex_unlock(&interface->mutex);
   pthread_cond_signal(&interface->cond);

   return noErr;
}

static int coreaudio_init(void **data)
{
   CFRunLoopRef run_loop = NULL;
   AudioObjectPropertyAddress address = {
      kAudioHardwarePropertyRunLoop,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster
   };

   coreaudio_t *sound = calloc(1, sizeof(coreaudio_t));
   if (sound == NULL)
      return -1;

   pthread_mutex_init(&sound->mutex, NULL);
   pthread_cond_init(&sound->cond, NULL);

   AudioObjectSetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, sizeof(CFRunLoopRef), &run_loop);
   *data = sound;

   return 0;
}

static int coreaudio_open(void* data, wav_header_t *w)
{
   coreaudio_t *interface = data;

   ComponentDescription desc = {
      .componentType = kAudioUnitType_Output,
      .componentSubType = kAudioUnitSubType_HALOutput,
      .componentManufacturer = kAudioUnitManufacturer_Apple,
   };

   Component comp = FindNextComponent(NULL, &desc);
   if (comp == NULL)
      return -1;

   OSStatus result = OpenAComponent(comp, &interface->audio_unit);
   if (result != noErr)
      return -1;

   interface->audio_unit_inited = true;

   AudioStreamBasicDescription requested_desc; 
   memset(&requested_desc, 0, sizeof(requested_desc));

   switch (w->rsd_format)
   {
      case RSD_ALAW:
         requested_desc.mFormatID = kAudioFormatALaw;
         requested_desc.mFormatFlags = 0;
         requested_desc.mBitsPerChannel = 8;
         break;

      case RSD_MULAW:
         requested_desc.mFormatID = kAudioFormatULaw;
         requested_desc.mFormatFlags = 0;
         requested_desc.mBitsPerChannel = 8;
         break;

      default:
         requested_desc.mFormatID = kAudioFormatLinearPCM;
         requested_desc.mFormatFlags = kAudioFormatFlagIsPacked;

         switch (w->rsd_format)
         {
            case RSD_S32_BE:
            case RSD_U32_BE:
            case RSD_S16_BE:
            case RSD_U16_BE:
               requested_desc.mFormatFlags |= kAudioFormatFlagIsBigEndian;
               break;
         }

         switch (w->rsd_format)
         {
            case RSD_S32_LE:
            case RSD_S32_BE:
            case RSD_S16_LE:
            case RSD_S16_BE:
            case RSD_S8:
               requested_desc.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
               break;
         }

         switch (w->rsd_format)
         {
            case RSD_S32_LE:
            case RSD_S32_BE:
            case RSD_U32_LE:
            case RSD_U32_BE:
               requested_desc.mBitsPerChannel = 32;
               break;

            case RSD_S16_LE:
            case RSD_S16_BE:
            case RSD_U16_LE:
            case RSD_U16_BE:
               requested_desc.mBitsPerChannel = 16;
               break;

            case RSD_S8:
            case RSD_U8:
               requested_desc.mBitsPerChannel = 8;
               break;
         }
         break;
   }

   requested_desc.mChannelsPerFrame = w->numChannels;
   requested_desc.mSampleRate = w->sampleRate;
   requested_desc.mFramesPerPacket = 1;
   requested_desc.mBytesPerFrame = requested_desc.mBitsPerChannel * requested_desc.mChannelsPerFrame / 8;
   requested_desc.mBytesPerPacket = requested_desc.mBytesPerFrame * requested_desc.mFramesPerPacket;

   result = AudioUnitSetProperty(interface->audio_unit, kAudioUnitProperty_StreamFormat,
         kAudioUnitScope_Input, 0, &requested_desc, sizeof(requested_desc));
   if (result != noErr)
      return -1;

   AudioStreamBasicDescription actual_desc;
   UInt32 param_size = sizeof(actual_desc);
   result = AudioUnitGetProperty(interface->audio_unit, kAudioUnitProperty_StreamFormat,
         kAudioUnitScope_Input, 0, &actual_desc, &param_size);
   if (result != noErr)
      return -1;

   if (fabs(requested_desc.mSampleRate - actual_desc.mSampleRate) > requested_desc.mSampleRate * 0.05)
      return -1;
   if (requested_desc.mChannelsPerFrame != actual_desc.mChannelsPerFrame)
      return -1;
   if (requested_desc.mBitsPerChannel != actual_desc.mBitsPerChannel)
      return -1;
   if (actual_desc.mFormatID == kAudioFormatALaw || actual_desc.mFormatID == kAudioFormatULaw)
      actual_desc.mFormatFlags = 0;
   if (requested_desc.mFormatID != actual_desc.mFormatID)
      return -1;
   if (requested_desc.mFormatFlags != actual_desc.mFormatFlags)
      return -1;

   // TODO: Make this behave properly with multiple channels (> 2).
   AudioChannelLayout layout = {
      .mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelBitmap,
      .mChannelBitmap = (1 << w->numChannels) - 1,
   };

   result = AudioUnitSetProperty(interface->audio_unit, kAudioUnitProperty_AudioChannelLayout,
         kAudioUnitScope_Input, 0, &layout, sizeof(layout));
   if (result != noErr)
      return -1;

   AURenderCallbackStruct input = {
      .inputProc = audio_callback,
      .inputProcRefCon = interface,
   };

   result = AudioUnitSetProperty(interface->audio_unit, kAudioUnitProperty_SetRenderCallback,
         kAudioUnitScope_Input, 0, &input, sizeof(input));
   if (result != noErr)
      return -1;

   result = AudioUnitInitialize(interface->audio_unit);
   if (result != noErr)
      return -1;

   interface->buffer_size = DEFAULT_CHUNK_SIZE * 32;
   interface->buffer_mask = interface->buffer_size - 1;
   interface->buffer = calloc(interface->buffer_size, sizeof(uint8_t));
   if (!interface->buffer)
      return -1;

   result = AudioOutputUnitStart(interface->audio_unit);
   if (result != noErr)
      return -1;

   return 0;
}

static size_t coreaudio_write(void *data, const void* buf_, size_t size)
{
   coreaudio_t *interface = data;
   const uint8_t *buf = buf_;

   size_t written = 0;
   while (size)
   {
      pthread_mutex_lock(&interface->mutex);

      size_t write_avail = interface->buffer_size - interface->valid_size;

      while (write_avail == 0)
      {
         pthread_cond_wait(&interface->cond, &interface->mutex);
         write_avail = interface->buffer_size - interface->valid_size;
      }

      if (write_avail > size)
         write_avail = size;

      size_t write_ptr = (interface->read_ptr + interface->valid_size) & interface->buffer_mask;

      uint8_t *output1 = interface->buffer + write_ptr;
      uint8_t *output2 = interface->buffer;
      size_t write_avail1 = write_avail;
      size_t write_avail2 = 0;

      if (write_ptr + write_avail > interface->buffer_size)
      {
         write_avail1 = interface->buffer_size - write_ptr;
         write_avail2 = write_avail - write_avail1;
      }

      memcpy(output1, buf, write_avail1);
      buf += write_avail1;
      memcpy(output2, buf + write_avail1, write_avail2);
      buf += write_avail2;

      size -= write_avail;
      written += write_avail;
      interface->valid_size += write_avail;

      pthread_mutex_unlock(&interface->mutex);
   }

   return written;
}

static int coreaudio_latency(void *data)
{
   coreaudio_t *interface = data;

   pthread_mutex_lock(&interface->mutex);
   // Not guaranteed to be accurate, since CoreAudio probably buffers a little bit,
   // but don't know how to figure out better values.
   int latency = interface->valid_size;
   pthread_mutex_unlock(&interface->mutex);

   return latency;
}

static void coreaudio_get_backend(void *data, backend_info_t *backend_info)
{
   (void)data;
   backend_info->latency = DEFAULT_CHUNK_SIZE;
   backend_info->chunk_size = DEFAULT_CHUNK_SIZE;
}

const rsd_backend_callback_t rsd_coreaudio = {
   .init = coreaudio_init,
   .write = coreaudio_write,
   .latency = coreaudio_latency,
   .close = coreaudio_close,
   .get_backend_info = coreaudio_get_backend,
   .open = coreaudio_open,
   .backend = "CoreAudio"
};

