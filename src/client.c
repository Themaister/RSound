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

#include "librsound/rsound.h"
#include <signal.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define READ_SIZE 1024
#define HEADER_SIZE 44

static int raw_mode = 0;
static uint32_t raw_rate = 44100;
static uint16_t channel = 2;

static char port[128] = "12345";
static char host[128] = "localhost";

static int set_other_params(rsound_t *rd);
static void parse_input(int argc, char **argv);

int main(int argc, char **argv)
{
   int rc;
	rsound_t *rd;
	char *buffer;
   
   parse_input(argc, argv);
   if ( rsd_init(&rd) < 0 )
   {
      fprintf(stderr, "Failed to initialize\n");
      exit(1);
   }

   rsd_set_param(rd, RSD_HOST, (void*)host);
   rsd_set_param(rd, RSD_PORT, (void*)port);
   if ( set_other_params(rd) < 0 )
   {
      fprintf(stderr, "Couldn't read data from stdin.\n");
      rsd_free(rd);
      exit(1);
   }

   if ( rsd_start(rd) < 0 )
   {
      fprintf(stderr, "Failed to establish connection to server\n");
      rsd_free(rd);
      exit(1);
   }
   
   buffer = malloc ( READ_SIZE );
   if ( buffer == NULL )
   {
      fprintf(stderr, "Failed to allocate memory for buffer\n");
      exit(1);
   }


   while(1)
   {
      memset(buffer, 0, READ_SIZE);
      
      rc = read(0, buffer, READ_SIZE);
      if ( rc <= 0 )
      {
         rsd_stop(rd);
         exit(0);
      }

      rc = rsd_write(rd, buffer, READ_SIZE);
      if ( rc <= 0 )
      {
         rsd_stop(rd);
         rsd_free(rd);
         fprintf(stderr, "Server closed connection.\n");
         exit(0);
      }
      
   }
   
   return 0;
}
      
static int set_other_params(rsound_t *rd)
{
   int rate, channels;

   int rc;
   int read_in = 0;
   char buf[HEADER_SIZE] = {0};

#define RATE 24
#define CHANNEL 22
   
   if ( !raw_mode )
   {  
      while ( read_in < HEADER_SIZE )
      {
         rc = read( 0, buf + read_in, HEADER_SIZE - read_in );
         if ( rc <= 0 )
            return -1;
         read_in += rc;
      }

      channels = (int)(*((uint16_t*)(buf+CHANNEL)));
      rate = (int)(*((uint32_t*)(buf+RATE)));
   }
   else
   {
      rate = (int)raw_rate;
      channels = (int)channel;
   }

   rsd_set_param(rd, RSD_SAMPLERATE, &rate);
   rsd_set_param(rd, RSD_CHANNELS, &channels);
   return 0;
}

static void print_help(char *appname)
{
   printf("Usage: %s [ <hostname> | -p/--port | -h/--help | --raw | -r/--rate | -c/--channels ]\n", appname);
   
   printf("\n%s reads PCM data (S16_LE only currently) only through stdin and sends this data directly to a rsoundserv.\n", appname); 
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


