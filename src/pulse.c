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

#include "pulse.h"
#include "rsound.h"

static void pulse_close(void *data)
{
   pulse_t* sound = data;

   if ( sound != NULL && sound->s != NULL )
   {
      pa_simple_flush(sound->s, NULL);
      pa_simple_free(sound->s);
   }
}

static int pulse_init(void** data)
{
   pulse_t *sound = calloc(1, sizeof(pulse_t));
   if ( sound == NULL )
      return -1;
   *data = sound;
   return 0;
}

static int pulse_open(void* data, wav_header_t *w)
{
   pulse_t* interface = data;

   pa_sample_spec ss;

   ss.channels = w->numChannels;
   ss.rate = w->sampleRate;

   switch ( w->rsd_format )
   {
      case RSD_S16_LE:
         ss.format = PA_SAMPLE_S16LE;
         interface->framesize = 2;
         break;

      case RSD_U16_LE:
      case RSD_U16_BE:
         return -1;

      case RSD_S16_BE:
         ss.format = PA_SAMPLE_S16BE;
         interface->framesize = 2;
         break;

      case RSD_U8:
         ss.format = PA_SAMPLE_U8;
         interface->framesize = 1;
         break;

      case RSD_S8:
         return -1;

      default:
         return -1;
   }

   interface->framesize *= w->numChannels;
   interface->rate = w->sampleRate;

   interface->s = pa_simple_new( NULL, "RSD", PA_STREAM_PLAYBACK, NULL, 
                                 "RSound stream", &ss,
                                 NULL, NULL, NULL);

   if ( interface->s == NULL )
      return -1;

   return 0;
}

static size_t pulse_write(void *data, const void* buf, size_t size)
{
   pulse_t *sound = data;

   if ( pa_simple_write(sound->s, buf, size, NULL) < 0 )
      return -1;

   return size;
}

static int pulse_latency(void* data)
{
   pulse_t *sound = data;

   return (pa_simple_get_latency(sound->s, NULL) * sound->rate * sound->framesize)/1000000;
}

static void pulse_get_backend(void *data, backend_info_t *backend_info)
{
   (void)data;
   backend_info->latency = DEFAULT_CHUNK_SIZE;
   backend_info->chunk_size = DEFAULT_CHUNK_SIZE;
}

const rsd_backend_callback_t rsd_pulse = {
   .init = pulse_init,
   .write = pulse_write,
   .latency = pulse_latency,
   .close = pulse_close,
   .get_backend_info = pulse_get_backend,
   .open = pulse_open,
   .backend = "PulseAudio"
};


