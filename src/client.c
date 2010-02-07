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
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "endian.h"
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>

#define HEADER_SIZE 44

static int raw_mode = 0;
static uint32_t raw_rate = 44100;
static uint16_t channel = 2;
static uint16_t bitsPerSample = 16;

static char port[128] = "12345";
static char host[128] = "localhost";

static int send_header_info(int);
static int get_backend_info(int, uint32_t*, uint32_t*);
static void print_help(char*);
static void parse_input(int, char**);


int main(int argc, char **argv)
{
   int s, rc;
   struct addrinfo hints, *res;
   uint32_t chunk_size, buffer_size;
   char *buffer;
   
   parse_input(argc, argv);
   
   memset(&hints, 0, sizeof( struct addrinfo ));
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   
   getaddrinfo(host, port, &hints, &res);
   
   s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   
   if ( connect(s, res->ai_addr, res->ai_addrlen) != 0 )
   {
      fprintf(stderr, "Error connecting to %s\n", host);
      exit(1);
   }
   
   freeaddrinfo(res);
  
      
   if ( send_header_info(s) == -1 )
   {
      fprintf(stderr, "Couldn't send WAV-info\n");
      exit(1);
   }

   /* buffer_size isn't in this program, since we don't care about blocking. */
   if ( !get_backend_info(s, &chunk_size, &buffer_size))
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

   while(1)
   {
      memset(buffer, 0, chunk_size);
      
      rc = read(0, buffer, chunk_size);
      if ( rc <= 0 )
      {
         close(s);
         exit(0);
      }

      rc = send(s, buffer, chunk_size, 0);
      if ( rc != (int)chunk_size && rc > 0 )
      {
         fprintf(stderr, "Sent not enough data ...\n");
      }
      else if ( rc == 0 )
      {
         fprintf(stderr, "Server closed connection ...\n");
         close(s);
         exit(0);
      }
      
   }
   
   return 0;
}
      
static int send_header_info(int s)
{
   /* If client is big endian, swaps over the data that the client sends if sending
    * premade WAV-header (--raw). 
    * Server expects little-endian, since WAV headers are of this format. */

   if (!is_little_endian())
   {
      swap_endian_16 ( &channel );
      swap_endian_32 ( &raw_rate );
      swap_endian_16 ( &bitsPerSample );
   }


   if ( !raw_mode )
   {
      char buffer[HEADER_SIZE] = {0};
      int rc = 0;
      
      read( 0, buffer, HEADER_SIZE );
      rc = send ( s, buffer, HEADER_SIZE, 0 );
      
      if ( rc == HEADER_SIZE )
      {
         return 1;
         
      }
      
      else
         return -1;
   }
   else
   {
      char buffer[HEADER_SIZE] = {0};
      int rc = 0;

#define RATE 24
#define CHANNEL 22
#define BITRATE 34

      *((uint32_t*)(buffer+RATE)) = raw_rate;
      *((uint16_t*)(buffer+CHANNEL)) = channel;
      *((uint16_t*)(buffer+BITRATE)) = 16;
      rc = send ( s, buffer, HEADER_SIZE, 0 );
      if ( rc == HEADER_SIZE )
      {
         return 1;
      }

      else
         return -1;
   }
}

static int get_backend_info(int socket, uint32_t* chunk_size, uint32_t* buffer_size)
{
   uint32_t chunk_size_temp, buffer_size_temp;
   int rc;
   int socket_buffer_size;

   rc = recv(socket, &chunk_size_temp, sizeof(uint32_t), 0);
   if ( rc != sizeof(uint32_t))
   {
      return 0;
   }
   rc = recv(socket, &buffer_size_temp, sizeof(uint32_t), 0);
   if ( rc != sizeof(uint32_t))
   {
      return 0;
   }

   chunk_size_temp = ntohl(chunk_size_temp);
   buffer_size_temp = ntohl(buffer_size_temp);

   *chunk_size = chunk_size_temp;
   *buffer_size = buffer_size_temp;
   
   socket_buffer_size = (int)chunk_size_temp * 4;
   if ( setsockopt(socket,SOL_SOCKET,SO_SNDBUF,&socket_buffer_size,sizeof(int)) == -1 )
   {
      perror("setsockopt");
      close(socket);
      exit(1);
   }

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
            print_help(argv[0]);
            exit(1);

         case 'h':
            print_help(argv[1]);
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


