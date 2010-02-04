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

static void clean_ao_interface(ao_t* sound)
{
   ao_close(sound->device);
   free(sound->buffer);
}

static int init_ao(ao_t* interface, wav_header* w)
{
   
   int default_driver;

   ao_initialize();

   default_driver = ao_default_driver_id();

   interface->format.bits = 16;
   interface->format.channels = w->numChannels;
   interface->format.rate = w->sampleRate;
   interface->format.byte_format = AO_FMT_LITTLE;

   interface->device = ao_open_live(default_driver, &interface->format, NULL);
   if ( interface->device == NULL )
   {
      fprintf(stderr, "Error opening device.\n");
      return 0;
   }

   return 1;
}

void* ao_thread ( void* data )
{
   ao_t sound;
   wav_header w;
   int rc;
   int active_connection;

   int s_new = *((int*)data);
   free(data);

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
      fprintf(stderr, "Initializing AO ...\n");

   if ( !init_ao(&sound, &w) )
   {
      fprintf(stderr, "Failed to initialize AO\n");
      close(s_new);
      pthread_exit(NULL);
   }

   uint32_t chunk_size = DEFAULT_CHUNK_SIZE;
// Dirty, and should be avoided, but I need to study the API better.
   if ( !send_backend_info(s_new, &chunk_size, 16*chunk_size, (int)w.numChannels) )
   {
      fprintf(stderr, "Couldn't send backend info.\n");
      close(s_new);
      pthread_exit(NULL);
   }

   if ( verbose )
      fprintf(stderr, "Initializing of AO successful... Party time!\n");

   sound.buffer = malloc ( chunk_size );
   if ( !sound.buffer )
   {
      fprintf(stderr, "Couldn't allocate memory for buffer.");
      close(s_new);
      pthread_exit(NULL);
   }

   active_connection = 1;
   while(active_connection)
   {
      rc = recieve_data(s_new, sound.buffer, chunk_size, chunk_size );
      if ( rc == 0 )
      {
         active_connection = 0;
         break;
      }

      ao_play(sound.device, sound.buffer, chunk_size);

   }

   close(s_new);
   clean_ao_interface(&sound);
   if ( verbose )
      fprintf(stderr, "Closed connection. The friendly PCM-service welcomes you back.\n\n\n");

   pthread_exit(NULL);
   return NULL; /* GCC warning */

}
