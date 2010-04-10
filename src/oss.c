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

#include "oss.h"
#include "rsound.h"

static void oss_close(void *data)
{
   oss_t *sound = data;
   close(sound->audio_fd);
}

/* Opens and sets params */
static int oss_init(void **data)
{
   oss_t *sound = calloc(1, sizeof(oss_t));
   if ( sound == NULL )
      return -1;
   *data = sound;
   return 0;
}

static int oss_open(void *data, wav_header_t *w)
{
   oss_t *sound = data;
   char oss_device[128] = {0};
   if ( strcmp(device, "default") != 0 )
      strncpy(oss_device, device, 127);
   else
      strncpy(oss_device, OSS_DEVICE, 127);

   sound->audio_fd = open(oss_device, O_WRONLY, 0);
   if ( sound->audio_fd == -1 )
   {
      fprintf(stderr, "Couldn't open device %s.\n", oss_device);
      return -1;
   }
   
   int format;
   switch ( w->rsd_format )
   {
      case RSD_S16_LE:
         format = AFMT_S16_LE;
         break;
      case RSD_U16_LE:
         format = AFMT_U16_LE;
         break;
      case RSD_S16_BE:
         format = AFMT_S16_BE;
         break;
      case RSD_U16_BE:
         format = AFMT_U16_BE;
         break;
      case RSD_U8:
         format = AFMT_U8;
         break;
      case RSD_S8:
         format = AFMT_S8;
         break;

      default:
         return -1;
   }
   int oldfmt = format;
   
   int stereo; 
   int sampleRate = w->sampleRate;
   
   if ( ioctl( sound->audio_fd, SNDCTL_DSP_SETFMT, &format) == -1 )
   {
      perror("SNDCTL_DSP_SETFMT");
      return -1;
   }
   
   if ( format != oldfmt )
   {
      fprintf(stderr, "Sound card doesn't support %s sampling format.\n", rsnd_format_to_string(w->rsd_format) );
      return -1;
   }
   
   if ( w->numChannels == 2 )
      stereo = 1;
   else if ( w->numChannels == 1 )
      stereo = 0;
   else
   {
      fprintf(stderr, "Multichannel audio not supported.\n");
      return -1;
   }
      
   if ( ioctl( sound->audio_fd, SNDCTL_DSP_STEREO, &stereo) == -1 )
   {
      perror("SNDCTL_DSP_STEREO");
      return -1;
   }
   
   if ( stereo != 1 && w->numChannels != 1 )
   {
      fprintf(stderr, "Sound card doesn't support stereo mode.\n");
      return -1;
   }
   
   if ( ioctl ( sound->audio_fd, SNDCTL_DSP_SPEED, &sampleRate ) == -1 )
   {
      perror("SNDCTL_DSP_SPEED");
      return -1;
   }
   
   if ( sampleRate != (int)w->sampleRate )
   {
      fprintf(stderr, "Sample rate couldn't be set correctly.\n");
      return -1;
   }
   
   return 0;
}

static void oss_get_backend (void *data, backend_info_t *backend_info)
{
   oss_t *sound = data;
   audio_buf_info zz;

   if ( ioctl( sound->audio_fd, SNDCTL_DSP_GETOSPACE, &zz ) != 0 )
   {
      fprintf(stderr, "Getting data from ioctl failed SNDCTL_DSP_GETOSPACE.\n");
      memset(backend_info, 0, sizeof(backend_info_t));
   }

   backend_info->latency = zz.fragsize;
   backend_info->chunk_size = DEFAULT_CHUNK_SIZE;
}

static size_t oss_write (void *data, const void* buf, size_t size)
{
   oss_t *sound = data;
   return write(sound->audio_fd, buf, size);
}

const rsd_backend_callback_t rsd_oss = {
   .init = oss_init,
   .open= oss_open,
   .write = oss_write,
   .get_backend_info = oss_get_backend,
   .close = oss_close,
   .backend = "OSS"
};
   

