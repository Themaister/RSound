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

#include "oss.h"
#include "rsound.h"

static void clean_oss_interface(oss_t* sound)
{
   close(sound->audio_fd);
   close(sound->conn.socket);
   close(sound->conn.ctl_socket);
   if ( sound->buffer )
      free(sound->buffer);
}

/* Opens and sets params */
static int init_oss(oss_t* sound, wav_header* w)
{
   int format = AFMT_S16_LE;
   int stereo; 
   char oss_device[64] = {0};
   int sampleRate = w->sampleRate;

   if ( strcmp(device, "default") != 0 )
      strncpy(oss_device, device, 63);
   else
      strncpy(oss_device, OSS_DEVICE, 63);


   sound->audio_fd = open(oss_device, O_WRONLY, 0);

   if ( sound->audio_fd == -1 )
   {
      fprintf(stderr, "Could not open device %s.\n", oss_device);
      return 0;
   }
   
   if ( ioctl( sound->audio_fd, SNDCTL_DSP_SETFMT, &format) == -1 )
   {
      perror("SNDCTL_DSP_SETFMT");
      close(sound->audio_fd);
      return 0;
   }
   
   if ( format != AFMT_S16_LE )
   {
      fprintf(stderr, "Sound card doesn't support S16LE sampling format.\n");
      close(sound->audio_fd);
      return 0;
   }
   
   if ( w->numChannels == 2 )
      stereo = 1;
   else if ( w->numChannels == 1 )
      stereo = 0;
   else
   {
      fprintf(stderr, "Multichannel audio not supported.\n");
      close(sound->audio_fd);
      return 0;
   }
      
   if ( ioctl( sound->audio_fd, SNDCTL_DSP_STEREO, &stereo) == -1 )
   {
      perror("SNDCTL_DSP_STEREO");
      close(sound->audio_fd);
      return 0;
   }
   
   if ( stereo != 1 && w->numChannels != 1 )
   {
      fprintf(stderr, "Sound card doesn't support stereo mode.\n");
      close(sound->audio_fd);
      return 0;
   }
   
   if ( ioctl ( sound->audio_fd, SNDCTL_DSP_SPEED, &sampleRate ) == -1 )
   {
      perror("SNDCTL_DSP_SPEED");
      close(sound->audio_fd);
      return 0;
   }
   
   if ( sampleRate != (int)w->sampleRate )
   {
      fprintf(stderr, "Sample rate couldn't be set correctly.\n");
      close(sound->audio_fd);
      return 0;
   }
   
   
    return 1;
}

void* oss_thread( void* data )
{
   oss_t sound;
   wav_header w;
   int rc;
   int active_connection;
   int underrun_count = 0;
   uint32_t buffer_size;
   audio_buf_info zz;

   connection_t *conn = data;
   sound.conn.socket = conn->socket;
   sound.conn.ctl_socket = conn->ctl_socket;
   free(conn);

   sound.buffer = NULL;
   sound.audio_fd = -1;
   
   if ( debug )
      fprintf(stderr, "Connection accepted, awaiting WAV header data...\n");

   rc = get_wav_header(sound.conn, &w);

   if ( rc != 1 )
   {
      fprintf(stderr, "Couldn't read WAV header... Disconnecting.\n");
      goto oss_exit;
   }

   if ( debug )
   {
      fprintf(stderr, "Successfully got WAV header ...\n");
      pheader(&w);
   }  

   if ( debug )
      printf("Initializing OSS ...\n");
   if ( !init_oss(&sound, &w) )
   {
      fprintf(stderr, "Initializing of OSS failed ...\n");
      goto oss_exit;
   }

   if ( ioctl( sound.audio_fd, SNDCTL_DSP_GETOSPACE, &zz ) != 0 )
   {
      fprintf(stderr, "Getting data from ioctl failed SNDCTL_DSP_GETOSPACE.\n");
      goto oss_exit;
   }

   buffer_size = (uint32_t)zz.bytes;
   sound.fragsize = (uint32_t)zz.fragsize;
   
   /* Low packet size */
   sound.fragsize = 128 * w.numChannels * 2;
   /* :) */
   
   if ( debug )
      fprintf(stderr, "Fragsize %d, Buffer size %d\n", sound.fragsize, (int)buffer_size);
   
   backend_info_t backend = {
      .latency = (uint32_t)zz.fragsize,
      .chunk_size = sound.fragsize
   };

   if ( !send_backend_info(sound.conn, backend ))
   {
      fprintf(stderr, "Error sending backend info.\n");
      goto oss_exit;
   }
   
   sound.buffer = malloc ( sound.fragsize );
   if ( !sound.buffer )
   {
      fprintf(stderr, "Error allocating memory for sound buffer.\n");
      goto oss_exit;
   }
  
   if ( debug )
      printf("Initializing of OSS successful...\n");


   active_connection = 1;
   /* While connection is active, read data and reroutes it to OSS_DEVICE */
   while(active_connection)
   {
      rc = recieve_data(sound.conn, sound.buffer, sound.fragsize);
      if ( rc == 0 )
      {
         active_connection = 0;
         break;
      }

      rc = write(sound.audio_fd, sound.buffer, sound.fragsize);
      if (rc < (int)sound.fragsize) 
      {
         if ( debug )
            fprintf(stderr, "Underrun occurred. Count: %d\n", ++underrun_count);
      } 
      else if (rc < 0) 
      {
         fprintf(stderr,
               "Error from write\n");
      }  
   }

   if ( debug )
      fprintf(stderr, "Closed connection.\n\n");

oss_exit:
   clean_oss_interface(&sound);
   pthread_exit(NULL);
   return NULL; /* GCC warning */
}

