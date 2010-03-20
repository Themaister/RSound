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
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <time.h>

/* Default values */
char device[128] = "default";
char port[128] = "12345";
int verbose = 0;
int debug = 0;
rsd_backend_callback_t *backend = NULL;
int daemonize = 0;
int no_threading = 0;

static void* get_addr(struct sockaddr*);

int main(int argc, char ** argv)
{
   int s, s_new, s_ctl, i;
   connection_t conn;
   struct sockaddr_storage their_addr[2];
   socklen_t addr_size;
   struct pollfd fd;
   char remoteIP[2][INET6_ADDRSTRLEN] = { "", "" };
   char *valid_addr[2];
   char timestring[64] = {0};
   
   parse_input(argc, argv);
   
   if ( daemonize )
   {
      if ( debug )
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

   if ( debug )
      fprintf(stderr, "Listening for connection ...\n");

   fd.fd = s;
   fd.events = POLLIN;

   /* Set up listening socket */
   if ( listen(s, 2) == -1 )
   {
      fprintf(stderr, "Couldn't listen for connections \"%s\"...\n", strerror(errno));
      exit(1);
   }
	
   /* Sets up interface for cleanly shutting down the server */
   write_pid_file();
   signal(SIGINT, cleanup);
   signal(SIGTERM, cleanup);

   while(1)
   {
      /* Accepts, and creates new sound thread */
           
      addr_size = sizeof (their_addr[0]);
      s_new = accept(s, (struct sockaddr*)&their_addr[0], &addr_size);

      if ( s_new == -1 )
      {
         fprintf(stderr, "Accepting failed... Errno: %d\n", errno);
         fprintf(stderr, "%s\n", strerror( errno ) ); 
         continue;
      }
            
      /* Accepts a ctl socket. They have to come from same source. 
       * Times out very quickly (in case the server is being queried from an unknown source. */

      if (poll(&fd, 1, 200) < 0)
      {
         perror("poll");
         exit(1);
      }

      if (fd.revents & POLLIN)
      {
         addr_size = sizeof (their_addr[0]);
         s_ctl = accept(s, (struct sockaddr*)&their_addr[1], &addr_size);
      }
      else
      {
         fprintf(stderr, "CTL-socket timed out.\n");
         close(s_new);
         continue;
      }

      if ( s_ctl == -1 )
      {
         fprintf(stderr, "%s\n", strerror( errno ) ); 
         continue;
      }

      /* Checks if they are from same source */
      valid_addr[0] = (char*)inet_ntop(their_addr[0].ss_family, 
         get_addr((struct sockaddr*)&their_addr[0]),
            remoteIP[0], INET6_ADDRSTRLEN);
      valid_addr[1] = (char*)inet_ntop(their_addr[1].ss_family, 
         get_addr((struct sockaddr*)&their_addr[1]),
            remoteIP[1], INET6_ADDRSTRLEN);

      if ( strcmp( remoteIP[0], remoteIP[1] ) && valid_addr[0] && valid_addr[1] )
      {
         if ( debug )
         {
            fprintf(stderr, "Warning: Got two connections from different sources.\n");
            fprintf(stderr, "%s :: %s\n", remoteIP[0], remoteIP[1]);
         }
         close(s_new);
         close(s_ctl);
         continue;
      }
      else if ( valid_addr[0] && valid_addr[1] )
      {
         if ( verbose )
         {
            time_t cur_time;
            time(&cur_time);
            strftime(timestring, 63, "%F - %H:%M:%S", localtime(&cur_time)); 
            fprintf(stderr, "Connection :: [ %s ] [ %s ] ::\n", timestring, remoteIP[1]);
         }
      }
      conn.socket = s_new;
      conn.ctl_socket = s_ctl;
      new_sound_thread(conn);
   }    
   
   return 0;
}
   
static void* get_addr(struct sockaddr *sa)
{
      if ( sa->sa_family == AF_INET ) 
      {
         return &(((struct sockaddr_in*)sa)->sin_addr);
      }
      else if ( sa->sa_family == AF_INET6 )
      {
         return &(((struct sockaddr_in6*)sa)->sin6_addr);
      }
      return NULL;
}

