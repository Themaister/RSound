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

#define BUF_SIZE 1024

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

   al->conv = converter_fmt_to_s16ne(w->rsd_format);

   // Don't support multichannels yet.
   if (w->numChannels > 2)
      return -1;

   al->format = (w->numChannels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

   al->rate = w->sampleRate;

   al->num_buffers = al->rate / 2500 + 1;
   al->buffers = malloc(al->num_buffers * sizeof(ALuint));
   al->res_buf = malloc(al->num_buffers * sizeof(ALuint));
   if ( al->buffers == NULL || al->res_buf == NULL )
      return -1;

   alGenSources(1, &al->source);
   alGenBuffers(al->num_buffers, al->buffers);
   memcpy(al->res_buf, al->buffers, al->num_buffers * sizeof(ALuint));
   al->res_ptr = al->num_buffers;

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
      if (rsnd_format_to_bytes(al->fmt) == 1)
         osize = 2 * size;
      else if (rsnd_format_to_bytes(al->fmt) == 4)
         osize = size / 2;

      memcpy(convbuf, inbuf, size);

      audio_converter(convbuf, al->fmt, al->conv, size);
      buffer_ptr = convbuf;
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
   al_unqueue_buffers(al);
   latency = BUF_SIZE * (al->num_buffers - al->res_ptr);

   if (rsnd_format_to_bytes(al->fmt) == 4)
      latency /= 2;
   else if (al->fmt & (RSD_ALAW | RSD_MULAW))
      latency *= 2;

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


