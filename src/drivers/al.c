/*  RSound - A PCM audio client/server
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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

#include "al.h"
#include "../rsound.h"

#ifdef _WIN32
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static ALCdevice *global_handle;
static ALCcontext *global_context;

#define BUF_SIZE 512

static void al_close(void *data)
{
   al_t *al= data;
   if ( al )
   {
      alSourceStop(al->source);
      alDeleteSources(1, &al->source);
      if ( al->buffers )
      {
         alDeleteBuffers(al->num_buffers, al->buffers);
         free(al->buffers);
         free(al->res_buf);
      }
   }
   free(al);
}

static int al_init(void** data)
{
   al_t *sound = calloc(1, sizeof(al_t));
   if ( sound == NULL )
      return -1;

   *data = sound;
   return 0;
}

static void al_initialize(void)
{
   global_handle = alcOpenDevice(NULL);
   if ( global_handle == NULL )
      exit(1);

   global_context = alcCreateContext(global_handle, NULL);
   if ( global_context == NULL )
      exit(1);

   alcMakeContextCurrent(global_context);
}

static void al_shutdown(void)
{
   // For some reason, cleaning up in Win32 makes the application crash. >_<
#ifndef _WIN32
   alcMakeContextCurrent(NULL);
   alcDestroyContext(global_context);
   alcCloseDevice(global_handle);
#endif
}

static int al_open(void* data, wav_header_t *w)
{
   al_t *al = data;


   al->fmt = w->rsd_format;
   al->conv = RSD_NULL;

   int bits = rsnd_format_to_bytes(w->rsd_format) * 8;
   int i = 0;

   if ( is_little_endian() )
      i++;
   if ( w->rsd_format & ( RSD_S16_BE | RSD_U16_BE ) )
      i++;

   if ( (i % 2) == 0 && bits == 16 )
      al->conv |= RSD_SWAP_ENDIAN;

   if ( w->rsd_format & ( RSD_U16_LE | RSD_U16_BE ) )
      al->conv |= RSD_U_TO_S;
   else if ( w->rsd_format & RSD_S8 )
      al->conv |= RSD_S_TO_U;

   if ( w->numChannels == 2 && w->bitsPerSample == 16 )
      al->format = AL_FORMAT_STEREO16;
   else if ( w->numChannels == 1 && w->bitsPerSample == 16 )
      al->format = AL_FORMAT_MONO16;
   else if ( w->numChannels == 2 && w->bitsPerSample == 8 )
      al->format = AL_FORMAT_STEREO8;
   else if ( w->numChannels == 1 && w->bitsPerSample == 8 )
      al->format = AL_FORMAT_MONO8;

   if ( w->rsd_format == RSD_ALAW )
   {
      al->format = (w->numChannels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
      al->conv |= RSD_ALAW_TO_S16;
   }
   else if ( w->rsd_format == RSD_MULAW )
   {
      al->format = (w->numChannels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
      al->conv |= RSD_MULAW_TO_S16;
   }


   al->rate = w->sampleRate;

   al->num_buffers = al->rate / 2500 + 1;
   al->buffers = malloc(al->num_buffers * sizeof(ALuint));
   al->res_buf = malloc(al->num_buffers * sizeof(ALuint));
   if ( al->buffers == NULL || al->res_buf == NULL )
      return -1;

   alGenSources(1, &al->source);
   alGenBuffers(al->num_buffers, al->buffers);
   al->queue = 0;
   al->res_ptr = 0;

   return 0;
}

static int al_unqueue_buffers(al_t *al)
{
   ALint val;

   alGetSourcei(al->source, AL_BUFFERS_PROCESSED, &val);

   if ( val > 0 )
   {
      alSourceUnqueueBuffers(al->source, val, &al->res_buf[al->res_ptr]);
      al->res_ptr += val;
      return val;
   }

   return 0;
}

static ALuint al_get_buffer(al_t *al)
{
   // Checks if we need to block to get vacant buffer.
   ALuint buffer;

   if ( al->res_ptr == 0 )
   {
#ifndef _WIN32
      struct timespec tv = {
         .tv_sec = 0,
         .tv_nsec = 1000000
      };
#endif

      for(;;)
      {
         if ( al_unqueue_buffers(al) > 0 )
            break;
#ifdef _WIN32
         Sleep(1);
#else
         nanosleep(&tv, NULL);
#endif
      }
   }

   buffer = al->res_buf[--al->res_ptr];

   return buffer;
}

static size_t al_write(void *data, const void* inbuf, size_t size)
{

   al_t *al = data;

   size_t osize = size;

   uint8_t convbuf[2*size];
   void *buffer_ptr = (void*)inbuf;

   if (al->conv != RSD_NULL)
   {
      osize = (al->fmt & (RSD_ALAW | RSD_MULAW)) ? 2*size : size;

      memcpy(convbuf, inbuf, size);

      audio_converter(convbuf, al->fmt, al->conv, size);
      buffer_ptr = convbuf;
   }


   // Fills up the buffer before we start playing.


   if ( al->queue < al->num_buffers )
   {
      alBufferData(al->buffers[al->queue++], al->format, buffer_ptr, osize, al->rate);
      if ( alGetError() != AL_NO_ERROR )
      {
         return 0;
      }

      if ( al->queue == al->num_buffers )
      {
         alSourceQueueBuffers(al->source, al->num_buffers, al->buffers);
         alSourcePlay(al->source);
         if ( alGetError() != AL_NO_ERROR )
         {
            return 0;
         }
      }

      return size;
   }

   ALuint buffer = al_get_buffer(al);

   // Buffers up the data
   alBufferData(buffer, al->format, buffer_ptr, osize, al->rate);
   alSourceQueueBuffers(al->source, 1, &buffer);
   if ( alGetError() != AL_NO_ERROR )
      return 0;

   // Checks if we're playing
   ALint val;
   alGetSourcei(al->source, AL_SOURCE_STATE, &val);
   if ( val != AL_PLAYING )
      alSourcePlay(al->source);

   if ( alGetError() != AL_NO_ERROR )
      return 0;

   return size;
}

static void al_get_backend(void *data, backend_info_t *backend_info)
{
   (void) data;
   backend_info->latency = BUF_SIZE;
   backend_info->chunk_size = BUF_SIZE;
}

static int al_latency(void *data)
{
   al_t *al = data;

   int latency;
   if ( al->queue < al->num_buffers )
   {
      latency = al->queue * BUF_SIZE;
   }
   else
   {
      al_unqueue_buffers(al);
      latency = BUF_SIZE * (al->num_buffers - al->res_ptr);
   }
   return latency;
}

const rsd_backend_callback_t rsd_al = {
   .init = al_init,
   .initialize = al_initialize,
   .shutdown = al_shutdown,
   .write = al_write,
   .latency = al_latency,
   .close = al_close,
   .get_backend_info = al_get_backend,
   .open = al_open,
   .backend = "OpenAL"
};


