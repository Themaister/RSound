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
#include "rsound.h"

static ALCdevice *global_handle;
static ALCcontext *global_context;

#define BUF_SIZE 2048

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
      }
   }
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
   alcMakeContextCurrent(NULL);
   alcDestroyContext(global_context);
   alcCloseDevice(global_handle);
}

static int al_open(void* data, wav_header_t *w)
{
   al_t *al = data;

   // Crude
   if ( w->numChannels == 2 && w->bitsPerSample == 16 )
      al->format = AL_FORMAT_STEREO16;
   else if ( w->numChannels == 1 && w->bitsPerSample == 16 )
      al->format = AL_FORMAT_MONO16;
   else if ( w->numChannels == 2 && w->bitsPerSample == 8 )
      al->format = AL_FORMAT_STEREO8;
   else if ( w->numChannels == 1 && w->bitsPerSample == 8 )
      al->format = AL_FORMAT_MONO8;
   else
      al->format = AL_FORMAT_STEREO16; // Oh what the hell :D

   al->rate = w->sampleRate;

	al->num_buffers = al->rate / 10000 + 1;
   al->buffers = malloc(al->num_buffers * sizeof(int));
   if ( al->buffers == NULL )
      return -1;

   alGenSources(1, &al->source);
   alGenBuffers(al->num_buffers, al->buffers);
   al->queue = 0;
   
   return 0;
}

static size_t al_write(void *data, const void* buf, size_t size)
{

   al_t *al = data;

   // Fills up the buffer before we start playing.
   
   if ( al->queue < al->num_buffers )
   {
      alBufferData(al->buffers[al->queue++], al->format, buf, size, al->rate);
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

   ALuint buffer;
   ALint val;

   // Waits until we have a buffer we can unqueue.

   struct timespec tv = {
      .tv_sec = 0,
      .tv_nsec = 1000000
   };

   // Do we need to unqueue first?
   for(;;)
   {
      alGetSourcei(al->source, AL_BUFFERS_PROCESSED, &val);
      if ( val > 0 )
         break;

      nanosleep(&tv, NULL);
   } 

   alSourceUnqueueBuffers(al->source, 1, &buffer);

   // Buffers up the data
   alBufferData(buffer, al->format, buf, size, al->rate);
   alSourceQueueBuffers(al->source, 1, &buffer);
   if ( alGetError() != AL_NO_ERROR )
      return 0;

   // Checks if we're playing
   alGetSourcei(al->source, AL_SOURCE_STATE, &val);
   if ( val != AL_PLAYING )
      alSourcePlay(al->source);
   
   if ( alGetError() != AL_NO_ERROR )
      return 0;

   return size;
}

static void al_get_backend(void *data, backend_info_t *backend_info)
{
   al_t *al = data;
   backend_info->latency = BUF_SIZE * al->num_buffers;
   backend_info->chunk_size = BUF_SIZE;
}

const rsd_backend_callback_t rsd_al = {
   .init = al_init,
   .initialize = al_initialize,
   .shutdown = al_shutdown,
   .write = al_write,
   .close = al_close,
   .get_backend_info = al_get_backend,
   .open = al_open,
   .backend = "OpenAL"
};


