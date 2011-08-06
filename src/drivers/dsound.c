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

#include "dsound.h"
#include "../rsound.h"

static void ds_rsd_close(void *data)
{
   ds_t* ds = data;

   if (ds->dsb)
   {
      IDirectSoundBuffer_Stop(ds->dsb);
      IDirectSoundBuffer_Release(ds->dsb);
   }

   if (ds->ds)
      IDirectSound_Release(ds->ds);

   free(ds);
}

static int ds_rsd_init(void** data)
{
   ds_t *sound = calloc(1, sizeof(ds_t));
   if (sound == NULL)
      return -1;
   *data = sound;
   return 0;
}

static void clear_buffers(ds_t *ds)
{
   ds->writering = ds->rings - 1;

   DWORD size;
   void *output;
   if (IDirectSoundBuffer_Lock(ds->dsb, 0, 0, &output, &size, 0, 0, DSBLOCK_ENTIREBUFFER) == DS_OK)
   {
      memset(output, 0, size);
      IDirectSoundBuffer_Unlock(ds->dsb, output, size, 0, 0);
   }
}

static int ds_rsd_open(void* data, wav_header_t *w)
{
   ds_t* ds = data;

   if (DirectSoundCreate(NULL, &ds->ds, NULL) != DS_OK)
      return -1;

   if (IDirectSound_SetCooperativeLevel(ds->ds, GetDesktopWindow(), DSSCL_NORMAL) != DS_OK)
      return -1;

   int bits = 16;
   ds->fmt = w->rsd_format;
   ds->conv = converter_fmt_to_s16ne(w->rsd_format);

   ds->rings = 16;
   ds->latency = DEFAULT_CHUNK_SIZE * 2;

   WAVEFORMATEX wfx = {
      .wFormatTag = WAVE_FORMAT_PCM,
      .nChannels = w->numChannels,
      .nSamplesPerSec = w->sampleRate,
      .wBitsPerSample = bits,
      .nBlockAlign = w->numChannels * bits / 8,
      .nAvgBytesPerSec = w->sampleRate * w->numChannels * bits / 8,
   };

   DSBUFFERDESC bufdesc = {
      .dwSize = sizeof(DSBUFFERDESC),
      .dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS,
      .dwBufferBytes = ds->rings * ds->latency,
      .lpwfxFormat = &wfx,
   };

   if (IDirectSound_CreateSoundBuffer(ds->ds, &bufdesc, &ds->dsb, 0) != DS_OK)
      return -1;

   IDirectSoundBuffer_SetCurrentPosition(ds->dsb, 0);
   clear_buffers(ds);
   IDirectSoundBuffer_Play(ds->dsb, 0, 0, DSBPLAY_LOOPING);

   return 0;
}

static size_t ds_rsd_write(void *data, const void* inbuf, size_t size)
{
   ds_t *ds = data;

   size_t osize = size;

   uint8_t convbuf[2 * size];
   const uint8_t *buffer_ptr = inbuf;

   if (ds->conv != RSD_NULL)
   {
      if (rsnd_format_to_bytes(ds->fmt) == 1)
         osize = 2 * size;
      else if (rsnd_format_to_bytes(ds->fmt) == 4)
         osize = size / 2;

      memcpy(convbuf, inbuf, size);

      audio_converter(convbuf, ds->fmt, ds->conv, size);
      buffer_ptr = convbuf;
   }

   // With this approach we are prone to underruns which would "ring",
   // but the RSound API does not really encourage letting stuff underrun anyways.
   ds->writering = (ds->writering + 1) % ds->rings;
   for (;;)
   {
      DWORD pos;
      IDirectSoundBuffer_GetCurrentPosition(ds->dsb, &pos, 0);
      unsigned activering = pos / ds->latency;
      if (activering != ds->writering)
         break;

      Sleep(1);
   }

   void *output1, *output2;
   DWORD size1, size2;

   HRESULT res;
   if ((res = IDirectSoundBuffer_Lock(ds->dsb, ds->writering * ds->latency, osize,
            &output1, &size1, &output2, &size2, 0)) != DS_OK)
   {
      if (res != DSERR_BUFFERLOST)
         return 0;

      if (IDirectSoundBuffer_Restore(ds->dsb) != DS_OK)
         return 0;

      if (IDirectSoundBuffer_Lock(ds->dsb, ds->writering * ds->latency, osize,
                  &output1, &size1, &output2, &size2, 0) != DS_OK)
         return 0;
   }

   memcpy(output1, buffer_ptr, size1);
   memcpy(output2, buffer_ptr + size1, size2);

   IDirectSoundBuffer_Unlock(ds->dsb, output1, size1, output2, size2);

   return size;
}

static int ds_rsd_latency(void* data)
{
   ds_t *ds = data;

   DWORD pos;
   IDirectSoundBuffer_GetCurrentPosition(ds->dsb, &pos, 0);
   DWORD next_writepos = ((ds->writering + 1) % ds->rings) * ds->latency;
   if (next_writepos <= pos)
      next_writepos += ds->rings * ds->latency;

   int delta = next_writepos - pos;

   if (rsnd_format_to_bytes(ds->fmt) == 1)
      delta /= 2;
   else if (rsnd_format_to_bytes(ds->fmt) == 4)
      delta *= 2;

   return delta;
}

static void ds_rsd_get_backend(void *data, backend_info_t *backend_info)
{
   ds_t *ds = data;

   int latency = ds->latency;
   if (rsnd_format_to_bytes(ds->fmt) == 1)
      latency /= 2;
   else if (rsnd_format_to_bytes(ds->fmt) == 4)
      latency *= 2;

   backend_info->latency = latency * ds->rings;
   backend_info->chunk_size = latency;
}

const rsd_backend_callback_t rsd_ds = {
   .init = ds_rsd_init,
   .write = ds_rsd_write,
   .latency = ds_rsd_latency,
   .close = ds_rsd_close,
   .get_backend_info = ds_rsd_get_backend,
   .open = ds_rsd_open,
   .backend = "DirectSound"
};


