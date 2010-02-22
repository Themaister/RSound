/*  RSound - A PCM audio client/server
 *  Copyright (C) 2009 - Hans-Kristian Arntzen
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

#include "alsa.h"
#include "rsound.h"


static void clean_alsa_interface(void* data)
{
   alsa_t *sound = data;

   close(sound->socket);
   if ( sound->handle )
   {
      snd_pcm_drop(sound->handle);
      snd_pcm_close(sound->handle);
   }
   if ( sound->buffer )
      free(sound->buffer);
}

/* ALSA is just wonderful, isn't it? ... */
static int init_alsa(alsa_t* interface, wav_header* w)
{
   uint32_t chunk_size = 0;
   uint32_t buffer_size = 0;
   
   int rc;
   unsigned int buffer_time = BUFFER_TIME;
   snd_pcm_uframes_t frames = 256;
   snd_pcm_uframes_t bufferSize;
   interface->buffer = NULL;
   /* Prefer a small frame count for this, with a high buffer/framesize. */

   rc = snd_pcm_open(&interface->handle, device, SND_PCM_STREAM_PLAYBACK, 0);

   if ( rc < 0 )
   {
      fprintf(stderr, "Unable to open PCM device: %s\n", snd_strerror(rc));
      return 0;
   }

   snd_pcm_hw_params_malloc(&interface->params);
   if ( snd_pcm_hw_params_any(interface->handle, interface->params) < 0 ) return 0;
   if ( snd_pcm_hw_params_set_access(interface->handle, interface->params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ) return 0;
   if ( snd_pcm_hw_params_set_format(interface->handle, interface->params, SND_PCM_FORMAT_S16_LE) < 0) return 0;
   if ( snd_pcm_hw_params_set_channels(interface->handle, interface->params, w->numChannels) < 0 ) return 0;
   if ( snd_pcm_hw_params_set_rate_near(interface->handle, interface->params, &w->sampleRate, NULL) < 0 ) return 0;
   
   if ( snd_pcm_hw_params_set_buffer_time_near(interface->handle, interface->params, &buffer_time, NULL) < 0 ) return 0; 
   if ( snd_pcm_hw_params_set_period_size_near(interface->handle, interface->params, &frames, NULL) < 0 ) return 0;

   rc = snd_pcm_hw_params(interface->handle, interface->params);
   if (rc < 0) 
   {
      fprintf(stderr,
            "unable to set hw parameters: %s\n",
            snd_strerror(rc));
      return 0;
   }

   snd_pcm_hw_params_get_period_size(interface->params, &interface->frames,
         NULL);
   interface->size = (int)interface->frames * w->numChannels * 2;
   interface->frames = (int)interface->frames;
   snd_pcm_hw_params_get_buffer_size(interface->params, &bufferSize);

   chunk_size = (uint32_t)interface->size;
   buffer_size = (uint32_t)bufferSize * w->numChannels * 2;

   snd_pcm_hw_params_free(interface->params);
   
   if ( verbose )
      fprintf(stderr, "Buffer size: %u, Fragment size: %u.\n", buffer_size, chunk_size);

   interface->buffer = malloc(interface->size);
   if ( interface->buffer == NULL )
   {
      fprintf(stderr, "Error allocation memory for buffer.\n");
      return 0;
   }
   return 1;
}

void* alsa_thread ( void* data )
{
   alsa_t sound;

   cleanup_callback(clean_alsa_interface, &sound);

   wav_header w;
   int rc;
   int active_connection;
   int underrun_count = 0;

   int s_new = *((int*)data);
   free(data);
   sound.socket = s_new;

   if ( verbose )
      fprintf(stderr, "Connection accepted, awaiting WAV header data...\n");

   rc = get_wav_header(s_new, &w);

   if ( rc != 1 )
   {
      close(s_new);
      fprintf(stderr, "Couldn't read WAV header... Disconnecting.\n");
      pthread_exit(NULL);
   }

   if ( verbose )
   {
      fprintf(stderr, "Successfully got WAV header ...\n");
      pheader(&w);
   }  

   if ( verbose )
      fprintf(stderr, "Initializing ALSA ...\n");

   if ( !init_alsa(&sound, &w) )
   {
      fprintf(stderr, "Failed to initialize ALSA ...\n");
      goto alsa_exit;

   }

   if ( !send_backend_info(s_new, sound.size, (uint32_t)pthread_self()) )
   {
      fprintf(stderr, "Failed to send buffer info ...\n");
      goto alsa_exit;
   }

   if ( verbose )
      fprintf(stderr, "Initializing of ALSA successful... Party time!\n");


   active_connection = 1;
   while(active_connection)
   {
      memset(sound.buffer, 0, sound.size);

      /* Reads complete buffer */
      rc = recieve_data(s_new, sound.buffer, sound.size);
      if ( rc == 0 )
      {
         active_connection = 0;
         break;
      }

      /* Plays it back :D */
      rc = snd_pcm_writei(sound.handle, sound.buffer, sound.frames);
      if (rc == -EPIPE) 
      {
         fprintf(stderr, "Underrun occurred. Count: %d\n", ++underrun_count);
         snd_pcm_prepare(sound.handle);
      } 
      else if (rc < 0) 
      {
         fprintf(stderr,
               "Error from writei: %s\n",
               snd_strerror(rc));
      }  
      else if (rc != (int)sound.frames )
      {
         fprintf(stderr,
               "Short write, write %d frames\n", rc);
      }

   }

   if ( verbose )
      fprintf(stderr, "Closed connection. The friendly PCM-service welcomes you back.\n\n\n");

alsa_exit:
   pthread_exit(NULL);
   return NULL; /* GCC warning */

}
