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

static void al_close(void *data)
{
   al_t* sound = data;
   if ( data )
   {
      if ( sound->context )
      {
         alcDestroyContext(sound->context);
         sound->context = 0;
      }

      if ( sound->handle )
      {
         alcCloseDevice(sound->handle);
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

static int al_open(void* data, wav_header_t *w)
{
   al_t *al = data;

   // Crude
   if ( w->numChannels == 2 )
      al->format = AL_FORMAT_STEREO16;
   else
      al->format = AL_FORMAT_MONO16;

   al->rate = w->sampleRate;

   al->handle = alcOpenDevice(NULL);
   if ( al->handle == NULL )
      return -1;

   al->context = alcCreateContext(al->handle, NULL);
   if ( al->context == NULL )
      return -1;

   alcMakeContextCurrent(al->context);

   alGenSources(1, &al->source);
   
   return 0;
}

static size_t al_write(void *data, const void* buf, size_t size)
{
   al_t *al = data;

   
   ALuint buffer = 0;

   alGenBuffers(1, &buffer);

   alBufferData(buffer, al->format, buf, size, al->rate);
   alSourcei(al->source, AL_BUFFER, buffer);

   ALint playing;
   alGetSourcei(al->source, AL_SOURCE_STATE, &playing);
   if ( playing != AL_PLAYING )
      alSourcePlay(al->source);

   return size;

}

static void al_get_backend(void *data, backend_info_t *backend_info)
{
   backend_info->latency = DEFAULT_CHUNK_SIZE;
   backend_info->chunk_size = DEFAULT_CHUNK_SIZE;
}

const rsd_backend_callback_t rsd_al = {
   .init = al_init,
   .write = al_write,
   .close = al_close,
   .get_backend_info = al_get_backend,
   .open = al_open,
   .backend = "OpenAL"
};


