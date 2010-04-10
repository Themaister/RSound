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

#include "ao.h"
#include "rsound.h"

static void ao_rsd_close(void *data)
{
   ao_t* sound = data;

   if ( sound->device )
      ao_close(sound->device);
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
   int endian = 0;

   switch ( w->rsd_format )
   {
      case RSD_S16_LE:
         bits = 16;
         endian = AO_FMT_LITTLE;
         break;

      case RSD_U16_LE:
      case RSD_U16_BE:
         return -1;

      case RSD_S16_BE:
         bits = 16;
         endian = AO_FMT_BIG;
         break;
      case RSD_U8:
         bits = 8;
         break;
      case RSD_S8:
         bits = 8;
         break;

      default:
         return -1;
   }


   ao_sample_format format = {
      .bits = 16,
      .channels = w->numChannels,
      .rate = w->sampleRate,
      .byte_format = AO_FMT_LITTLE
   };
   
   int default_driver = ao_default_driver_id();
   if ( default_driver < 0 )
      return -1;
   interface->device = ao_open_live(default_driver, &format, NULL);
   if ( interface->device == NULL )
   {
      fprintf(stderr, "Error opening device.\n");
      return -1;
   }

   return 0;
}

static size_t ao_rsd_write(void *data, const void* buf, size_t size)
{
   ao_t *sound = data;
   if ( ao_play(sound->device, (void*)buf, size) == 0 )
      return -1;
   return size;
}

static void ao_rsd_get_backend(void *data, backend_info_t *backend_info)
{
   backend_info->latency = DEFAULT_CHUNK_SIZE;
   backend_info->chunk_size = DEFAULT_CHUNK_SIZE;
}

const rsd_backend_callback_t rsd_ao = {
   .init = ao_rsd_init,
   .initialize = ao_rsd_initialize,
   .shutdown = ao_rsd_shutdown,
   .write = ao_rsd_write,
   .close = ao_rsd_close,
   .get_backend_info = ao_rsd_get_backend,
   .open = ao_rsd_open,
   .backend = "AO"
};


