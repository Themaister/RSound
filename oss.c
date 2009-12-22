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

#include "oss.h"

// Opens and sets params
int init_oss(oss_t* sound, wav_header* w)
{
	sound->audio_fd = open(OSS_DEVICE, O_WRONLY, 0);

	if ( sound->audio_fd == -1 )
	{
		perror(OSS_DEVICE);
		return 0;
	}
	
	int format = AFMT_S16_LE;
	if ( ioctl( sound->audio_fd, SNDCTL_DSP_SETFMT, &format) == -1 )
	{
		perror("SNDCTL_DSP_SETFMT");
		close(sound->audio_fd);
		return 0;
	}
	
	if ( format != AFMT_S16_LE )
	{
		fprintf(stderr, "Sound card doesn't support S16LE sampling format.\n");
		close(sound->audio_fd);
		return 0;
	}
	
	int stereo; 
	if ( w->numChannels == 2 )
		stereo = 1;
	else if ( w->numChannels == 1 )
		stereo = 0;
	else
	{
		fprintf(stderr, "Multichannel audio not supported.\n");
		close(sound->audio_fd);
		return 0;
	}
		
	if ( ioctl( sound->audio_fd, SNDCTL_DSP_STEREO, &stereo) == -1 )
	{
		perror("SNDCTL_DSP_STEREO");
		close(sound->audio_fd);
		return 0;
	}
	
	if ( stereo != 1 && w->numChannels != 1 )
	{
		fprintf(stderr, "Sound card doesn't support stereo mode.\n");
		close(sound->audio_fd);
		return 0;
	}
	
	int sampleRate = w->sampleRate;
	if ( ioctl ( sound->audio_fd, SNDCTL_DSP_SPEED, &sampleRate ) == -1 )
	{
		perror("SNDCTL_DSP_SPEED");
		close(sound->audio_fd);
		return 0;
	}
	
	if ( sampleRate != w->sampleRate )
	{
		fprintf(stderr, "Sample rate couldn't be set correctly.\n");
		close(sound->audio_fd);
		return 0;
	}
	
	
    return 1;
}

void clean_oss_interface(oss_t* sound)
{
	close(sound->audio_fd);
}


void* oss_thread( void* socket )
{
	oss_t sound;
	wav_header w;
	int rc;
	int read_counter;
	int active_connection;
	int underrun_count = 0;

	int s_new = *((int*)socket);
	free(socket);

	if ( verbose )
		fprintf(stderr, "Connection accepted, awaiting WAV header data...\n");

	rc = get_wav_header(s_new, &w);

	if ( rc != 1 )
	{
		close(s_new);
		fprintf(stderr, "Couldn't read WAV header... Disconnecting.\n");
		pthread_exit(NULL);
	}

	if ( verbose )
	{
		fprintf(stderr, "Successfully got WAV header ...\n");
		pheader(&w);
	}	

	if ( verbose )
		printf("Initializing OSS ...\n");
	if ( !init_oss(&sound, &w) )
	{
		fprintf(stderr, "Initializing of OSS failed ...\n");
		close(s_new);
		pthread_exit(NULL);
	}
	

	if ( verbose )
		printf("Initializing of OSS successful... Party time!\n");


	active_connection = 1;
	// While connection is active, read CHUNK_SIZE bytes and reroutes it to OSS_DEVICE
	while(active_connection)
	{
		rc = recv(s_new, sound.buffer, CHUNK_SIZE, 0);
		if ( rc == 0 )
		{
			active_connection = 0;
			break;
		}

		rc = write(sound.audio_fd, sound.buffer, CHUNK_SIZE);
		if (rc < CHUNK_SIZE) 
		{
			fprintf(stderr, "Underrun occurred. Count: %d\n", ++underrun_count);
		} 
		else if (rc < 0) 
		{
			fprintf(stderr,
					"Error from write\n");
		}  
		else if (rc != CHUNK_SIZE) 
		{
			fprintf(stderr,
					"Short write, write %d frames\n", rc);
		}

	}

	close(s_new);
	clean_oss_interface(&sound);
	if ( verbose )
		fprintf(stderr, "Closed connection. The friendly PCM-service welcomes you back.\n\n\n");

	pthread_exit(NULL);
}

