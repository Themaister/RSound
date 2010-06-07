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

#include "librsound/rsound.h"
#include <signal.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "endian.h"
#include "config.h"

#define READ_SIZE 1024
#define HEADER_SIZE 44

static int raw_mode = 0;
static uint32_t raw_rate = 44100;
static uint16_t channel = 2;
static int format = 0;

static char port[128] = "";
static char host[1024] = "";

static int set_other_params(rsound_t *rd);
static void parse_input(int argc, char **argv);

static int infd = 0;

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

   if ( strlen(host) > 0 )
      rsd_set_param(rd, RSD_HOST, (void*)host);
   if ( strlen(port) > 0 )
      rsd_set_param(rd, RSD_PORT, (void*)port);

   if ( set_other_params(rd) < 0 )
   {
      fprintf(stderr, "Couldn't read data.\n");
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
      rc = read(infd, buffer, READ_SIZE);
      if ( rc <= 0 )
         goto quit;

      rc = rsd_write(rd, buffer, READ_SIZE);
      if ( rc <= 0 )
      {
         fprintf(stderr, "Server closed connection.\n");
         goto quit;
      }

   }
quit:
   rsd_stop(rd);
   rsd_free(rd);
   close(infd);

   return 0;
}

static int set_other_params(rsound_t *rd)
{
   int rate, channels, bits;
   uint16_t temp_channels, temp_bits, temp_fmt;
   uint32_t temp_rate;

   int rc;
   int read_in = 0;
   char buf[HEADER_SIZE] = {0};

#define RATE 24
#define CHANNEL 22
#define BITS_PER_SAMPLE 34
#define FORMAT 20

   if ( !raw_mode )
   {  
      while ( read_in < HEADER_SIZE )
      {
         rc = read( infd, buf + read_in, HEADER_SIZE - read_in );
         if ( rc <= 0 )
            return -1;
         read_in += rc;
      }

      // We read raw little endian data from the wave input file. This needs to be converted to
      // host byte order when we pass it to rsd_set_param() later.
      temp_channels = *((uint16_t*)(buf+CHANNEL));
      temp_rate = *((uint32_t*)(buf+RATE));
      temp_bits = *((uint16_t*)(buf+BITS_PER_SAMPLE));
      temp_fmt = *((uint16_t*)(buf+FORMAT));
      if ( !is_little_endian() )
      {
         swap_endian_16(&temp_channels);
         swap_endian_16(&temp_bits);
         swap_endian_32(&temp_rate);
         swap_endian_16(&temp_fmt);
      }

      rate = (int)temp_rate;
      channels = (int)temp_channels;
      bits = (int)temp_bits;

      // Assuming too much, but hey. Not sure how to find big-endian or little-endian in wave files.
      if ( bits == 16 )
         format = RSD_S16_LE;
      else if ( bits == 8 && temp_fmt == 6 )
         format = RSD_ALAW;
      else if ( bits == 8 && temp_fmt == 7 )
         format = RSD_MULAW;
      else if ( bits == 8 )
         format = RSD_U8;
      else
      {
         fprintf(stderr, "Only 8 or 16 bit WAVE files supported.\n");
         rsd_free(rd);
         exit(1);
      }

   }
   else
   {
      rate = (int)raw_rate;
      channels = (int)channel;
   }

   rsd_set_param(rd, RSD_SAMPLERATE, &rate);
   rsd_set_param(rd, RSD_CHANNELS, &channels);
   rsd_set_param(rd, RSD_FORMAT, &format);
   return 0;
}

static void print_help()
{
   printf("rsdplay (librsound) version %s - Copyright (C) 2010 Hans-Kristian Arntzen\n", RSD_VERSION);
   printf("=========================================================================\n");
   printf("Usage: rsdplay [ <hostname> | -p/--port | -h/--help | --raw | -r/--rate | -c/--channels | -B/--bits | -f/--file | -s/--server ]\n");

   printf("\nrsdplay reads PCM data only through stdin (default) or a file, and sends this data directly to an rsound server.\n"); 
   printf("Unless specified with --raw, rsdplay expects a valid WAV header to be present in the input stream.\n\n");
   printf(" Examples:\n"); 
   printf("\trsdplay foo.net < bar.wav\n");
   printf("\tcat bar.wav | rsdplay foo.net -p 4322 --raw -r 48000 -c 2\n\n");

   printf("-p/--port: Defines which port to connect to.\n");
   printf("\tExample: -p 18453. Defaults to port 12345.\n");
   printf("--raw: Enables raw PCM input. When using --raw, rsdplay will generate a fake WAV header\n");
   printf("-r/--rate: Defines input samplerate (raw PCM)\n");
   printf("\tExample: -r 48000. Defaults to 44100\n");
   printf("-c/--channel: Specifies number of sound channels (raw PCM)\n");
   printf("\tExample: -c 1. Defaults to stereo (2)\n");
   printf("-B: Specifies sample format in raw PCM stream\n");
   printf("\tSupported formats are: S16LE, S16BE, U16LE, U16BE, S8, U8, ALAW, MULAW.\n" 
         "\tYou can pass 8 and 16 also, which is equal to U8 and S16LE respectively.\n");
   printf("-h/--help: Prints this help\n");
   printf("-f/--file: Uses file rather than stdin\n");
   printf("-s/--server: More explicit way of assigning hostname\n\n");
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
      { "file", 1, NULL, 'f'},
      { "server", 1, NULL, 's'},
      { NULL, 0, NULL, 0 }
   };

   char optstring[] = "r:p:hc:f:B:s:";
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

         case 'f':
            infd = open(optarg, O_RDONLY);
            if ( infd < 0 )
            {
               fprintf(stderr, "Could not open file ...\n");
               exit(1);
            }
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
            print_help();
            exit(1);

         case 'h':
            print_help();
            exit(0);

         case 's':
            strncpy(host, optarg, 1023);
            host[1023] = '\0';
            break;

         case 'B':
            if ( strcmp("S16LE", optarg) == 0 || strcmp("16", optarg) == 0 )
               format = RSD_S16_LE;
            else if ( strcmp("S16BE", optarg) == 0 )
               format = RSD_S16_BE;
            else if ( strcmp("U16LE", optarg) == 0 )
               format = RSD_U16_LE;
            else if ( strcmp("U16BE", optarg) == 0 )
               format = RSD_U16_BE;
            else if ( strcmp("S8", optarg) == 0 )
               format = RSD_S8;
            else if ( strcmp("U8", optarg) == 0 || strcmp("8", optarg) == 0 )
               format = RSD_U8;
            else if ( strcmp("ALAW", optarg) == 0 )
               format = RSD_ALAW;
            else if ( strcmp("MULAW", optarg) == 0 )
               format = RSD_MULAW;
            else
            {
               fprintf(stderr, "Invalid bit format.\n");
               print_help();
               exit(1);
            }
            break;


         default:
            fprintf(stderr, "Error in parsing arguments.\n");
            exit(1);
      }

   }

   while ( optind < argc )
   {
      strncpy(host, argv[optind++], 1023);
      host[1023] = 0;
   }
}


