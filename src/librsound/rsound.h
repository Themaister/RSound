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

#define RSD_DEFAULT_HOST "localhost"
#define RSD_DEFAULT_PORT "12345"
#define LIBRSOUND_VERSION "0.6"

enum {
   RSD_SAMPLERATE = 0,
   RSD_CHANNELS,
   RSD_HOST,
   RSD_PORT,
   RSD_BUFSIZE,
   RSD_LATENCY
};

typedef struct connection
{
   int socket;
   int ctl_socket;
} connection_t;

typedef struct rsound_thread
{
   pthread_t threadId;
   pthread_mutex_t mutex;
   pthread_mutex_t cond_mutex;
   pthread_cond_t cond;
} rsound_thread_t;

typedef struct backend_info
{
   // Inherit latency from backend that must be added to the calculated latency. 
   uint32_t latency;
   uint32_t chunk_size;
} backend_info_t;

typedef struct rsound
{
   connection_t conn;
   char *host;
   char *port;
   char *buffer;

   volatile int buffer_pointer;
   size_t buffer_size;
   volatile int thread_active;

   int64_t total_written;
	struct timespec start_tv_nsec;
   struct timeval start_tv_usec;
   int has_written;
   int bytes_in_buffer;
   int min_latency;
   backend_info_t backend_info;

   int ready_for_data;

   uint32_t rate;
   uint32_t channels;

   rsound_thread_t thread;
} rsound_t;

/* -- API --
   All functions (except for rsd_write() return 0 for success, and -1 for error */

/* Initializes an rsound_t structure. To make sure no memory leaks occur, you need to rsd_free() it after use.
   e.g.
      rsound_t *rd;
      rsd_init(&rd);
*/
int rsd_init (rsound_t **rd);

/* Sets params associated with an rsound_t. These options (int options) include:
   RSD_HOST: Server to connect to. Expects (char *) in param. Mandatory. Might use RSD_DEFAULT_HOST
   RSD_PORT: Set port. Expects (char *) in param. Mandatory. Might use RSD_DEFAULT_PORT
   RSD_CHANNELS: Set number of audio channels. Expects (int *) in param. Mandatory.
   RSD_SAMPLERATE: Set samplerate of audio stream. Expects (int *) in param. Mandatory.
   RSD_BUFSIZE: Sets internal buffersize for the stream. Might be overridden if too small. 
   Expects (int *) in param. Optional.
   RSD_LATENCY: (!NOT PROPERLY IMPLEMENTED YET!) Sets maximum audio latency in milliseconds. 
   Might be overridden if too small. Expects (int *) in param. Optional.
*/
int rsd_set_param (rsound_t *rd, int option, void* param);

/* Establishes connection to server. Might fail if connection can't be established or that one of 
   the mandatory options isn't set in rsd_set_param() */ 
int rsd_start (rsound_t *rd);

/* Disconnects from server. To continue playing, you will need to rsd_start() again. */
int rsd_stop (rsound_t *rd);

/* Writes from buf to the internal buffer. Might fail if no connection is established, 
   or there was an unexpected error. This function will block until all data has
   been written to the buffer. This function will return the number of bytes written to the buffer,
   or 0 should it fail (disconnection from server). You will have to restart the stream again should this occur. */
size_t rsd_write (rsound_t *rd, const char* buf, size_t size);

/* Gets the position of the buffer pointer. 
   Not really interesting for normal applications. */
size_t rsd_pointer (rsound_t *rd);

/* Aquires how much data can be written to the buffer without blocking */
size_t rsd_get_avail (rsound_t *rd);

/* Aquires the latency at the moment for the audio stream. It is measured in bytes. Useful for syncing video and audio. */
size_t rsd_delay (rsound_t *rd);

/* Pauses or unpauses a stream. pause -> enable = 1 */
int rsd_pause (rsound_t *rd, int enable);

/* Frees an rsound_t struct. */
int rsd_free (rsound_t *rd);

#ifdef __cplusplus
}
#endif

#endif



