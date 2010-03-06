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

#include "porta.h"
#include "rsound.h"

#define FRAMES_PER_BUFFER (DEFAULT_CHUNK_SIZE/4)

static void clean_porta_interface(porta_t* sound)
{
   close(sound->conn.socket);
   close(sound->conn.ctl_socket);
   if ( sound->stream )
   {
      Pa_StopStream ( sound->stream );
      Pa_CloseStream ( sound->stream );
   }
   if ( sound->buffer )
      free(sound->buffer);
}

/* Designed to use the blocking I/O API. It's just a more simple design. */
static int init_porta(porta_t* sound, wav_header* w)
{
   PaError err;
   PaStreamParameters params;
   
   err = Pa_Initialize();
   if ( err != paNoError )
   {
      fprintf(stderr, "Error initalizing.\n");
      fprintf(stderr,  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
      return 0;
   }
   
   params.device = Pa_GetDefaultOutputDevice();
   params.channelCount = w->numChannels;
   params.sampleFormat = paInt16;
   params.suggestedLatency = Pa_GetDeviceInfo( params.device )->defaultLowOutputLatency;
   params.hostApiSpecificStreamInfo = NULL;
   
   sound->size = FRAMES_PER_BUFFER * 2 * w->numChannels;
   sound->frames = FRAMES_PER_BUFFER;
   sound->buffer = malloc ( sound->size );
   
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
      return 0;
   }

   err = Pa_StartStream ( sound->stream );
   if ( err != paNoError )
   {
      fprintf(stderr, "Couldn't start stream.\n");
      fprintf(stderr,  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
      return 0;
   }

   return 1;
}

void* porta_thread( void* data )
{
   porta_t sound;
   wav_header w;
   int rc;
   int active_connection;
   PaError err;
   
   connection_t *conn = data;
   sound.conn.socket = conn->socket;
   sound.conn.ctl_socket = conn->ctl_socket;
   free(conn);

   sound.buffer = NULL;
   sound.stream = NULL;
   
   if ( verbose )
      fprintf(stderr, "Connection accepted, awaiting WAV header data...\n");

   rc = get_wav_header(sound.conn, &w);

   if ( rc != 1 )
   {
      fprintf(stderr, "Couldn't read WAV header... Disconnecting.\n");
      goto porta_quit;
   }

   if ( verbose )
   {
      fprintf(stderr, "Successfully got WAV header ...\n");
      pheader(&w);
   }  

   if ( verbose )
      printf("Initializing PortAudio ...\n");
   if ( !init_porta(&sound, &w) )
   {
      fprintf(stderr, "Initializing of PortAudio failed ...\n");
      goto porta_quit;
   }

   /* Just have to set something for buffer_size */
   backend_info_t backend = {
      .latency = (uint32_t)(w.numChannels * 2 * w.sampleRate * Pa_GetStreamInfo( sound.stream )->outputLatency),
      .chunk_size = sound.size
   };



   if ( !send_backend_info(sound.conn, backend ) )
   {
      fprintf(stderr, "Couldn't send backend info.\n");
      goto porta_quit;
   }

   if ( verbose )
      printf("Initializing of PortAudio successful... Party time!\n");

   active_connection = 1;
   
   while(active_connection)
   {
      memset(sound.buffer, 0, sound.size);

      /* Reads complete buffer */
      rc = recieve_data(sound.conn, sound.buffer, sound.size);
      if ( rc == 0 )
      {
         active_connection = 0;
         break;
      }
   
      err = Pa_WriteStream( sound.stream, sound.buffer, sound.frames );
      if ( err )
      {
         fprintf(stderr, "Buffer underrun occured.\n");
      }
      
   }

   if ( verbose )
      fprintf(stderr, "Closed connection. The friendly PCM-service welcomes you back.\n\n\n");

porta_quit: 
   clean_porta_interface(&sound);
   pthread_exit(NULL);
   return NULL; /* To make GCC warning happy */

}

