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

#include "endian.h"
#include "rsound.h"

// This file defines some backend independed operations


void new_sound_thread ( int socket )
{
	int *s = malloc ( sizeof(int));
	*s = socket;

	extern void* (*backend) (void *);

	pthread_t thread;

	pthread_create(&thread, NULL, backend, (void*)s);		
}

// Parses input (TODO: Make this more elegant?)
void parse_input(int argc, char ** argv)
{
	int i;
	for ( i = 1; i < argc; i++ )
	{
		if ( !strcmp( "-d", argv[i] ) || !strcmp( "--device", argv[i] ) )
		{
			if ( i < argc -1 )
			{
				strncpy( device, argv[++i], 64 );
				continue;
			}
			else
			{
				fprintf(stderr, "-d/--device takes an argument.\n");
				exit(1);
			}

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
		
		if ( !strcmp( "-v", argv[i] ) || !strcmp( "--verbose", argv[i] ) )
		{
			verbose = 1;
			continue;
		}
		
		if ( !strcmp( "-b", argv[i] ) || !strcmp( "--backend", argv[i] ) )
		{
			if ( i < argc - 1 )
			{
				i++;
				#ifdef _ALSA
				if ( !strcmp( "alsa", argv[i] ) )
				{
					backend = alsa_thread;
					continue;
				}	
				#endif
				#ifdef _OSS
				if ( !strcmp( "oss", argv[i] ) )
				{
					backend = oss_thread;
					continue;
				}
				#endif
				#ifdef _AO
				if ( !strcmp( "libao", argv[i] ) )
				{
					backend = ao_thread;
					continue;
				}
				#endif
				#ifdef _PORTA
				if ( !strcmp( "portaudio", argv[i] ) )
				{
					backend = porta_thread;
					continue;
				}
				#endif
				
				fprintf(stderr, "Valid backend not given. Exiting ...\n");
				exit(1);
			}
			else
			{
				fprintf(stderr, "Valid backend not given. Exiting ...\n");
				exit(1);
			}
		}

		if ( !strcmp ( "-n", argv[i] ) || !strcmp( "--no-daemon", argv[i] ) )
		{
			daemonize = 0;
			continue;
		}
				
		if ( !strcmp ( "-h", argv[i] ) || !strcmp( "--help", argv[i] ) )
		{
			print_help(argv[0]);
			exit(0);
		}

	}
	
	if ( backend == NULL )
	{
		#ifdef _ALSA
		backend = alsa_thread;
		#elif _OSS
		backend = oss_thread;
		#elif _PORTA
		backend = porta_thread;
		#elif _AO
		backend = ao_thread;
		#endif
	}
	
	if ( backend == NULL )
	{
		fprintf(stderr, "%s was not compiled with any output support, exiting ...", argv[0]);
	}
	
	
}

void print_help(char *appname)
{
	putchar('\n');
	printf("Usage: %s [ -d/--device | -b/--backend | -p/--port | -n/--no-daemon | -v/--verbose | -h/--help ]\n", appname);
	printf("\n-d/--device: Specifies an ALSA or OSS device to use.\n");
	printf("  Examples:\n\t-d hw:1,0\n\t-d /dev/audio\n\t Defaults to \"default\" for alsa and /dev/dsp for OSS\n");
	
	printf("\n-b/--backend: Specifies which audio backend to use.\n");
	printf("Supported backends: ");
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
	putchar('\n');
	putchar('\n');
	
	printf("-p/--port: Defines which port to listen on.\n");
	printf("\tExample: -p 18453. Defaults to port 12345.\n");
	printf("-v/--verbose: Enables verbosity\n");
	printf("-n/--no-daemon: Do not run as daemon.\n");
	printf("-h/--help: Prints this help\n\n");
}


void pheader(wav_header *w)
{
	printf("============================================\n");
	printf("WAV header:\n");

	if (w->numChannels == 1)
		printf("  Mono | ");
	else if (w->numChannels == 2)
		printf("  Stereo | ");
	else
		printf("  Multichannel | ");

	printf("%d / ", w->sampleRate);
	printf("%d\n", w->bitsPerSample);

	printf("============================================\n\n");
}

// Reads raw 44 bytes WAV header and parses this
int get_wav_header(int socket, wav_header* head)
{

	int i = is_little_endian();
	// WAV files are little-endian. If server is big-endian, swaps over data to get sane results.
	uint16_t temp16;
	uint32_t temp32;

	int rc = 0;
	char header[HEADER_SIZE] = {0};

	rc = recv(socket, header, HEADER_SIZE, 0);
	if ( rc != HEADER_SIZE )
	{
		fprintf(stderr, "Didn't read enough data for WAV header. recv() returned %d. \n", rc);
		return -1;
	}


	temp16 = *((uint16_t*)(header+22));
	if (!i)
		swap_endian_16 ( &temp16 );
	head->numChannels = temp16;

	temp32 = *((uint32_t*)(header+24));
	if (!i)
		swap_endian_32 ( &temp32 );
	head->sampleRate = temp32;

	temp16 = *((uint16_t*)(header+34));
	if (!i)
		swap_endian_16 ( &temp16 );
	head->bitsPerSample = temp16;

/* Useless stuff (so far)
	memcpy(head->chunkId, header, 4);

	memcpy(&head->chunkSize, header+4, 4);

	memcpy(head->format, header+8, 4);

	memcpy(head->subChunkId, header+12, 4);

	memcpy(&head->subChunkSize, header+16, 4);

	memcpy(&head->audioFormat, header+20, 2);

	memcpy(&head->byteRate, header+28,  4);

	memcpy(&head->blockAlign, header+32, 2);

	memcpy(head->subChunkId, header+36, 4);

	memcpy(&head->subChunkSize2, header+40, 4);
*/

	// Checks some basic sanity of header file
	if ( head->sampleRate <= 0 || head->sampleRate > 192000 || head->bitsPerSample % 8 != 0 )
	{
		fprintf(stderr, "Got garbage header data ...\n");
		return -1;
	}

	return 1;
}

// Sets up listening socket for further use
int set_up_socket()
{
	int rc;
	int s;
	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof (struct addrinfo));
#ifdef __CYGWIN__
	// Because Windows fails.
	hints.ai_family = AF_INET;
#else
	hints.ai_family = AF_UNSPEC;
#endif
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ( verbose )
	{
		fprintf(stderr, "Binding on port %s\n", port);
	}
	if ((rc = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
		return -1;
	}

	s = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if ( s == -1 )
	{
		fprintf(stderr, "Error getting socket\n");
		goto error;
	}


	int yes = 1;
	if (setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) 
	{
		perror("setsockopt");
		goto error;
	} 
	rc = bind(s, servinfo->ai_addr, servinfo->ai_addrlen);
	if ( rc == -1 )
	{
		fprintf(stderr, "Error binding on port %s.\n", port);
		goto error;
	}

	freeaddrinfo(servinfo);
	return s;

	error:
		freeaddrinfo(servinfo);
		return -1;

}

