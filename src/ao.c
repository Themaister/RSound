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
   int default_driver = ao_default_driver_id();
   ao_t *sound = calloc(1, sizeof(ao_t));
   sound->default_driver = default_driver;
   if ( sound->default_driver <= 0 )
      return -1;
   *data = sound;
   return 0;
}

static int ao_rsd_set_param(void* data, wav_header_t *w)
{
   ao_t* interface = data;

   interface->format.bits = 16;
   interface->format.channels = w->numChannels;
   interface->format.rate = w->sampleRate;
   interface->format.byte_format = AO_FMT_LITTLE;

   interface->device = ao_open_live(interface->default_driver, &interface->format, NULL);
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
   return ao_play(sound->device, (void*)buf, size);
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
   .set_params = ao_rsd_set_param
};


