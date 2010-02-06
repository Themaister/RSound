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

#include "rsound.h"

/* Default values */
char device[128] = "default";
char port[128] = "12345";
int verbose = 0;
void* (*backend) ( void * ) = NULL;
int daemonize = 1;
int no_threading = 0;

int main(int argc, char ** argv)
{
   int s, s_new, i;
   
   parse_input(argc, argv);
   
   if ( daemonize )
   {
      fprintf(stderr, "Forking into background ...\n");
      i = fork();
      if ( i < 0 ) exit(1);
      if ( i > 0 ) exit(0);
      /* Forking into background */
   }

   /* Sets up listening socket */
   s = set_up_socket();

   if ( s < 0 )
   {
      fprintf(stderr, "Couldn't set up listening socket. Exiting ...\n");
      exit(1);
   }

   if ( verbose )
      fprintf(stderr, "Listening for connection ...\n");

   while(1)
   {
      /* Listens, accepts, and creates new sound thread */
      if ( listen(s, 1) == -1 )
      {
         fprintf(stderr, "Couldn't listen for connection ...\n");
         exit(10);
      }

      s_new = accept(s, NULL, NULL);

      if ( s_new == -1 )
      {
         fprintf(stderr, "Accepting failed... Errno: %d\n", errno);
         fprintf(stderr, "%s\n", strerror( errno ) ); 
         continue;
      }

      new_sound_thread(s_new);
   }    
   
   return 0;
}
   
   
   



