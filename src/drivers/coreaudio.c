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
#include <stdint.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

static void coreaudio_close(void *data)
{
   coreaudio_t *sound = data;
   OSStatus status;
   UInt32 sizeof_running, running;

   if (!sound)
      return;

   pthread_mutex_lock(&sound->mutex);
   if (sound->unit_allocated)
   {
      sound->unit_allocated = 0;
      sound->stopping = 1;

      if (!sound->started && sound->valid_byte_count)
      {
         status = AudioOutputUnitStart(sound->audio_unit);
         if (status)
            goto exit_unlock;
         sound->started = 1;
      }

      sizeof_running = sizeof(UInt32);
      AudioUnitGetProperty(sound->audio_unit,
            kAudioDevicePropertyDeviceIsRunning,
            kAudioUnitScope_Input,
            0,
            &running,
            &sizeof_running);

      if (!running) goto exit_unlock;

      if (sound->started)
      {
         while (sound->valid_byte_count)
            pthread_cond_wait(&sound->cond, &sound->mutex);

         pthread_mutex_unlock(&sound->mutex);

         status = AudioOutputUnitStop(sound->audio_unit);
         if (status)
            goto exit;

         CloseComponent(sound->audio_unit);
         goto exit;
      }
   }

   if (sound->buffer)
      free(sound->buffer);

exit_unlock:
   pthread_mutex_unlock(&sound->mutex);
exit:
   pthread_mutex_destroy(&sound->mutex);
   pthread_cond_destroy(&sound->cond);
   free(sound);
}

static OSStatus audio_callback(void *inRefCon, AudioUnitRenderActionFlags *inActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
   coreaudio_t* interface = inRefCon;
   unsigned int validByteCount;
   unsigned int totalBytesToCopy;

   (void)inTimeStamp;
   (void)inBusNumber;
   (void)inNumberFrames;

   if (!ioData)
      return 0;

   if (ioData->mNumberBuffers != 1)
      return 0;

   totalBytesToCopy = ioData->mBuffers[0].mDataByteSize;

   pthread_mutex_lock(&interface->mutex);

   validByteCount = interface->valid_byte_count;

   if (validByteCount < totalBytesToCopy && !interface->stopping)
   {
      *inActionFlags = kAudioUnitRenderAction_OutputIsSilence;
      memset(ioData->mBuffers[0].mData, 0, ioData->mBuffers[0].mDataByteSize);
      pthread_mutex_unlock(&interface->mutex);
      return 0;
   }

   uint8_t *outBuffer = ioData->mBuffers[0].mData;
   unsigned outBufSize = ioData->mBuffers[0].mDataByteSize;
   unsigned bytesToCopy = MIN(outBufSize, validByteCount);
   unsigned firstFrag = bytesToCopy;
   uint8_t *sample = (uint8_t*)interface->buffer + interface->valid_byte_offset;

   if (interface->valid_byte_offset + bytesToCopy > interface->buffer_byte_count)
      firstFrag = interface->buffer_byte_count - interface->valid_byte_offset;

   if (firstFrag < bytesToCopy)
   {
      memcpy(outBuffer, sample, firstFrag);
      memcpy(outBuffer + firstFrag, interface->buffer, bytesToCopy - firstFrag);
   }
   else
      memcpy(outBuffer, sample, bytesToCopy);

   if (bytesToCopy < outBufSize)
      memset(outBuffer + bytesToCopy, 0, outBufSize - bytesToCopy);

   interface->valid_byte_count -= bytesToCopy;
   interface->valid_byte_offset = (interface->valid_byte_offset + bytesToCopy) % interface->buffer_byte_count;

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
   coreaudio_t* interface = data;
   OSStatus result = noErr;
   Component comp;
   ComponentDescription desc;
   AudioStreamBasicDescription requested_desc, actual_desc;
   AudioChannelLayout layout;
   AURenderCallbackStruct input;
   UInt32 i_param_size;

   desc.componentType = kAudioUnitType_Output;
   desc.componentSubType = kAudioUnitSubType_HALOutput;
   desc.componentManufacturer = kAudioUnitManufacturer_Apple;
   desc.componentFlags = 0;
   desc.componentFlagsMask = 0;

   comp = FindNextComponent(NULL, &desc);
   if (comp == NULL)
      return -1;

   result = OpenAComponent(comp, &interface->audio_unit);
   if (result)
      return -1;

   interface->unit_allocated = 1;

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

   result = AudioUnitSetProperty(interface->audio_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &requested_desc, sizeof(requested_desc));
   if (result)
      return -1;

   i_param_size = sizeof(actual_desc);
   result = AudioUnitGetProperty(interface->audio_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &actual_desc, &i_param_size);
   if (result)
      return -1;

   if (fabs(requested_desc.mSampleRate - actual_desc.mSampleRate) > requested_desc.mSampleRate * .05)
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

   memset(&layout, 0, sizeof(layout));
   layout.mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelBitmap;
   layout.mChannelBitmap = (1 << w->numChannels) - 1;

   result = AudioUnitSetProperty(interface->audio_unit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Input, 0, &layout, sizeof(layout));
   if (result)
      return -1;

   input.inputProc = (AURenderCallback) audio_callback;
   input.inputProcRefCon = interface;

   result = AudioUnitSetProperty(interface->audio_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &input, sizeof(input));
   if (result)
      return -1;

   result = AudioUnitInitialize(interface->audio_unit);
   if (result)
      return -1;

   interface->buffer_byte_count = DEFAULT_CHUNK_SIZE * 4 * 8;
   interface->valid_byte_offset = 0;
   interface->valid_byte_count = 0;
   interface->buffer = malloc(interface->buffer_byte_count);
   if (!interface->buffer)
      return -1;

   memset(interface->buffer, 0, interface->buffer_byte_count);

   return 0;
}

static size_t coreaudio_write(void *data, const void* buf, size_t size)
{
   coreaudio_t *sound = data;
   int err;
   size_t bytes_written = 0;
   unsigned int bytes_to_copy;
   unsigned int first_empty_byte_offset, empty_byte_count;

   while (size)
   {
      pthread_mutex_lock(&sound->mutex);

      empty_byte_count = sound->buffer_byte_count - sound->valid_byte_count;
      while (empty_byte_count == 0)
      {
         if (!sound->started)
         {
            err = AudioOutputUnitStart(sound->audio_unit);
            if (err)
               return 0;
            sound->started = 1;
         }

         pthread_cond_wait(&sound->cond, &sound->mutex);
         empty_byte_count = sound->buffer_byte_count - sound->valid_byte_count;
      }

      first_empty_byte_offset = (sound->valid_byte_offset + sound->valid_byte_count) % sound->buffer_byte_count;
      if (first_empty_byte_offset + empty_byte_count > sound->buffer_byte_count)
         bytes_to_copy = MIN(size, sound->buffer_byte_count - first_empty_byte_offset);
      else
         bytes_to_copy = MIN(size, empty_byte_count);

      memcpy((uint8_t*)sound->buffer + first_empty_byte_offset, buf, bytes_to_copy);

      size -= bytes_to_copy;
      bytes_written += bytes_to_copy;
      buf = (uint8_t*)buf + bytes_to_copy;
      sound->valid_byte_count += bytes_to_copy;

      pthread_mutex_unlock(&sound->mutex);
   }

   return bytes_written;
}

static int coreaudio_latency(void* data)
{
   int latency;
   coreaudio_t *sound = data;

   pthread_mutex_lock(&sound->mutex);
   latency = sound->valid_byte_count;
   pthread_mutex_unlock(&sound->mutex);

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

