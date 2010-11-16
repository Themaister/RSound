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

#include "roarvs.h"

static void roarvs_rsd_close(void *data)
{
   roar_rsd_t *roar = data;
   if ( roar->sound != NULL )
      roar_vs_close(roar->sound, ROAR_VS_TRUE, NULL);
   free(roar);
}

static int roarvs_rsd_init(void** data)
{
   roar_rsd_t *roar = calloc(1, sizeof(*roar));
   if (roar == NULL)
   {
      *data = NULL;
      return -1;
   }
   int error;

   roar->sound = roar_vs_new(NULL, "rsd", &error);

   if ( roar->sound == NULL ) 
   {
      log_printf("Error opening device: %s\n", roar_vs_strerr(error));
      return -1;
   }

   *data = roar;
   return 0;
}

static int roarvs_rsd_open(void* data, wav_header_t *w)
{
   roar_rsd_t* roar = data;
   struct roar_audio_info info = {
      .bits     = -1,
      .codec    = -1,
      .rate     = w->sampleRate,
      .channels = w->numChannels
   };
   int error;

   switch ( w->rsd_format )
   {
      case RSD_S16_LE:
         info.bits  = 16;
         info.codec = ROAR_CODEC_PCM_S_LE;
         break;
      case RSD_U16_LE:
         info.bits  = 16;
         info.codec = ROAR_CODEC_PCM_U_LE;
         break;
      case RSD_U16_BE:
         info.bits  = 16;
         info.codec = ROAR_CODEC_PCM_U_BE;
         break;
      case RSD_S16_BE:
         info.bits  = 16;
         info.codec = ROAR_CODEC_PCM_S_BE;
         break;
      case RSD_U8:
         info.bits  =  8;
         info.codec = ROAR_CODEC_PCM_U;
         break;
      case RSD_S8:
         info.bits  =  8;
         info.codec = ROAR_CODEC_PCM_S;
         break;
      case RSD_ALAW:
         info.bits  =  8;
         info.codec =  ROAR_CODEC_ALAW;
         break;
      case RSD_MULAW:
         info.bits  =  8;
         info.codec =  ROAR_CODEC_MULAW;
         break;

      default:
         return -1;
   }

   if ( roar_vs_stream(roar->sound, &info, ROAR_DIR_PLAY, &error) == -1 ) 
   {
      log_printf("Error opening device: %s\n", roar_vs_strerr(error));
      return -1;
   }

   roar->bps = info.rate * roar_info2framesize(&info);

   return 0;
}

static size_t roarvs_rsd_write(void *data, const void* buf, size_t size)
{
   roar_rsd_t *roar = data;
   if ( roar_vs_write(roar->sound, buf, size, NULL) != (ssize_t)size )
      return 0;
   return size;
}

static int roarvs_rsd_latency(void* data)
{
   roar_rsd_t *roar = data;

   int err = ROAR_ERROR_NONE;
   roar_mus_t rc;
   if ( (rc = roar_vs_latency(roar->sound, ROAR_VS_BACKEND_DEFAULT, &err)) == 0 )
   {
      if (err != ROAR_ERROR_NONE)
         return DEFAULT_CHUNK_SIZE; // Just return something halfway sane.
      else
         return 0;
   }
   else
      return rc * roar->bps / 1000000L;
}

static void roarvs_rsd_get_backend(void *data, backend_info_t *backend_info)
{
   (void)data;
   backend_info->latency = DEFAULT_CHUNK_SIZE;
   backend_info->chunk_size = DEFAULT_CHUNK_SIZE;
}

const rsd_backend_callback_t rsd_roarvs = {
   .init = roarvs_rsd_init,
   .write = roarvs_rsd_write,
   .latency = roarvs_rsd_latency,
   .close = roarvs_rsd_close,
   .get_backend_info = roarvs_rsd_get_backend,
   .open = roarvs_rsd_open,
   .backend = "RoarAudio VS"
};


