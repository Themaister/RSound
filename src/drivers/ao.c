/*  RSound - A PCM audio client/server
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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

#include "ao.h"
#include "../rsound.h"

static void ao_rsd_close(void *data)
{
   ao_t* sound = data;

   if ( sound->device )
      ao_close(sound->device);
   free(sound);
}

static void ao_rsd_initialize(void)
{
   ao_initialize();
}

static void ao_rsd_shutdown(void)
{
   ao_shutdown();
}

static int ao_rsd_init(void** data)
{
   ao_t *sound = calloc(1, sizeof(ao_t));
   if ( sound == NULL )
      return -1;
   *data = sound;
   return 0;
}

static int ao_rsd_open(void* data, wav_header_t *w)
{
   ao_t* interface = data;

   int bits = 0;
   int endian = AO_FMT_NATIVE;

   interface->converter = RSD_NULL;
   interface->fmt = w->rsd_format;

   if (rsnd_format_to_bytes(w->rsd_format) == 4) 
   {
      interface->converter = converter_fmt_to_s32ne(w->rsd_format);
      bits = 32;
   }
   else
   {
      interface->converter = converter_fmt_to_s16ne(w->rsd_format);
      bits = 16;
   }

   ao_sample_format format = {
      .bits = bits,
      .channels = w->numChannels,
      .rate = w->sampleRate,
      .byte_format = endian
   };

   int default_driver = ao_default_driver_id();
   if ( default_driver < 0 )
      return -1;
   interface->device = ao_open_live(default_driver, &format, NULL);
   if ( interface->device == NULL )
   {
      log_printf("Error opening device.\n");
      return -1;
   }

   return 0;
}

static size_t ao_rsd_write(void *data, const void* inbuf, size_t size)
{
   ao_t *sound = data;

   size_t osize = size;
   
   uint8_t convbuf[2 * size];
   void *buffer = (void*)inbuf;

   if (sound->converter != RSD_NULL)
   {
      osize = (rsnd_format_to_bytes(sound->fmt) == 1) ? 2*size : size;

      memcpy(convbuf, inbuf, size);

      audio_converter(convbuf, sound->fmt, sound->converter, size);
      buffer = convbuf;
   }

   if ( ao_play(sound->device, buffer, osize) == 0 )
      return 0;
   return size;
}

// We can't measure this accurately, but hey.
static int ao_rsd_latency(void* data)
{
   (void)data;
   return DEFAULT_CHUNK_SIZE;
}

static void ao_rsd_get_backend(void *data, backend_info_t *backend_info)
{
   (void)data;
   backend_info->latency = DEFAULT_CHUNK_SIZE;
   backend_info->chunk_size = DEFAULT_CHUNK_SIZE;
}

const rsd_backend_callback_t rsd_ao = {
   .init = ao_rsd_init,
   .initialize = ao_rsd_initialize,
   .shutdown = ao_rsd_shutdown,
   .write = ao_rsd_write,
   .latency = ao_rsd_latency,
   .close = ao_rsd_close,
   .get_backend_info = ao_rsd_get_backend,
   .open = ao_rsd_open,
   .backend = "AO"
};


