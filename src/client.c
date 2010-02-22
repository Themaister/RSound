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

#define _POSIX_SOURCE
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "endian.h"
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#define HEADER_SIZE 44
#define MAX_PACKET_SIZE 1024

static int raw_mode = 0;
static uint32_t raw_rate = 44100;
static uint16_t channel = 2;
static uint16_t bitsPerSample = 16;

static char port[128] = "12345";
static char host[128] = "localhost";

static int send_header_info(int, uint16_t, uint32_t);
static int get_backend_info(int, uint32_t*, uint32_t*);
static void cancel_stream(int);
static void print_help(char*);
static void parse_input(int, char**);
static struct pollfd fd;

static uint32_t threadId;


int main(int argc, char **argv)
{
   int s, rc;
   struct addrinfo hints, *res;
   uint32_t chunk_size = 0;
   char *buffer;
   size_t sent;
   size_t send_size;
   
   parse_input(argc, argv);
   
   memset(&hints, 0, sizeof( struct addrinfo ));
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   
   getaddrinfo(host, port, &hints, &res);
   
   s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   fd.fd = s;
   
   if ( connect(s, res->ai_addr, res->ai_addrlen) != 0 )
   {
      fprintf(stderr, "Error connecting to %s\n", host);
      exit(1);
   }
   
   freeaddrinfo(res);

   if ( fcntl(s, F_SETFL, O_NONBLOCK) < 0)
   {
      fprintf(stderr, "Couldn't set socket to non-blocking ...\n");
      exit(1);
   }
  
   if ( send_header_info(s, 0, 0) == -1 )
   {
      fprintf(stderr, "Couldn't send WAV-info\n");
      close(s);
      exit(1);
   }

   if ( !get_backend_info(s, &chunk_size, &threadId))
   {
      fprintf(stderr, "Server closed connection.\n");
      close(s);
      exit(1);
   }

   buffer = malloc ( chunk_size );
   if ( !buffer )
   {
      fprintf(stderr, "Couldn't allocate memory for buffer.\n");
      close(s);
      exit(1);
   }

   signal(SIGTERM, cancel_stream);
   signal(SIGINT, cancel_stream);

   while(1)
   {
      memset(buffer, 0, chunk_size);
      
      rc = read(0, buffer, chunk_size);
      if ( rc <= 0 )
      {
         close(s);
         exit(0);
      }

      sent = 0;
      fd.events = POLLOUT;
      while ( sent < chunk_size )
      {
         if ( poll(&fd, 1, 500) < 0 )
         {
            free(buffer);
            exit(1);
         }

         if ( fd.revents == POLLHUP )
         {
            fprintf(stderr, "Server closed connection.\n");
            free(buffer);
            exit(1);
         }

         send_size = (chunk_size - sent > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : chunk_size - sent;
         rc = send(s, buffer + sent, send_size, 0);

         if ( rc <= 0 )
         {
            fprintf(stderr, "Server closed connection.\n");
            free(buffer);
            exit(1);
         }

         sent += rc;
      }
      
   }
   
   return 0;
}
      
static int send_header_info(int s, uint16_t stream_type, uint32_t threadId)
{
   /* If client is big endian, swaps over the data that the client sends if sending
    * premade WAV-header (--raw). 
    * Server expects little-endian, since WAV headers are of this format. */

   int rc;
   char buffer[HEADER_SIZE] = {0};

   if (!is_little_endian())
   {
      swap_endian_16 ( &channel );
      swap_endian_32 ( &raw_rate );
      swap_endian_16 ( &bitsPerSample );
   }

   
   if ( !raw_mode )
   {  
      rc = read( 0, buffer, HEADER_SIZE );
      if ( rc != HEADER_SIZE )
         return -1;

      *((uint16_t*)buffer) = htons(stream_type);
      *((uint32_t*)(buffer+4)) = htons(threadId);

      fd.events = POLLOUT;
      if ( poll(&fd, 1, 500) < 0 )
         return -1;

      if ( fd.revents == POLLHUP )
         return -1;
      
      rc = send ( s, buffer, HEADER_SIZE, 0 );
      
      if ( rc == HEADER_SIZE )
         return 1;
      else
         return -1;
   }
   else
   {

#define RATE 24
#define CHANNEL 22
#define BITRATE 34

      *((uint16_t*)buffer) = htons(stream_type);
      *((uint32_t*)(buffer+4)) = htons(threadId);
      *((uint32_t*)(buffer+RATE)) = raw_rate;
      *((uint16_t*)(buffer+CHANNEL)) = channel;
      *((uint16_t*)(buffer+BITRATE)) = 16;

      fd.events = POLLOUT;
      if ( poll(&fd, 1, 500) < 0 )
         return -1;

      if ( fd.revents == POLLHUP )
         return -1;

      rc = send ( s, buffer, HEADER_SIZE, 0 );
      if ( rc == HEADER_SIZE )
         return 1;
      else
         return -1;
   }
}

static int get_backend_info(int socket, uint32_t* chunk_size, uint32_t* threadId)
{
   uint32_t chunk_size_temp, threadId_temp;
   int rc;
   int socket_buffer_size;
   fd.events = POLLIN;
   if ( poll(&fd, 1, 500) < 0 )
      return -1;

   if ( fd.revents == POLLHUP )
      return -1;

   rc = recv(socket, &chunk_size_temp, sizeof(uint32_t), 0);
   if ( rc != sizeof(uint32_t))
   {
      return 0;
   }

   if ( poll(&fd, 1, 500) < 0 )
      return -1;

   if ( fd.revents == POLLHUP )
      return -1;

   rc = recv(socket, &threadId_temp, sizeof(uint32_t), 0);
   if ( rc != sizeof(uint32_t))
   {
      return 0;
   }
   *chunk_size = ntohl(chunk_size_temp);
   *threadId = ntohl(threadId_temp);
   
   return 1;

}

static void print_help(char *appname)
{
   printf("Usage: %s [ <hostname> | -p/--port | -h/--help | --raw | -r/--rate | -c/--channels ]\n", appname);
   
   printf("\n%s reads PCM data only via stdin and sends this data directly to a rsoundserv.\n", appname); 
   printf("Unless specified with --raw, %s expects a valid WAV header to be present in the input stream.\n\n", appname);
   printf(" Examples:\n"); 
   printf("\t%s foo.net < bar.wav\n", appname);
   printf("\tcat bar.wav | %s foo.net -p 4322 --raw -r 48000 -c 2\n\n", appname);
   printf("With eg. -ao pcm:file, MPlayer or similar programs can stream audio to rsoundserv via FIFO pipes or stdout\n\n");
   
   printf("-p/--port: Defines which port to connect to.\n");
   printf("\tExample: -p 18453. Defaults to port 12345.\n");
   printf("--raw: Enables raw PCM input. When using --raw, %s will generate a fake WAV header\n", appname);
   printf("-r/--rate: Defines input samplerate (raw PCM)\n");
   printf("\tExample: -r 48000. Defaults to 44100\n");
   printf("-c/--channel: Specifies number of sound channels (raw PCM)\n");
   printf("\tExample: -c 1. Defaults to stereo (2)\n");
   printf("-h/--help: Prints this help\n\n");
}

static void parse_input(int argc, char **argv)
{

   char *program_name;
   int c, option_index = 0;

   struct option opts[] = {
      { "raw", 0, NULL, 'R' },
      { "port", 1, NULL, 'p' },
      { "help", 0, NULL, 'h'},
      { "rate", 1, NULL, 'r'},
      { "channels", 1, NULL, 'c'},
      { NULL, 0, NULL, 0 }
   };

   char optstring[] = "r:p:hc:";
   program_name = malloc(strlen(argv[0] + 1));
   if ( program_name == NULL )
   {
      fprintf(stderr, "Error allocating memory.\n");
      exit(1);
   }
   strcpy(program_name, argv[0]);

   while ( 1 )
   {
      c = getopt_long ( argc, argv, optstring, opts, &option_index );

      if ( c == -1 )
         break;

      switch ( c )
      {
         case 'r':
            raw_rate = atoi(optarg);
            break;

         case 'R':
            raw_mode = 1;
            break;

         case 'p':
            strncpy(port, optarg, 127);
            port[127] = 0;
            break;

         case 'c':
            channel = atoi(optarg);
            break;

         case '?':
            print_help(program_name);
            free(program_name);
            exit(1);

         case 'h':
            print_help(program_name);
            free(program_name);
            exit(0);

         default:
            fprintf(stderr, "Error in parsing arguments.\n");
            exit(1);
      }

   }

   if ( optind < argc )
   {
      while ( optind < argc )
      {
         strncpy(host, argv[optind++], 127);
         host[127] = 0;
      }
   }
}

static void cancel_stream(int signal)
{
   int s, rc;
   struct addrinfo hints, *res;
 
   memset(&hints, 0, sizeof( struct addrinfo ));
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   
   getaddrinfo(host, port, &hints, &res);
   
   s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   fd.fd = s;
   
   if ( connect(s, res->ai_addr, res->ai_addrlen) != 0 )
   {
      fprintf(stderr, "Error connecting to %s\n", host);
      exit(1);
   }
   
   freeaddrinfo(res);

   if ( fcntl(s, F_SETFL, O_NONBLOCK) < 0)
   {
      fprintf(stderr, "Couldn't set socket to non-blocking ...\n");
      exit(1);
   }
  
   if ( send_header_info(s, 1, threadId) == -1 )
   {
      fprintf(stderr, "Couldn't send WAV-info\n");
      close(s);
      exit(1);
   }
   exit(0);
}
