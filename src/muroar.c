/*  RSound - A PCM audio client/server
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
 *  Copyright (C) 2010 - Philipp Schafft
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

#include "muroar-internal.h"
#include <muroar.h>
#include "rsound.h"

static void muroar_rsd_close(void *data)
{
   muroar_t* sound = data;

   if ( sound->fh != -1 )
      muroar_close(sound->fh);
}

static int muroar_rsd_init(void** data)
{
   muroar_t *sound = calloc(1, sizeof(muroar_t));
   if ( sound == NULL )
      return -1;
   *data = sound;
   return 0;
}

static int muroar_rsd_open(void* data, wav_header_t *w)
{
   muroar_t* interface = data;

   int bits  = -1;
   int codec = -1;

   switch ( w->rsd_format )
   {
      case RSD_S16_LE:
         bits  = 16;
         codec = MUROAR_CODEC_PCM_S_LE;
         break;
      case RSD_U16_LE:
         bits  = 16;
         codec = MUROAR_CODEC_PCM_U_LE;
         break;
      case RSD_U16_BE:
         bits  = 16;
         codec = MUROAR_CODEC_PCM_U_BE;
         break;
      case RSD_S16_BE:
         bits  = 16;
         codec = MUROAR_CODEC_PCM_S_BE;
         break;
      case RSD_U8:
         bits  =  8;
         codec = MUROAR_CODEC_PCM_U;
         break;
      case RSD_S8:
         bits  =  8;
         codec = MUROAR_CODEC_PCM_S;
         break;
      case RSD_ALAW:
         bits  =  8;
         codec =  MUROAR_CODEC_ALAW;
         break;
      case RSD_MULAW:
         bits  =  8;
         codec =  MUROAR_CODEC_MULAW;
         break;

      default:
         return -1;
   }

   if ( (interface->fh = muroar_connect(NULL, "rsd")) == -1 )
   {
      fprintf(stderr, "Error opening device.\n");
      return -1;
   }

   if ( muroar_stream(interface->fh, MUROAR_PLAY_WAVE, NULL, codec, w->sampleRate, w->numChannels, bits) == -1 )
   {
      fprintf(stderr, "Error opening device.\n");
      return -1;
   }

   return 0;
}

static size_t muroar_rsd_write(void *data, const void* buf, size_t size)
{
   muroar_t *sound = data;
   if ( muroar_write(sound->fh, (void*)buf, size) != (ssize_t)size )
      return 0;
   return size;
}

// We can't measure this accurately, but hey.
static int muroar_rsd_latency(void* data)
{
   (void)data;
   return DEFAULT_CHUNK_SIZE;
}

static void muroar_rsd_get_backend(void *data, backend_info_t *backend_info)
{
   (void)data;
   backend_info->latency = DEFAULT_CHUNK_SIZE;
   backend_info->chunk_size = DEFAULT_CHUNK_SIZE;
}

const rsd_backend_callback_t rsd_muroar = {
   .init = muroar_rsd_init,
   .write = muroar_rsd_write,
   .latency = muroar_rsd_latency,
   .close = muroar_rsd_close,
   .get_backend_info = muroar_rsd_get_backend,
   .open = muroar_rsd_open,
   .backend = "muRoar"
};


