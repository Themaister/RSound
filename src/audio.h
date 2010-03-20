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

#ifndef AUDIO_H
#define AUDIO_H

#define MONO 1
#define STEREO 2
#define HEADER_SIZE 44 
#define DEFAULT_CHUNK_SIZE 512

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

typedef struct wav_header 
{
   uint32_t chunkId;
   uint32_t chunkSize;
   uint32_t format;
   uint32_t subChunkId;
   uint32_t subChunkSize;
   uint32_t audioFormat;
   uint16_t numChannels;
   uint32_t sampleRate;
   uint32_t byteRate;
   uint16_t blockAlign;
   uint16_t bitsPerSample;
   uint32_t subChunkId2;
   uint32_t subChunkSize2;
} wav_header_t;

typedef struct backend_info
{
   uint32_t latency;
   uint32_t chunk_size;
} backend_info_t;

typedef struct rsd_backend_callback
{
   void (*initialize)(void);
   int (*init)(void**);
   int (*set_params)(void*, wav_header_t*);
   size_t (*write)(void*, const void*, size_t);
   void (*get_backend_info)(void*, backend_info_t*);
   void (*close)(void*);
   void (*shutdown)(void);
   const char *backend;
} rsd_backend_callback_t;

typedef struct
{
   int socket;
   int ctl_socket;
} connection_t;

#endif
