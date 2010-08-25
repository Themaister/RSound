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

#include "porta.h"
#include "../rsound.h"

#define FRAMES_PER_BUFFER (DEFAULT_CHUNK_SIZE/4)

static void porta_close(void *data)
{
   porta_t* sound = data; 
   if ( sound->stream )
   {
      Pa_StopStream ( sound->stream );
      Pa_CloseStream ( sound->stream );
   }
   free(sound);
}

static void porta_initialize(void)
{
   Pa_Initialize();
}

static void porta_shutdown(void)
{
   Pa_Terminate();
}

/* Designed to use the blocking I/O API. It's just a more simple design. */
static int porta_init(void **data)
{
   porta_t *sound = calloc(1, sizeof(porta_t));
   if ( sound == NULL )
      return -1;
   *data = sound;
   return 0;
}

static int porta_open(void *data, wav_header_t *w)
{
   porta_t *sound = data;
   PaError err;
   PaStreamParameters params;

   params.device = Pa_GetDefaultOutputDevice();
   params.channelCount = w->numChannels;

   sound->converter = RSD_NULL;
   sound->fmt = w->rsd_format;

   switch ( w->rsd_format )
   {
      case RSD_S16_LE:
         params.sampleFormat = paInt16;
         if ( !is_little_endian() )
            sound->converter |= RSD_SWAP_ENDIAN;
         break;

      case RSD_U16_LE:
         params.sampleFormat = paInt16;
         sound->converter |= RSD_U_TO_S;
         if ( !is_little_endian() )
            sound->converter |= RSD_SWAP_ENDIAN;
         break;

      case RSD_S16_BE:
         params.sampleFormat = paInt16;
         if ( is_little_endian() )
            sound->converter |= RSD_SWAP_ENDIAN;
         break;

      case RSD_U16_BE:
         params.sampleFormat = paInt16;
         sound->converter |= RSD_U_TO_S;
         if ( is_little_endian() )
            sound->converter |= RSD_SWAP_ENDIAN;
         break;

      case RSD_U8:
         params.sampleFormat = paUInt8;
         break;

      case RSD_S8:
         params.sampleFormat = paInt8;
         break;

      case RSD_ALAW:
         params.sampleFormat = paInt16;
         sound->converter |= RSD_ALAW_TO_S16;
         break;

      case RSD_MULAW:
         params.sampleFormat = paInt16;
         sound->converter |= RSD_MULAW_TO_S16;
         break;

      default:
         return -1;
   }


   params.suggestedLatency = Pa_GetDeviceInfo( params.device )->defaultLowOutputLatency;
   params.hostApiSpecificStreamInfo = NULL;

   sound->size = FRAMES_PER_BUFFER * rsnd_format_to_bytes(w->rsd_format) * w->numChannels;
   if ( w->rsd_format & ( RSD_ALAW | RSD_MULAW ) )
      sound->size *= 2;

   sound->frames = FRAMES_PER_BUFFER;
   sound->bps = rsnd_format_to_bytes(w->rsd_format) * w->numChannels * w->sampleRate;

   err = Pa_OpenStream (
         &sound->stream,
         NULL,
         &params,
         w->sampleRate,
         sound->frames,
         paClipOff,
         NULL,
         NULL );  

   if ( err != paNoError )
   {
      fprintf(stderr, "Couldn't open stream.\n");
      fprintf(stderr,  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
      return -1;
   }

   err = Pa_StartStream ( sound->stream );
   if ( err != paNoError )
   {
      fprintf(stderr, "Couldn't start stream.\n");
      fprintf(stderr,  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
      return -1;
   }

   return 0;
}

static void porta_get_backend(void *data, backend_info_t *backend_info)
{
   porta_t *sound = data;
   backend_info->latency = (uint32_t)(sound->bps * Pa_GetStreamInfo( sound->stream )->outputLatency),
      backend_info->chunk_size = sound->size;
}

static int porta_latency(void *data)
{
   porta_t *sound = data;
   return sound->bps * Pa_GetStreamInfo(sound->stream)->outputLatency;
}

static size_t porta_write(void *data, const void *inbuf, size_t size)
{
   porta_t *sound = data;
   PaError err;

   size_t osize = size;
   
   uint8_t convbuf[2*size];
   void *buffer = (void*)inbuf;

   if (sound->converter != RSD_NULL)
   {
      osize = (sound->fmt & (RSD_ALAW | RSD_MULAW)) ? 2*size : size;

      memcpy(convbuf, inbuf, size);

      audio_converter(convbuf, sound->fmt, sound->converter, size);
      buffer = convbuf;
   }

   size_t write_frames = osize / (sound->size / sound->frames);

   err = Pa_WriteStream( sound->stream, buffer, write_frames );
   if ( err < 0 && err != paOutputUnderflowed )
      return -1;

   return size;
}

const rsd_backend_callback_t rsd_porta = {
   .init = porta_init,
   .initialize = porta_initialize,
   .close = porta_close,
   .latency = porta_latency,
   .write = porta_write,
   .get_backend_info = porta_get_backend,
   .open = porta_open,
   .shutdown = porta_shutdown,
   .backend = "PortAudio"
};


