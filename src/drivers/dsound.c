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

// mmmhm. Copy pasta from bSNES. :D

#include "dsound.h"
#include "../rsound.h"

static void ds_rsd_close(void *data)
{
   ds_t* ds = data;

   if (ds->dsb_p)
   {
      IDirectSoundBuffer_Stop(ds->dsb_p);
      IDirectSoundBuffer_Release(ds->dsb_p);
   }
   if (ds->dsb_b)
   {
      IDirectSoundBuffer_Stop(ds->dsb_b);
      IDirectSoundBuffer_Release(ds->dsb_b);
   }
   free(ds);
}

static LPDIRECTSOUND g_ds;
static void ds_init(void)
{
   DirectSoundCreate(0, &g_ds, 0);
   IDirectSound_SetCooperativeLevel(g_ds, GetDesktopWindow(), DSSCL_PRIORITY);
}

static void ds_deinit(void)
{
   //IDirectSound_Release(g_ds); // Crashes for some reason in Win32.
}

static int ds_rsd_init(void** data)
{
   ds_t *sound = calloc(1, sizeof(ds_t));
   if ( sound == NULL )
      return -1;
   *data = sound;
   return 0;
}

static void clear_buffers(ds_t *ds)
{
   ds->writering = ds->rings - 1;

   DWORD size;
   void *output;
   IDirectSoundBuffer_Lock(ds->dsb_b, 0, ds->latency * ds->rings, &output, &size, 0, 0, 0);
   memset(output, 0, size);
   IDirectSoundBuffer_Unlock(ds->dsb_b, output, size, 0, 0);

   IDirectSoundBuffer_Play(ds->dsb_b, 0, 0, DSBPLAY_LOOPING);
}

static int ds_rsd_open(void* data, wav_header_t *w)
{
   ds_t* ds = data;

   int bits = 16;
   ds->fmt = w->rsd_format;
   ds->conv = converter_fmt_to_s16ne(w->rsd_format);

   ds->rings = 4;
   ds->latency = DEFAULT_CHUNK_SIZE * 4;

   memset(&ds->dsbd, 0, sizeof(ds->dsbd));
   ds->dsbd.dwSize = sizeof(ds->dsbd);
   ds->dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
   ds->dsbd.dwBufferBytes = 0;
   ds->dsbd.lpwfxFormat = 0;
   if (IDirectSound_CreateSoundBuffer(g_ds, &ds->dsbd, &ds->dsb_p, 0) != DS_OK)
      return -1;

   memset(&ds->wfx, 0, sizeof(ds->wfx));
   ds->wfx.wFormatTag      = WAVE_FORMAT_PCM;
   ds->wfx.nChannels       = w->numChannels;
   ds->wfx.nSamplesPerSec  = w->sampleRate;
   ds->wfx.wBitsPerSample  = bits;
   ds->wfx.nBlockAlign     = ds->wfx.wBitsPerSample / 8 * ds->wfx.nChannels;
   ds->wfx.nAvgBytesPerSec = ds->wfx.nSamplesPerSec * ds->wfx.nBlockAlign;
   IDirectSoundBuffer_SetFormat(ds->dsb_p, &ds->wfx);

   memset(&ds->dsbd, 0, sizeof(ds->dsbd));
   ds->dsbd.dwSize  = sizeof(ds->dsbd);
   ds->dsbd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLFREQUENCY | DSBCAPS_GLOBALFOCUS | DSBCAPS_LOCSOFTWARE;
   ds->dsbd.dwBufferBytes   = ds->rings * ds->latency;
   ds->dsbd.guid3DAlgorithm = GUID_NULL;
   ds->dsbd.lpwfxFormat     = &ds->wfx;
   if (IDirectSound_CreateSoundBuffer(g_ds, &ds->dsbd, &ds->dsb_b, 0) != DS_OK)
      return -1;

   IDirectSoundBuffer_SetFrequency(ds->dsb_b, w->sampleRate);
   IDirectSoundBuffer_SetCurrentPosition(ds->dsb_b, 0);

   clear_buffers(ds);

   return 0;
}

static size_t ds_rsd_write(void *data, const void* inbuf, size_t size)
{
   ds_t *ds = data;

   size_t osize = size;

   uint8_t convbuf[2*size];
   void *buffer_ptr = (void*)inbuf;

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

   DWORD pos, ds_size;
   ds->writering = (ds->writering + 1) % ds->rings;
   for (;;)
   {
      IDirectSoundBuffer_GetCurrentPosition(ds->dsb_b, &pos, 0);
      unsigned activering = pos / ds->latency;
      if (activering != ds->writering)
         break;

      Sleep(1);
   }

   void *output;
   if (IDirectSoundBuffer_Lock(ds->dsb_b, ds->writering * ds->latency, osize, &output, &ds_size, 0, 0, 0) == DS_OK)
   {
      memcpy(output, buffer_ptr, ds_size);
      IDirectSoundBuffer_Unlock(ds->dsb_b, output, ds_size, 0, 0);
   }

   return size;
}

static int ds_rsd_latency(void* data)
{
   ds_t *ds = data;

   DWORD pos;
   IDirectSoundBuffer_GetCurrentPosition(ds->dsb_b, &pos, 0);
   unsigned activering = pos / ds->latency;
   unsigned next_writering = (ds->writering + 1) % ds->rings;
   if (next_writering <= activering)
      next_writering += ds->rings;

   int delta = next_writering - activering;

   int latency = ds->latency;
   if (rsnd_format_to_bytes(ds->fmt) == 1)
      latency /= 2;
   else if (rsnd_format_to_bytes(ds->fmt) == 4)
      latency *= 2;

   return ds->latency * delta;
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
   .initialize = ds_init,
   .shutdown = ds_deinit,
   .write = ds_rsd_write,
   .latency = ds_rsd_latency,
   .close = ds_rsd_close,
   .get_backend_info = ds_rsd_get_backend,
   .open = ds_rsd_open,
   .backend = "DirectSound"
};


