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

#ifndef __RSOUND_H
#define __RSOUND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#define LIBRSOUND_VERSION PACKAGE_VERSION
#else
#define LIBRSOUND_VERSION "0.9"
#endif

#define RSD_DEFAULT_HOST "localhost"
#define RSD_DEFAULT_PORT "12345"
#define RSD_DEFAULT_OBJECT "rsound"


   /* Defines sample formats available. Defaults to S16_LE should it never be set. */
   enum rsd_format
   {
      RSD_S16_LE = 0x0001,
      RSD_S16_BE = 0x0002,
      RSD_U16_LE = 0x0004,
      RSD_U16_BE = 0x0008,
      RSD_U8     = 0x0010,
      RSD_S8     = 0x0020
   };

   /* Defines operations that can be used with rsd_set_param() */
   enum rsd_settings
   {
      RSD_SAMPLERATE = 0,
      RSD_CHANNELS,
      RSD_HOST,
      RSD_PORT,
      RSD_BUFSIZE,
      RSD_LATENCY,
      RSD_FORMAT
   };

   /* Do not use directly */
   typedef struct connection
   {
      volatile int socket;
      volatile int ctl_socket;
   } connection_t;

   /* Do not use directly */
   typedef struct rsound_thread
   {
      pthread_t threadId;
      pthread_mutex_t mutex;
      pthread_mutex_t cond_mutex;
      pthread_cond_t cond;
   } rsound_thread_t;

   /* No not use directly */
   typedef struct backend_info
   {
      /* Inherit latency from backend that must be added to the calculated latency when we call rsd_delay() client side. */
      uint32_t latency;
      uint32_t chunk_size;
   } backend_info_t;

   /* Defines the main structure for use with the API. */
   typedef struct rsound
   {
      connection_t conn;
      char *host;
      char *port;
      char *buffer;
      int conn_type;

      volatile int buffer_pointer;
      size_t buffer_size;
      volatile int thread_active;

      int64_t total_written;
      struct timespec start_tv_nsec;
      struct timeval start_tv_usec;
      volatile int has_written;
      int bytes_in_buffer;
      int delay_offset;
      int max_latency;
      backend_info_t backend_info;

      volatile int ready_for_data;

      uint32_t rate;
      uint32_t channels;
      uint16_t format;
      int framesize;

      rsound_thread_t thread;
   } rsound_t;

   /* -- API --
      All functions (except for rsd_write() return 0 for success, and -1 for error. errno is currently not set. */

   /* Initializes an rsound_t structure. To make sure no memory leaks occur, you need to rsd_free() it after use.
      A typical use of the API is as follows:
      rsound_t *rd;
      rsd_init(&rd);
      rsd_set_param(rd, RSD_HOST, "foohost");
    *sets more params*
    rsd_start(rd);
    rsd_write(rd, buf, size); 
    rsd_stop(rd);
    rsd_free(rd);
    */
   int rsd_init (rsound_t **rd);

   /* Sets params associated with an rsound_t. These options (int options) include:

RSD_HOST: Server to connect to. Expects (char *) in param. If not set, will default 
to environmental variable RSD_SERVER or "localhost". 

RSD_PORT: Set port. Expects (char *) in param. If not set, will default to environmental 
variable RSD_PORT or "12345".

RSD_CHANNELS: Set number of audio channels. Expects (int *) in param. Mandatory.
RSD_SAMPLERATE: Set samplerate of audio stream. Expects (int *) in param. Mandatory.
RSD_BUFSIZE: Sets internal buffersize for the stream. Might be overridden if too small. 
Expects (int *) in param. Optional.
RSD_LATENCY: Sets maximum audio latency in milliseconds, (must be used with rsd_delay_wait() or this will have no effect). 
Most applications do not need this. Might be overridden if too small. Expects (int *) in param. Optional.
RSD_FORMAT: Sets sample format. It defaults to S16_LE, so you probably will not use this. Expects (int *) in param, with
available values found in the format enum. If invalid format is given, param might be changed to reflect the sample format the library will use.
*/
   int rsd_set_param (rsound_t *rd, enum rsd_settings option, void* param);

   /* Establishes connection to server. Might fail if connection can't be established or that one of 
      the mandatory options isn't set in rsd_set_param(). This needs to be called after params have been set
      with rsd_set_param(), and before rsd_write(). */ 
   int rsd_start (rsound_t *rd);

   /* Disconnects from server. All audio data still in network buffer and other buffers will be dropped. 
      To continue playing, you will need to rsd_start() again. */
   int rsd_stop (rsound_t *rd);

   /* Writes from buf to the internal buffer. Might fail if no connection is established, 
      or there was an unexpected error. This function will block until all data has
      been written to the buffer. This function will return the number of bytes written to the buffer,
      or 0 should it fail (disconnection from server). You will have to restart the stream again should this occur. */
   size_t rsd_write (rsound_t *rd, const char* buf, size_t size);

   /* Gets the position of the buffer pointer. 
      Not really interesting for normal applications. 
      Might be useful for implementing rsound on top of other blocking APIs. */
   size_t rsd_pointer (rsound_t *rd);

   /* Aquires how much data can be written to the buffer without blocking */
   size_t rsd_get_avail (rsound_t *rd);

   /* Aquires the latency at the moment for the audio stream. It is measured in bytes. Useful for syncing video and audio. */
   size_t rsd_delay (rsound_t *rd);

   /* Utility for returning latency in milliseconds. */
   inline size_t rsd_delay_ms (rsound_t *rd)
   {
      assert (rd);
      assert (rd->rate > 0 && rd->channels > 0 );

      return (rsd_delay(rd) * 1000) / ( rd->rate * rd->channels * rd->framesize );
   }

   /* Returns bytes per sample */
   inline int rsd_samplesize( rsound_t *rd )
   {
      return rd->framesize;
   }

   /* Will sleep until latency of stream reaches maximum allowed latency defined earlier by rsd_set_param - RSD_LATENCY 
      Useful for hard headed blocking I/O design where user defined latency is needed. If rsd_set_param hasn't been set
      with RSD_LATENCY, this function will do nothing. */
   void rsd_delay_wait(rsound_t *rd);


   /* Pauses or unpauses a stream. pause -> enable = 1 
      This function essentially calls on start() and stop(). This behavior might be changed later. */
   int rsd_pause (rsound_t *rd, int enable);

   /* Frees an rsound_t struct. */
   int rsd_free (rsound_t *rd);

#ifdef __cplusplus
}
#endif

#endif



