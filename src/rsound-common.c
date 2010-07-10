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

/************************************************************** 
 *   This file defines some backend independent functions      *
 ***************************************************************/

#include "rsound.h"
#include "config.h"
#include "endian.h"
#include "audio.h"
#include "proto.h"

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#else
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <pthread.h>
#endif

#include <signal.h>
#include <getopt.h>
#include <poll.h>

/* Not really portable, need to find something better */
#define PIDFILE "/tmp/.rsound.pid"

/* Pulls in callback structs depending on compilation options. */

#ifndef _WIN32
#ifdef _ALSA
extern const rsd_backend_callback_t rsd_alsa;
#endif

#ifdef _OSS
extern const rsd_backend_callback_t rsd_oss;
#endif

#ifdef _AO
extern const rsd_backend_callback_t rsd_ao;
#endif

#ifdef _PORTA
extern const rsd_backend_callback_t rsd_porta;
#endif

#ifdef _AL
extern const rsd_backend_callback_t rsd_al;
#endif

#ifdef _MUROAR
extern const rsd_backend_callback_t rsd_muroar;
#endif

#ifdef _PULSE
extern const rsd_backend_callback_t rsd_pulse;
#endif
#endif


#define MAX_PACKET_SIZE 1024



static void print_help(void);
static void* rsd_thread(void*);
static int recieve_data(void*, connection_t*, char*, size_t);

/* Writes a file with the process id, so that a subsequent --kill can kill it cleanly. */
#ifndef _WIN32
void write_pid_file(void)
{
   FILE *pidfile = fopen(PIDFILE, "a");
   if ( pidfile )
   {
      fprintf(pidfile, "%d\n", (int)getpid());
      fclose(pidfile);
   }
}
#endif

void initialize_audio ( void )
{
   if ( backend->initialize )
      backend->initialize();
}

#ifdef _WIN32
void cleanup( void )
{
	extern int listen_socket;

	if ( listen_socket > 0 )
		closesocket(listen_socket);

	if ( backend->shutdown )
		backend->shutdown();

	WSACleanup();
}
#else
void cleanup( int signal )
{
   extern int listen_socket;
   (void) signal;
   if ( listen_socket > 0 )
      close(listen_socket);

   fprintf(stderr, " --- Recieved signal, cleaning up ---\n");
   unlink(PIDFILE);

   if ( rsd_conn_type == RSD_CONN_UNIX )
      unlink(unix_sock);

   if ( backend->shutdown )
      backend->shutdown();
   exit(0);
}
#endif

void new_sound_thread ( connection_t connection )
{
#ifndef _WIN32
   static pthread_t last_thread = 0;
#endif
   pthread_t thread;

   connection_t *conn = calloc(1, sizeof(*conn));
   conn->socket = connection.socket;
   conn->ctl_socket = connection.ctl_socket;

   /* Prefer non-blocking sockets due to its consistent performance with poll()/recv() */
#ifdef _WIN32
   u_long iMode = 1;
	ioctlsocket(conn->socket, FIONBIO, &iMode);
	iMode = 1;
	if ( conn->ctl_socket > 0 )
		ioctlsocket(conn->ctl_socket, FIONBIO, &iMode);
#else
   if ( fcntl(conn->socket, F_SETFL, O_NONBLOCK) < 0)
   {
      fprintf(stderr, "Setting non-blocking socket failed.\n");
      goto error;
   }

   if ( conn->ctl_socket > 0 )
   {
      if ( fcntl(conn->ctl_socket, F_SETFL, O_NONBLOCK) < 0)
      {
         fprintf(stderr, "Setting non-blocking socket failed.\n");
         goto error;
      }
   }
#endif

#ifndef _WIN32
   /* If we're not using serveral threads, we must wait for the last thread to join. */
   if ( no_threading && last_thread != 0 )
      pthread_join(last_thread, NULL);
#endif

   /* Creates new thread */
   if ( pthread_create(&thread, NULL, rsd_thread, (void*)conn) < 0 )
   {
      fprintf(stderr, "Creating thread failed ...\n");
      goto error;
   }

   /* If we're not going to join, we need to detach it. */
#ifndef _WIN32
   if ( !no_threading )
#endif
      pthread_detach(thread);

   /* Sets the static variable for use with next new_sound_thread() call. */
#ifndef _WIN32
   last_thread = thread;
#endif
   return;

   /* Cleanup if fcntl failed. */
error:
   close(conn->socket);
   close(conn->ctl_socket);
   free(conn);
}

/* getopt command-line parsing. Sets the variables declared in daemon.c */
void parse_input(int argc, char **argv)
{
#ifndef _WIN32
   FILE *pidfile;
   int pid;
#endif

   int c, option_index = 0;

   struct option opts[] = {
      { "port", 1, NULL, 'p' },
      { "help", 0, NULL, 'h' },
      { "verbose", 0, NULL, 'v' },
      { "debug", 0, NULL, 'B' },
      { "bind", 1, NULL, 'H' },
#ifndef _WIN32
      { "backend", 1, NULL, 'b' },
      { "sock", 1, NULL, 'S' },
      { "kill", 0, NULL, 'K' },
      { "single", 0, NULL, 'T' },
      { "device", 1, NULL, 'd' },
      { "daemon", 0, NULL, 'D' },
#endif
      { NULL, 0, NULL, 0 }
   };

#ifdef _WIN32
   char optstring[] = "p:vh";
#else
   char optstring[] = "d:b:p:Dvh";
#endif

   for(;;)
   {
      c = getopt_long ( argc, argv, optstring, opts, &option_index );

      if ( c == -1 )
         break;

      switch ( c )
      {
#ifndef _WIN32
         case 'd':
            strncpy(device, optarg, 127);
            device[127] = 0;
            break;
#endif

         case 'p':
            strncpy(port, optarg, 127);
            port[127] = 0;
            break;

         case 'H':
            strncpy(bindaddr, optarg, 127);
            bindaddr[127] = 0;
            break;

         case '?':
            print_help();
            exit(1);

         case 'h':
            print_help();
            exit(0);

#ifndef _WIN32
         case 'T':
            no_threading = 1;
            break;
#endif
#ifndef _WIN32
         case 'K':
            pidfile = fopen(PIDFILE, "r");
            if ( pidfile )
            {
               int valid = 1;
               pid_t pidlist[1024];
               int i, index = 0;
               while ( !feof(pidfile) && valid && index < 1024 )
               {
                  if ( fscanf(pidfile, "%d", &pid) == 1 )
                     pidlist[index++] = (pid_t)pid;
                  else
                     valid = 0;
               }

               fclose(pidfile);

               for ( i = 0; i < index; i++ )
                  kill(pidlist[i], SIGTERM);

               exit(0);
            }
            else
            {
               fprintf(stderr, "Couldn't open PID file.\n");
               exit(1);
            }

            break;
#endif

#ifndef _WIN32
         case 'b':
#ifdef _ALSA
            if ( !strcmp( "alsa", optarg ) )
            {
               backend = &rsd_alsa;
               break;
            }  
#endif
#ifdef _OSS
            if ( !strcmp( "oss", optarg ) )
            {
               backend = &rsd_oss;
               break;
            }
#endif
#ifdef _AO
            if ( !strcmp( "libao", optarg ) )
            {
               backend = &rsd_ao;
               break;
            }
#endif
#ifdef _PORTA
            if ( !strcmp( "portaudio", optarg ) )
            {
               backend = &rsd_porta;
               break;
            }
#endif
#ifdef _AL
            if ( !strcmp( "openal", optarg ) )
            {
               backend = &rsd_al;
               break;
            }
#endif
#ifdef _MUROAR
            if ( !strcmp( "muroar", optarg) )
            {
               backend = &rsd_muroar;
               break;
            }
#endif
#ifdef _PULSE
            if ( !strcmp( "pulse", optarg) )
            {
               backend = &rsd_pulse;
               break;
            }
#endif

            fprintf(stderr, "\nValid backend not given. Exiting ...\n\n");
            print_help();
            exit(1);
#endif

#ifndef _WIN32
         case 'D':
            daemonize = 1;
            break;
#endif

         case 'v':
            verbose = 1;
            break;

         case 'B':
            debug = 1;
            verbose = 1;
            break;

#ifndef _WIN32
         case 'S':
            strncpy(unix_sock, optarg, 127);
            unix_sock[127] = '\0';
            rsd_conn_type = RSD_CONN_UNIX;
            break;
#endif

         default:
            fprintf(stderr, "Error parsing arguments.\n");
            exit(1);
      }


   }

   if ( optind < argc )
   {
      print_help();
      exit(1);
   }

#ifndef _WIN32
   if ( backend == NULL )
   {

      /* Select a default backend if nothing was specified on the command line. */

#ifdef __CYGWIN__
      /* We prefer portaudio if we're in Windows. */
#ifdef _PORTA
      backend = &rsd_porta;
#elif _AL
      backend = &rsd_al;
#elif _AO
      backend = &rsd_ao;
#elif _OSS
      backend = &rsd_oss;
#endif
#else
#ifdef _ALSA
      backend = &rsd_alsa;
#elif _OSS
      backend = &rsd_oss;
#elif _AL
      backend = &rsd_al;
#elif _PORTA
      backend = &rsd_porta;
#elif _PULSE
      backend = &rsd_pulse;
#elif _AO
      backend = &rsd_ao;
#elif _MUROAR
      backend = &rsd_muroar;
#endif

#endif

   }
#endif


   /* Shouldn't really happen, but ... */
   if ( backend == NULL )
   {
      fprintf(stderr, "rsd was not compiled with any output support, exiting ...");
      exit(1);
   }

}

static void print_help()
{
   printf("rsd - version %s - Copyright (C) 2010 Hans-Kristian Arntzen\n", RSD_VERSION);
   printf("==========================================================================\n");
   printf("Usage: rsd [ -d/--device | -b/--backend | -p/--port | --bind | -D/--daemon | -v/--verbose | --debug | -h/--help | --single | --kill ]\n");
   printf("\n-d/--device: Specifies an ALSA or OSS device to use.\n");
   printf("  Examples:\n\t-d hw:1,0\n\t-d /dev/audio\n\t"
         "    Defaults to \"default\" for alsa and /dev/dsp for OSS\n");

   printf("\n-b/--backend: Specifies which audio backend to use.\n");
   printf("Supported backends: ");

#ifdef _PULSE
   printf("pulse ");
#endif
#ifdef _ALSA
   printf("alsa ");
#endif
#ifdef _OSS
   printf("oss ");
#endif
#ifdef _AO
   printf("libao ");
#endif
#ifdef _PORTA
   printf("portaudio ");
#endif
#ifdef _AL
   printf("openal ");
#endif
#ifdef _MUROAR
   printf("muroar ");
#endif

   putchar('\n');
   putchar('\n');

   printf("-D/--daemon: Runs as daemon.\n");
   printf("-p/--port: Defines which port to listen on.\n");
   printf("--bind: Defines which address to bind to. Default is 0.0.0.0.\n");
   printf("\tExample: -p 18453. Defaults to port 12345.\n");
   printf("-v/--verbose: Enables verbosity\n");
   printf("-h/--help: Prints this help\n\n");
   printf("--debug: Enable more verbosity\n");
   printf("--single: Only allows a single connection at a time.\n");
   printf("--kill: Cleanly shuts downs the running rsd process.\n");
}

static void pheader(wav_header_t *w)
{
   fprintf(stderr, "============================================\n");
   fprintf(stderr, "WAV header:\n");

   if (w->numChannels == 1)
      fprintf(stderr, "  Mono | ");
   else if (w->numChannels == 2)
      fprintf(stderr, "  Stereo | ");
   else
      fprintf(stderr, "  Multichannel | ");

   fprintf(stderr, "%d / ", w->sampleRate);
   fprintf(stderr, "%s\n", rsnd_format_to_string(w->rsd_format));

   fprintf(stderr, "============================================\n\n");
}

/* Reads raw 44 bytes WAV header from client and parses this (naive approach) */
static int get_wav_header(connection_t conn, wav_header_t* head)
{

   /* Are we on a little endian system?
    * WAV files are little-endian. If server is big-endian, swaps over data to get sane results. */
   int i = is_little_endian();
   uint16_t temp16;
   uint32_t temp32;
   uint16_t temp_format;
   uint16_t pcm;

   int rc = 0;
   char header[HEADER_SIZE] = {0};

   rc = recieve_data(NULL, &conn, header, HEADER_SIZE);
   if ( rc != HEADER_SIZE )
   {
      fprintf(stderr, "Didn't read enough data for WAV header.");
      return -1;
   }

   /* Since we can't really rely on that the compiler doesn't pad our structs in funny ways (portability ftw), we need to do it this
      horrid way. :v */

   // Defines Positions in the WAVE header
#define PCM 20
#define CHANNELS 22
#define RATE 24
#define BITS_PER_SAMPLE 34

   /* This is not part of the WAV standard, but since we're ignoring these useless bytes at the end to begin with, why not?
      If this is 0 (RSD_UNSPEC) or some undefined value, we assume the default of S16_LE for 16 bit and U8 for 8bit. (We can assume that the client is using an old version of librsound since it sets 0
      by default in the header. */
#define FORMAT 42


   temp16 = *((uint16_t*)(header+CHANNELS));
   if (!i)
      swap_endian_16 ( &temp16 );
   head->numChannels = temp16;

   temp32 = *((uint32_t*)(header+RATE));
   if (!i)
      swap_endian_32 ( &temp32 );
   head->sampleRate = temp32;

   temp16 = *((uint16_t*)(header+BITS_PER_SAMPLE));
   if (!i)
      swap_endian_16 ( &temp16 );
   head->bitsPerSample = temp16;

   temp16 = *((uint16_t*)(header+FORMAT));
   if (!i)
      swap_endian_16 ( &temp16 );
   temp_format = temp16;

   temp16 = *((uint16_t*)(header+PCM));
   if (!i)
      swap_endian_16 ( &temp16 );
   pcm = temp16;

   // Checks bits to get a default should the format not be set.
   switch ( head->bitsPerSample )
   {
      case 16:
         head->rsd_format = RSD_S16_LE;
         break;
      case 8:
         head->rsd_format = RSD_U8;
         break;
      default:
         head->rsd_format = RSD_S16_LE;
   }

   if ( !pcm ) // Are we out of spec?
   {
      // If format is set to some defined value, use that instead.
      switch ( temp_format )
      {
         case RSD_S16_BE:
            if ( head->bitsPerSample == 16 )
               head->rsd_format = RSD_S16_BE;
            break;

         case RSD_U16_LE:
            if ( head->bitsPerSample == 16 )
               head->rsd_format = RSD_U16_LE;
            break;

         case RSD_U16_BE:
            if ( head->bitsPerSample == 16 )
               head->rsd_format = RSD_U16_BE;
            break;

         case RSD_S8:
            if ( head->bitsPerSample == 8 )
               head->rsd_format = RSD_S8;
            break;

         default:
            break;
      }
   }
   else if ( pcm == 6 ) // Magic key for ALAW
   {
      if ( head->bitsPerSample == 8 )
         head->rsd_format = RSD_ALAW;
   }
   else if ( pcm == 7 ) // Magic key for MULAW
   {
      if ( head->bitsPerSample == 8 )
         head->rsd_format = RSD_MULAW;
   }


   /* Checks some basic sanity of header file */
   if ( head->sampleRate <= 0 || head->sampleRate > 192000 || head->bitsPerSample % 8 != 0 || head->bitsPerSample == 0 )
   {
      fprintf(stderr, "Got garbage header data ...\n");
      fprintf(stderr, "Channels: %d, Samplerate: %d, Bits/sample: %d\n",
            (int)head->numChannels, (int)head->sampleRate, (int)head->bitsPerSample );
      return -1;
   }

   return 0;
}

static int send_backend_info(connection_t conn, backend_info_t *backend )
{

   // Magic 8 bytes that server sends to the client.
   // If we send 16 bytes, the first 8 bytes with 8 empty bytes after, 
   // it means that we can accept stuff on the ctl socket, which is used for delay measurements! :D
   // The old version of the server simply closed down the connection when something happened to the control socket.
   // This is mostly a backwards compatible solution.
#define RSND_HEADER_SIZE 16
#define LATENCY 0
#define CHUNKSIZE 1

   int rc = 0;
   struct pollfd fd;

   // 16 byte header
   uint32_t header[4] = {0};

   /* Again, padding ftw */
   // Client uses server side latency for delay calculations.
   header[LATENCY] = backend->latency;
   // Preferred TCP packet size. (Fragsize for audio backend. Might be ignored by client.)
   header[CHUNKSIZE] = backend->chunk_size;

   // For some reason, htonl was borked. :<
   if ( is_little_endian() )
   {
      swap_endian_32(&header[LATENCY]);
      swap_endian_32(&header[CHUNKSIZE]);
   }


   fd.fd = conn.socket;
   fd.events = POLLOUT;

   if ( poll(&fd, 1, 10000) < 0 )
      return -1;
   if ( fd.revents & POLLHUP )
      return -1;
   if ( fd.revents & POLLOUT )
      rc = send(conn.socket, (char*)header, RSND_HEADER_SIZE, 0);
   if ( rc != RSND_HEADER_SIZE)
      return -1;

   // RSD will no longer use this for writing
#ifdef _WIN32
   shutdown(conn.socket, SD_SEND);
#else
   shutdown(conn.socket, SHUT_WR);
#endif

   return 0;
}

/* Sets up listening socket for further use */
int set_up_socket()
{

#ifdef _WIN32
   WSADATA wsaData;
	int retval;

	if ((retval = WSAStartup(MAKEWORD(2,2), &wsaData)) != 0)
	{
		fprintf(stderr, "Error starting Winsock\n");
		WSACleanup();
		exit(1);
	}
#endif

   int rc;
   int s;
   struct addrinfo hints, *servinfo = NULL;
#ifndef _WIN32
   struct sockaddr_un un;
#endif
   int yes = 1;

   memset(&hints, 0, sizeof (struct addrinfo));
#if defined (__CYGWIN__) || defined (_WIN32)
   /* Because Windows apparently fails, and doesn't like AF_UNSPEC. */
   hints.ai_family = AF_INET;
#else
   hints.ai_family = AF_UNSPEC;
#endif
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;

#ifndef _WIN32
   if ( rsd_conn_type == RSD_CONN_UNIX )
   {
      servinfo = &hints;
      servinfo->ai_family = AF_UNIX;
      servinfo->ai_protocol = 0;
      servinfo->ai_addr = (struct sockaddr*)&un;
      servinfo->ai_addrlen = sizeof(un);
      un.sun_family = servinfo->ai_family;
      strncpy(un.sun_path, unix_sock, sizeof(un.sun_path)); // Should use strncpy
      un.sun_path[sizeof(un.sun_path)-1] = '\0';
   }
   else
#endif
   {
      const char *bind = NULL;
      if ( strlen(bindaddr) > 0 )
         bind = bindaddr;

      if ((rc = getaddrinfo(bind, port, &hints, &servinfo)) != 0)
      {
         fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
         return -1;
      }
   }

   if ( debug )
   {
#ifndef _WIN32
      if ( servinfo->ai_family == AF_UNIX )
         fprintf(stderr, "Creating domain socket: %s\n", unix_sock);
      else
#endif
         fprintf(stderr, "Binding on port %s\n", port);
   }

   s = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
   if ( s == -1 )
   {
      fprintf(stderr, "Error getting socket\n");
      goto error;
   }

#ifndef _WIN32
   if ( servinfo->ai_family != AF_UNIX )
#endif
   {
      if ( setsockopt(s, SOL_SOCKET, SO_REUSEADDR, CONST_CAST &yes, sizeof(int)) == -1) 
      {
         perror("setsockopt");
         goto error;
      }
   }

   rc = bind(s, servinfo->ai_addr, servinfo->ai_addrlen);
   if ( rc == -1 )
   {
#ifndef _WIN32
      if ( servinfo->ai_family == AF_UNIX )
         fprintf(stderr, "Error creating domain socket: %s\n", unix_sock);
      else
#endif
         fprintf(stderr, "Error binding on port %s.\n", port);
      goto error;
   }

#ifdef _WIN32
   if ( servinfo != NULL )
      freeaddrinfo(servinfo);
   return s;
#else
   if ( servinfo != NULL && servinfo->ai_family != AF_UNIX )
      freeaddrinfo(servinfo);
   return s;
#endif

   /* In case something goes to hell. */
error:
#ifdef _WIN32
   if ( servinfo != NULL )
      freeaddrinfo(servinfo);
   return -1;
#else
   if ( servinfo != NULL && servinfo->ai_family != AF_UNIX )
      freeaddrinfo(servinfo);
   return -1;
#endif

}

/* Makes sure that size data is recieved in full. Else, returns a 0. 
   Old protocol: If the control socket is set, this is a sign that it has been closed (for some reason),
   which currently means that we should stop the connection immediately.
   New protocol: If the control socket is set, we should handle it! */

static int recieve_data(void *data, connection_t *conn, char* buffer, size_t size)
{
   int rc;
   size_t read = 0;
   struct pollfd fd[2];
   size_t read_size;
   fd[0].fd = conn->socket;
   fd[0].events = POLLIN;
   fd[1].fd = conn->ctl_socket;
   fd[1].events = POLLIN;

   // Will not check ctl_socket if it's never used.
   int fds; 

   while ( read < size )
   {
      // We check this in a loop since ctl_socket might change in handle_ctl_request().
      if ( conn->ctl_socket > 0 )
         fds = 2;
      else
         fds = 1;

      if ( poll(fd, fds, 1000) < 0)
         return 0;

      // If POLLIN is active on ctl socket handle this request, or if POLLHUP, shut the stream down.
      if ( fds == 2 )
      {
         if ( fd[1].revents & POLLHUP )
            return 0;

         else if ( fd[1].revents & POLLIN )
         {
            // We will handle a ctl request from the client. This request should never block.
            if ( data != NULL && handle_ctl_request(conn, data) < 0 )
            {
               return 0;
            }
         }
      }

      if ( fd[0].revents & POLLHUP )
         return 0;

      else if ( fd[0].revents & POLLIN )
      {
         read_size = size - read > MAX_PACKET_SIZE ? MAX_PACKET_SIZE : size - read;
         rc = recv(conn->socket, buffer + read, read_size, 0);
         if ( rc <= 0 )
            return 0;

         read += rc;
      }

   }

   return read;
}

/* All and mighty connection handler. */
static void* rsd_thread(void *thread_data)
{
   connection_t conn;
   void *data = NULL;
   wav_header_t w;
   int rc, written;
   char *buffer = NULL;

   connection_t *temp_conn = thread_data;
   conn.socket = temp_conn->socket;
   conn.ctl_socket = temp_conn->ctl_socket;
   conn.serv_ptr = 0;
   conn.identity[0] = '\0';
   free(temp_conn);

   if ( debug )
      fprintf(stderr, "Connection accepted, awaiting WAV header data ...\n");

   /* Firstly, get the wave header with stream settings. */
   rc = get_wav_header(conn, &w);
   if ( rc == -1 )
   {
      close(conn.socket);
      close(conn.ctl_socket);
      fprintf(stderr, "Couldn't read WAV header... Disconnecting.\n");
      pthread_exit(NULL);
   }

   if ( debug )
   {
      fprintf(stderr, "Successfully got WAV header ...\n");
      pheader(&w);
   }

   if ( debug )
      fprintf(stderr, "Initializing %s ...\n", backend->backend);

   /* Sets up backend */
   if ( backend->init(&data) < 0 )
   {
      fprintf(stderr, "Failed to initialize %s ...\n", backend->backend);
      goto rsd_exit;
   }

   /* Opens device with settings. */
   if ( backend->open(data, &w) < 0 )
   {
      fprintf(stderr, "Failed to open audio driver ...\n");
      goto rsd_exit;
   }

   backend_info_t backend_info; 
   backend->get_backend_info(data, &backend_info);
   if ( backend_info.latency == 0 || backend_info.chunk_size == 0 )
   {
      fprintf(stderr, "Couldn't get backend info ...\n");
      goto rsd_exit;
   }

   // We only bother with setting buffer size if we're doing TCP.
   if ( rsd_conn_type == RSD_CONN_TCP )
   {
      int flag = 1;
      int bufsiz = backend_info.chunk_size * 32;
      setsockopt(conn.socket, SOL_SOCKET, SO_RCVBUF, CONST_CAST &bufsiz, sizeof(int));

      if ( conn.ctl_socket )
      {
         setsockopt(conn.ctl_socket, SOL_SOCKET, SO_RCVBUF, CONST_CAST &bufsiz, sizeof(int));
         setsockopt(conn.ctl_socket, SOL_SOCKET, SO_SNDBUF, CONST_CAST &bufsiz, sizeof(int));
         setsockopt(conn.ctl_socket, IPPROTO_TCP, TCP_NODELAY, CONST_CAST &flag, sizeof(int));
      }

      setsockopt(conn.socket, IPPROTO_TCP, TCP_NODELAY, CONST_CAST &flag, sizeof(int));
   }

   /* Now we can send backend info to client. */
   if ( send_backend_info(conn, &backend_info) < 0 )
   {
      fprintf(stderr, "Failed to send backend info ...\n");
      goto rsd_exit;
   }

   if ( debug )
      fprintf(stderr, "Initializing of %s successful ...\n", backend->backend);

   size_t size = backend_info.chunk_size;
   buffer = malloc(size);

   if ( buffer == NULL )
   {
      fprintf(stderr, "Couldn't allocate memory for buffer ...\n");
      goto rsd_exit;
   }

   /* Recieve data, write to sound card. Rinse, repeat :') */
   for(;;)
   {
      if ( strlen(conn.identity) > 0 && verbose )
      {
         fprintf(stderr, " :: %s\n", conn.identity);
         conn.identity[0] = '\0';
      }

      memset(buffer, 0, size);

      rc = recieve_data(data, &conn, buffer, size);
      if ( rc == 0 )
      {
         if ( debug )
            fprintf(stderr, "Client closed connection.\n");
         goto rsd_exit;
      }

      for ( written = 0; written < (int)size; )
      {
         rc = backend->write(data, buffer + written, size - written);
         if ( rc <= 0 )
         {
            if ( debug )
               fprintf(stderr, "write() failed with error code %d.\n", rc);
            goto rsd_exit;
         }

         written += rc;
         conn.serv_ptr += rc;
      }
   }

   /* Cleanup */
rsd_exit:
   if ( debug )
      fprintf(stderr, "Closed connection.\n\n");
#ifdef _WIN32
#undef close
   backend->close(data);
#define close(x) closesocket(x)
#endif
   free(buffer);
   free(data);
   close(conn.socket);
   if ( conn.ctl_socket )
      close(conn.ctl_socket);
   pthread_exit(NULL);
}

