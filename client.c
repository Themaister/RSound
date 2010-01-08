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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "endian.h"
#include <stdint.h>

#define HEADER_SIZE 44

#define DEFAULT_CHUNK_SIZE 1024

int raw_mode = 0;
uint32_t raw_rate = 44100;
uint16_t channel = 2;
uint16_t bitsPerSample = 16;

char port[6] = "12345";
char host[128] = "localhost";

int send_header_info(int);
int get_backend_info(int, uint32_t*, uint32_t*);
void print_help(char*);
void parse_input(int, char**);


int main(int argc, char **argv)
{
   parse_input(argc, argv);
   
   int s, rc;
   struct addrinfo hints, *res;
   memset(&hints, 0, sizeof( struct addrinfo ));
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   
   getaddrinfo(host, port, &hints, &res);
   
   s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   
   if ( connect(s, res->ai_addr, res->ai_addrlen) != 0 )
   {
      fprintf(stderr, "Error connecting to %s\n", host);
      exit(2);
   }
   
   freeaddrinfo(res);
   
   
   if ( send_header_info(s) == -1 )
   {
      fprintf(stderr, "Couldn't send WAV-info\n");
      exit(3);
   }

   uint32_t chunk_size, buffer_size;
   // buffer_size isn't in this program, since we don't care about blocking.
   if ( !get_backend_info(s, &chunk_size, &buffer_size))
   {
      fprintf(stderr, "Couldn't recieve backend info\n");
      close(s);
      exit(1);
   }

   uint8_t *buffer = malloc ( chunk_size );
   if ( !buffer )
   {
      fprintf(stderr, "Couldn't allocate memory for buffer.\n");
      close(s);
      exit(1);
   }

   
   while(1)
   {
      rc = read(0, buffer, chunk_size);
      if ( rc == 0 )
      {
         close(s);
         exit(0);
      }
      else
      {
         rc = send(s, buffer, chunk_size, 0);
         if ( rc != chunk_size && rc > 0 )
         {
            fprintf(stderr, "Sent not enough data ... :p\n");
         }
         else if ( rc == 0 )
         {
            fprintf(stderr, "Connection error ...\n");
            close(s);
            exit(8);
         }
      
      }
   }
   
   return 0;
}
      
int send_header_info(int s)
{
   /* If client is big endian, swaps over the data that the client sends if sending
    * premade WAV-header (--raw). 
    * Server expects little-endian */

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

      *((uint32_t*)(buffer + 24)) = raw_rate;
      *((uint16_t*)(buffer+22)) = channel;
      *((uint16_t*)(buffer+34)) = 16;
      rc = send ( s, buffer, HEADER_SIZE, 0 );
      if ( rc == HEADER_SIZE )
      {
         return 1;
      }

      else
         return -1;
   }
}

int get_backend_info(int socket, uint32_t* chunk_size, uint32_t* buffer_size)
{
   uint32_t chunk_size_temp, buffer_size_temp;
   int rc;

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

   return 1;

}

void print_help(char *appname)
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

void parse_input(int argc, char** argv)
{
   int i;
   for ( i = 1; i < argc; i++ )
   {
      if ( !strcmp( "-r", argv[i] ) || !strcmp( "--rate", argv[i] ) )
      {
         if ( i < argc -1 )
         {
            raw_rate = atoi ( argv[++i] );
            continue;
         }
         else
         {
            fprintf(stderr, "-r/--rate takes an argument.\n");
            exit(1);
         }

      }
      if ( !strcmp( "--raw", argv[i] ))
      {
         raw_mode = 1;
         continue;
      }
      if ( !strcmp( "-p", argv[i] ) || !strcmp ( "--port", argv[i] ) )
      {
         if ( i < argc -1 )
         {
            if ( atoi ( argv[++i] ) < 1 || atoi ( argv[i] ) >= 0xFFFF )
            {
               fprintf(stderr, "Invalid port\n");
               exit(1);
            }
            strncpy( port, argv[i], 6 );
            continue;
         }
         else
         {
            fprintf(stderr, "-p/--port takes an argument.\n");
            exit(1);
         }
         
      }
      if ( !strcmp( "-c", argv[i] ) || !strcmp ( "--channel", argv[i] ) )
      {
         if ( i < argc -1 )
         {
            if ( atoi ( argv[++i] ) < 1 )
            {
               fprintf(stderr, "Invalid num of channels\n");
               exit(1);
            }
            channel = atoi ( argv[i] );
            continue;
         }
         else
         {
            fprintf(stderr, "-c/--channel takes an argument.\n");
            exit(1);
         }
         
      }
      if ( !strcmp ( "-h", argv[i] ) || !strcmp( "--help", argv[i] ) )
      {
         print_help(argv[0]);
         exit(0);
      }
      strncpy(host, argv[i], 128);
      
   }
}
   
   
