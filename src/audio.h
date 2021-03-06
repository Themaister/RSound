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

#ifndef __AUDIO_H
#define __AUDIO_H

#define MONO 1
#define STEREO 2
#define HEADER_SIZE 44 
#define DEFAULT_CHUNK_SIZE 512

#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#ifdef HAVE_SAMPLERATE
#include <samplerate.h>
#else
#include "resampler.h"
#endif

// Defines audio formats supported by rsound. Might be extended in the future :)
enum rsd_format
{
   RSD_UNSPEC = 0x0000,
   RSD_S16_LE = 0x0001,
   RSD_S16_BE = 0x0002,
   RSD_U16_LE = 0x0004,
   RSD_U16_BE = 0x0008,
   RSD_U8     = 0x0010,
   RSD_S8     = 0x0020,
   RSD_ALAW   = 0x0100,
   RSD_MULAW  = 0x0200,
   RSD_S32_LE = 0x0400,
   RSD_S32_BE = 0x0800,
   RSD_U32_LE = 0x2000,
   RSD_U32_BE = 0x4000,
};

// Defines connection types the server can handle.
enum
{
   RSD_CONN_TCP = 0x0000,
   RSD_CONN_UNIX = 0x0001
};

// The header that is sent from client to server
typedef struct wav_header 
{
   uint16_t numChannels;
   uint32_t sampleRate;
   uint16_t bitsPerSample;
   uint16_t rsd_format;
   char *stream_name;
} wav_header_t;

// Info that is sent from server to client
typedef struct backend_info
{
   uint32_t latency;    // Is used by client to calculate latency 
   uint32_t chunk_size; // Preferred TCP packet size. Might just be ignored completely :)
   unsigned resample; // Do we have to resample? (Jack)
   double ratio; // Resampling ratio
} backend_info_t;

typedef struct rsd_backend_callback
{
   void (*initialize)(void);
   int (*init)(void**);
   int (*open)(void*, wav_header_t*);
   size_t (*write)(void*, const void*, size_t);
   void (*get_backend_info)(void*, backend_info_t*);
   int (*latency)(void*);
   void (*close)(void*);
   void (*shutdown)(void);
   const char *backend;
} rsd_backend_callback_t;

typedef struct
{
   int socket;
   int ctl_socket;
   int64_t serv_ptr;
   float rate_ratio;
   char identity[256];
} connection_t;


// Returns a string. Used for error reporting mostly should the format not be supported.
const char* rsnd_format_to_string(enum rsd_format fmt);

int rsnd_format_to_bytes(enum rsd_format fmt);

enum rsd_format_conv
{
   RSD_NULL = 0x0000,
   RSD_S_TO_U = 0x0001,
   RSD_U_TO_S = 0x0002,
   RSD_SWAP_ENDIAN = 0x0004,
   RSD_ALAW_TO_S16 = 0x0008,
   RSD_MULAW_TO_S16 = 0x0010,
   RSD_S8_TO_S16 = 0x0020,
   RSD_S16_TO_FLOAT = 0x0040,
   RSD_S32_TO_FLOAT = 0x0080,
   RSD_S32_TO_S16 = 0x0100
};

void audio_converter(void* data, enum rsd_format fmt, int operation, size_t bytes); 

#ifdef HAVE_SAMPLERATE
long resample_callback(void *cb_data, float **data);
#else
size_t resample_callback(void *cb_data, float **data);
#endif

typedef struct
{
   enum rsd_format format;
   void *data;
   connection_t *conn;
   float buffer[DEFAULT_CHUNK_SIZE];
   int framesize;
} resample_cb_state_t;

int receive_data(void *backend_data, connection_t *conn, void *buffer, size_t size);
int converter_fmt_to_s16ne(enum rsd_format format);
int converter_fmt_to_s32ne(enum rsd_format format);

#define BYTES_TO_SAMPLES(x, fmt) (x / (rsnd_format_to_bytes(fmt)))

#endif
