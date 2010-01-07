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

#define FRAMES_PER_BUFFER 256

// Designed to use the blocking I/O API. It's just a more simple design.
int init_porta(porta_t* sound, wav_header* w)
{
   PaError err;
   
   err = Pa_Initialize();
   if ( err != paNoError )
   {
      fprintf(stderr, "Error initalizing.\n");
      fprintf(stderr,  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
      return 0;
   }
   
   PaStreamParameters params;
   params.device = Pa_GetDefaultOutputDevice();
   params.channelCount = w->numChannels;
   params.sampleFormat = paInt16;
   params.suggestedLatency = Pa_GetDeviceInfo( params.device )->defaultHighOutputLatency;
   params.hostApiSpecificStreamInfo = NULL;
   
   sound->size = FRAMES_PER_BUFFER * 2 * w->numChannels;
   sound->buffer = malloc ( sound->size );
   
   err = Pa_OpenStream (
         &sound->stream,
         NULL,
         &params,
         w->sampleRate,
         FRAMES_PER_BUFFER,
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

void clean_porta_interface(porta_t* sound)
{
   Pa_StopStream ( sound->stream );
   Pa_CloseStream ( sound->stream );
   free(sound->buffer);
}


void* porta_thread( void* socket )
{
   porta_t sound;
   wav_header w;
   int rc;
   int read_counter;
   int active_connection;
   int underrun_count = 0;
   PaError err;
   
   int s_new = *((int*)socket);
   free(socket);

   if ( verbose )
      fprintf(stderr, "Connection accepted, awaiting WAV header data...\n");

   rc = get_wav_header(s_new, &w);

   if ( rc != 1 )
   {
      close(s_new);
      fprintf(stderr, "Couldn't read WAV header... Disconnecting.\n");
      pthread_exit(NULL);
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
      close(s_new);
      pthread_exit(NULL);
   }
   

   if ( verbose )
      printf("Initializing of PortAudio successful... Party time!\n");


   active_connection = 1;
   
   while(active_connection)
   {
      
      memset(sound.buffer, 0, sound.size);

      // Reads complete buffer
      for ( read_counter = 0; read_counter < sound.size; read_counter += CHUNK_SIZE )
      {
         rc = recv(s_new, sound.buffer + read_counter, CHUNK_SIZE, 0);
         if ( rc == 0 )
         {
            active_connection = 0;
            break;
         }
      }
   
      err = Pa_WriteStream( sound.stream, sound.buffer, FRAMES_PER_BUFFER );
      if ( err )
      {
         fprintf(stderr, "Error occured while writing stream.\n");
         active_connection = 0;
      }
      
   }
   
   close(s_new);
   clean_porta_interface(&sound);
   if ( verbose )
      fprintf(stderr, "Closed connection. The friendly PCM-service welcomes you back.\n\n\n");

   pthread_exit(NULL);

}

