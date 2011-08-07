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

#include "alsa.h"
#include "../rsound.h"

#define LATENCY_BUFFERS 1

static void alsa_close(void* data)
{
   alsa_t *sound = data;

   if ( sound != NULL && sound->handle != NULL )
   {
      snd_pcm_hw_params_free(sound->params);
      snd_pcm_drop(sound->handle);
      snd_pcm_close(sound->handle);
   }
   free(sound);
}

static int alsa_init(void **data)
{
   alsa_t *alsa = calloc(1, sizeof(alsa_t));
   if ( alsa == NULL )
      return -1;
   *data = alsa;

   return 0;
}

static int alsa_open(void *data, wav_header_t *w)
{
   alsa_t *interface = data;

   int rc = snd_pcm_open(&interface->handle, device, SND_PCM_STREAM_PLAYBACK, 0);
   if ( rc < 0 )
   {
      log_printf("Unable to open PCM device: %s\n", snd_strerror(rc));
      return -1;
   }

   /* Prefer a small frame count for this, with a high buffer/framesize ratio. */
   unsigned int buffer_time = BUFFER_TIME;
   snd_pcm_uframes_t frames = 128;

   /* Determines format to use */   
   snd_pcm_format_t format;
   switch ( w->rsd_format )
   {
      case RSD_S32_LE:
         format = SND_PCM_FORMAT_S32_LE;
         break;
      case RSD_S32_BE:
         format = SND_PCM_FORMAT_S32_BE;
         break;
      case RSD_U32_LE:
         format = SND_PCM_FORMAT_U32_LE;
         break;
      case RSD_U32_BE:
         format = SND_PCM_FORMAT_U32_BE;
         break;
      case RSD_S16_LE:
         format = SND_PCM_FORMAT_S16_LE;
         break;
      case RSD_U16_LE:
         format = SND_PCM_FORMAT_U16_LE;
         break;
      case RSD_S16_BE:
         format = SND_PCM_FORMAT_S16_BE;
         break;
      case RSD_U16_BE:
         format = SND_PCM_FORMAT_U16_BE;
         break;
      case RSD_U8:
         format = SND_PCM_FORMAT_U8;
         break;
      case RSD_S8:
         format = SND_PCM_FORMAT_S8;
         break;
      case RSD_ALAW:
         format = SND_PCM_FORMAT_A_LAW;
         break;
      case RSD_MULAW:
         format = SND_PCM_FORMAT_MU_LAW;
         break;

      default:
         return -1;
   }

   if ( snd_pcm_hw_params_malloc(&interface->params) < 0)
      return -1;

   unsigned int rate = w->sampleRate;
   unsigned int channels = w->numChannels;

   if ( snd_pcm_hw_params_any(interface->handle, interface->params) < 0 ) return -1;
   if ( snd_pcm_hw_params_set_access(interface->handle, interface->params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ) return -1;
   if ( snd_pcm_hw_params_set_format(interface->handle, interface->params, format) < 0) return -1;
   if ( snd_pcm_hw_params_set_channels(interface->handle, interface->params, channels) < 0 ) return -1;
   if ( snd_pcm_hw_params_set_rate(interface->handle, interface->params, rate, 0) < 0 ) return -1;
   if ( snd_pcm_hw_params_set_buffer_time_near(interface->handle, interface->params, &buffer_time, NULL) < 0 ) return -1; 
   if ( snd_pcm_hw_params_set_period_size_near(interface->handle, interface->params, &frames, NULL) < 0 ) return -1;

   rc = snd_pcm_hw_params(interface->handle, interface->params);
   if (rc < 0) 
   {
      log_printf("unable to set hw parameters: %s\n", snd_strerror(rc));
      return -1;
   }

   if (debug)
   {
      snd_pcm_uframes_t latency;
      snd_pcm_uframes_t buffer_size;
      snd_pcm_hw_params_get_period_size(interface->params, &latency, NULL);
      snd_pcm_hw_params_get_buffer_size(interface->params, &buffer_size);

      log_printf("ALSA: Period size: %d frames. Buffer size: %d frames.\n", (int)latency, (int)buffer_size);
   }

   return 0;
}

static void alsa_get_backend (void *data, backend_info_t* backend_info)
{
   alsa_t *sound = data;
   snd_pcm_uframes_t latency;
   snd_pcm_hw_params_get_period_size(sound->params, &latency,
         NULL);

   backend_info->latency = snd_pcm_frames_to_bytes(sound->handle, latency);
   backend_info->chunk_size = snd_pcm_frames_to_bytes(sound->handle, latency);
}

static int alsa_latency(void *data)
{
   alsa_t *sound = data;

   snd_pcm_sframes_t delay_alsa;
   if ( snd_pcm_delay(sound->handle, &delay_alsa) < 0 )
      return DEFAULT_CHUNK_SIZE; // We should at least return something.

   int delay = snd_pcm_frames_to_bytes(sound->handle, delay_alsa);

   return delay;
}

static size_t alsa_write (void *data, const void* buf, size_t size)
{
   alsa_t *sound = data;
   snd_pcm_sframes_t rc;
   snd_pcm_sframes_t write_size = snd_pcm_bytes_to_frames(sound->handle, size );

   rc = snd_pcm_writei(sound->handle, buf, write_size);
   if (rc == -EPIPE || rc == -EINTR || rc == -ESTRPIPE ) 
   {
      if ( snd_pcm_recover(sound->handle, rc, 1) < 0 )
         return 0;
      return size;
   }

   else if (rc < 0) 
   {
      log_printf("Error from writei: %s\n", snd_strerror(rc));
      return 0;
   }  

   return snd_pcm_frames_to_bytes(sound->handle, rc);
}

const rsd_backend_callback_t rsd_alsa = {
   .init = alsa_init,
   .open = alsa_open,
   .write = alsa_write,
   .latency = alsa_latency,
   .get_backend_info = alsa_get_backend,
   .close = alsa_close,
   .backend = "ALSA"
};
