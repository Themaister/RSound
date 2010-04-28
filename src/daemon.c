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

#include "rsound.h"
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <netinet/in.h>

/* Default values */

/* Global variables for use in modules */

char device[128] = "default";
char port[128] = "12345";
char unix_sock[128] = "";
int verbose = 0;
int debug = 0;
const rsd_backend_callback_t *backend = NULL;
int daemonize = 0;
int no_threading = 0;
int listen_socket = 0;
int rsd_conn_type = RSD_CONN_TCP;

static void* get_addr(struct sockaddr*);
static int legal_ip(const char*);
static int valid_ips(struct sockaddr_storage *their_addr);
static void log_message(const char* ip);

int main(int argc, char ** argv)
{
   int s = -1, s_new = -1, s_ctl = -1, i;
   connection_t conn;
   struct sockaddr_storage their_addr[2];
   socklen_t addr_size;
   struct pollfd fd;

   /* Parses input and sets the global variables */
   parse_input(argc, argv);
   
   /* Should we fork and kill our parent? :p */
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

   // Need to have a global socket so that we can cleanly close the socket in the signal handling routines.
   listen_socket = s;

   if ( debug )
      fprintf(stderr, "Listening for connection ...\n");

   fd.fd = s;
   fd.events = POLLIN;

   /* Set up listening socket */
   if ( listen(s, 10) == -1 )
   {
      fprintf(stderr, "Couldn't listen for connections \"%s\"...\n", strerror(errno));
      exit(1);
   }
	
   /* Sets up interface for cleanly shutting down the server */
   write_pid_file();
   signal(SIGINT, cleanup);
   signal(SIGTERM, cleanup);

   /* In case our backend API needs some initializing functions */
   initialize_audio();


   /* We're accepting two connects after each other, as we have one stream socket for audio data
      and one for controlling the server. Currently, the control socket is only useful for
      determining quickly when the client has hung up the connection. In case a control socket
      isn't supplied in a short time window (nmap, port scanners, etc),
      we shut down the connection. The connection, if accepted, will be handled in a new thread. */

   for(;;)
   {
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
         close(s_new);
         close(s_ctl);
         close(s);
         exit(1);
      }

      /* Accepts the connection if there's one pending */
      if (fd.revents & POLLIN)
      {
         addr_size = sizeof (their_addr[0]);
         s_ctl = accept(s, (struct sockaddr*)&their_addr[1], &addr_size);
      }
      /* We didn't get a control socket, so we don't care about it :) 
       If s_ctl is 0, the backend will not perform any operations on it. */
      else 
      {
         if ( debug )
            fprintf(stderr, "CTL-socket timed out. Ignoring CTL-socket. \n");

         s_ctl = 0;
      }

      if ( s_ctl == -1 )
      {
         close(s_new); s_new = -1;
         fprintf(stderr, "%s\n", strerror( errno ) ); 
         continue;
      }

      /* Checks if they are from same source, if not, close the connection. */
      /* Check will be ignored if there is no ctl-socket active. */
      /* TODO: Security here is *retarded* :D */
      if ( (s_ctl > 0) && valid_ips(their_addr) < 0 )
      {
         close(s_new); s_new = -1;
         close(s_ctl); s_ctl = -1;
         continue;
      }
   
      conn.socket = s_new;
      conn.ctl_socket = s_ctl;
      new_sound_thread(conn);
      s_new = -1; // Makes sure that we cannot clutter the backend connection in any way.
      s_ctl = -1;
   }

   return 0;
}
   
static void* get_addr(struct sockaddr *sa)
{
      /* Gotta love those & 'n * :D */

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

/* For now, just accept the IP blindly (tinfoil hat off) */
static int legal_ip( const char* remoteIP )
{
   (void)remoteIP;
   return 1;
}

static int valid_ips( struct sockaddr_storage *their_addr )
{

   char remoteIP[2][INET6_ADDRSTRLEN] = { "", "" };

   inet_ntop(their_addr[0].ss_family, 
         get_addr((struct sockaddr*)&their_addr[0]),
         remoteIP[0], INET6_ADDRSTRLEN);
   inet_ntop(their_addr[1].ss_family, 
         get_addr((struct sockaddr*)&their_addr[1]),
         remoteIP[1], INET6_ADDRSTRLEN);


   if ( strcmp( remoteIP[0], remoteIP[1] ) != 0  )
   {
      fprintf(stderr, "*** Warning: Got two connections from different sources. ***\n");
      fprintf(stderr, "*** %s :: %s ***\n", remoteIP[0], remoteIP[1]);
      return -1;
   }

   log_message(remoteIP[1]);

   /* Currently, legal_ip always returns 1 */
   if ( !legal_ip( remoteIP[0] ) )
   {
      return -1;
   }

   return 0;

}

static void log_message( const char * ip )
{
   char timestring[64] = {0};
   
   if ( verbose )
   {
      time_t cur_time;
      time(&cur_time);
      strftime(timestring, 63, "%F - %H:%M:%S", localtime(&cur_time)); 
      fprintf(stderr, "Connection :: [ %s ] [ %s ] ::\n", timestring, ip);
   }

}



