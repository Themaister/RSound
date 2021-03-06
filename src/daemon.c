/*  RSound - A PCM audio client/server
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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

#include <poll.h>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#endif

#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

/* Default values */

/* Global variables for use in modules */

char device[128] = "default";
char port[128] = "12345";
char bindaddr[128] = "";

const rsd_backend_callback_t *backend = NULL;
#ifndef _WIN32
char unix_sock[128] = "";
int no_threading = 0;
#endif

#ifdef HAVE_SAMPLERATE
int src_converter = SRC_SINC_FASTEST;
#endif

int verbose = 0;
int debug = 0;
int use_syslog = 0;
int listen_socket = 0;
int rsd_conn_type = RSD_CONN_TCP;
int resample_freq = 0;
int daemonize = 0;

static void* get_addr(struct sockaddr*);
static int valid_ips(struct sockaddr_storage *their_addr);
static void log_message(const char* ip);

// Union for casting without aliasing violations.
static union
{
   struct sockaddr* addr;
   struct sockaddr_storage* storage;
   struct sockaddr_in* v4;
   struct sockaddr_in6* v6;
} u[2];

int main(int argc, char ** argv)
{
   int s = -1, s_new = -1, s_ctl = -1;
   connection_t conn;
   struct sockaddr_storage their_addr[2];
   u[0].storage = &their_addr[0];
   u[1].storage = &their_addr[1];
   socklen_t addr_size;
   struct pollfd fd;

   /* Parses input and sets the global variables */
   parse_input(argc, argv);

#ifdef _WIN32
   if ( daemonize )
      FreeConsole();
#else
   /* Should we fork and kill our parent? :p */
   if ( daemonize )
   {
      if ( debug )
         log_printf("Forking into background ...\n");
      int i = fork();
      if ( i < 0 ) exit(1);
      if ( i > 0 ) exit(0);
      /* Forking into background */
   }
#endif

#ifdef HAVE_SYSLOG
   if (use_syslog)
      openlog("rsd", 0, LOG_USER);
#endif

   /* Sets up listening socket */
   s = set_up_socket();

   if ( s < 0 )
   {
      log_printf("Couldn't set up listening socket. Exiting ...\n");
      exit(1);
   }

   // Need to have a global socket so that we can cleanly close the socket in the signal handling routines.
   listen_socket = s;

   if ( debug )
      log_printf("Listening for connection ...\n");

   fd.fd = s;
   fd.events = POLLIN;

   /* Set up listening socket */
   if ( listen(s, 10) == -1 )
   {
      log_printf("Couldn't listen for connections \"%s\"...\n", strerror(errno));
      exit(1);
   }

#ifdef _WIN32
   atexit(cleanup);
#else

   /* Sets up interface for cleanly shutting down the server */
   write_pid_file();
   signal(SIGINT, cleanup);
   signal(SIGTERM, cleanup);
   // SIGPIPE may cause trouble ;)
   signal(SIGPIPE, SIG_IGN);
#endif

   /* In case our backend API needs some initializing functions */
   initialize_audio();

#ifdef _WIN32
   	printf(	"==============================================================================\n"
			":: RSD server : Win32 : %s - Copyright (C) 2010-2011 Hans-Kristian Arntzen ::\n"
			"==============================================================================\n", RSD_VERSION);
#endif


   /* We're accepting two connects after each other, as we have one stream socket for audio data
      and one for controlling the server. Currently, the control socket is only useful for
      determining quickly when the client has hung up the connection. In case a control socket
      isn't supplied in a short time window (nmap, port scanners, etc),
      we shut down the connection. The connection, if accepted, will be handled in a new thread. */

   for(;;)
   {
      addr_size = sizeof (their_addr[0]);
      s_new = accept(s, u[0].addr, &addr_size);

      if ( s_new == -1 )
      {
         log_printf("Accepting failed... Errno: %d\n", errno);
         log_printf("%s\n", strerror( errno ) ); 
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
         s_ctl = accept(s, u[1].addr, &addr_size);
      }
      /* We didn't get a control socket, so we don't care about it :) 
         If s_ctl is 0, the backend will not perform any operations on it. */
      else 
      {
         if ( debug )
            log_printf("CTL-socket timed out. Ignoring CTL-socket. \n");

         s_ctl = 0;
      }

      if ( s_ctl == -1 )
      {
         close(s_new); s_new = -1;
         log_printf("%s\n", strerror( errno ) ); 
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
   union
   {
      struct sockaddr *sa;
      struct sockaddr_in *v4;
      struct sockaddr_in6 *v6;
   } u;

   u.sa = sa;
   /* Gotta love those & 'n * :D */

   if ( sa->sa_family == AF_INET ) 
   {
      return &((u.v4)->sin_addr);
   }
   else if ( sa->sa_family == AF_INET6 )
   {
      return &((u.v6)->sin6_addr);
   }
   return NULL;
}

// Hooray!
#ifdef _WIN32
static const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{
   union
   {
      struct sockaddr *sa;
      struct sockaddr_in *v4;
      struct sockaddr_in6 *v6;
   } u;

   if (af == AF_INET)
   {
      struct sockaddr_in in;
      memset(&in, 0, sizeof(in));
      in.sin_family = AF_INET;
      memcpy(&in.sin_addr, src, sizeof(struct in_addr));

      u.v4 = &in;
      getnameinfo(u.sa, sizeof(struct
               sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST);
      return dst;
   }
   else if (af == AF_INET6)
   {
      struct sockaddr_in6 in;
      memset(&in, 0, sizeof(in));
      in.sin6_family = AF_INET6;
      memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));

      u.v6 = &in;
      getnameinfo(u.sa, sizeof(struct
               sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST);
      return dst;
   }
   return NULL;
}
#endif

static int valid_ips( struct sockaddr_storage *their_addr )
{
   char remoteIP[2][INET6_ADDRSTRLEN] = { "", "" };

   inet_ntop(their_addr[0].ss_family, 
         get_addr(u[0].addr),
         remoteIP[0], INET6_ADDRSTRLEN);
   inet_ntop(their_addr[1].ss_family, 
         get_addr(u[0].addr),
         remoteIP[1], INET6_ADDRSTRLEN);


   if ( strcmp( remoteIP[0], remoteIP[1] ) != 0  )
   {
      log_printf("*** Warning: Got two connections from different sources. ***\n");
      log_printf("*** %s :: %s ***\n", remoteIP[0], remoteIP[1]);
      return -1;
   }

   log_message(remoteIP[1]);

   return 0;
}

static void log_message( const char * ip )
{
   char timestring[64] = {0};

   if ( verbose )
   {
      time_t cur_time;
      time(&cur_time);
      strftime(timestring, 63, "%Y-%m-%d - %H:%M:%S", localtime(&cur_time)); 
      log_printf("Connection :: [ %s ] [ %s ] ::\n", timestring, ip);
   }
}

