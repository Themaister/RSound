#ifndef __RSOUND_H
#define __RSOUND_H

#include <pthread.h>
#include <time.h>
#include <stdint.h>

enum {
   RSD_SAMPLERATE = 0,
   RSD_CHANNELS,
   RSD_HOST,
   RSD_PORT,
   RSD_BUFSIZE
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

typedef struct rsound
{
   connection_t conn;
   char *host;
   char *port;
   char *buffer;

   int buffer_pointer;
   size_t chunk_size;
   size_t buffer_size;
   int thread_active;

   int64_t total_written;
   struct timespec start_tv;
   int has_written;
   int bytes_in_buffer;

   int ready_for_data;

   uint32_t rate;
   uint32_t channels;

   rsound_thread_t thread;
} rsound_t;

int rsd_init (rsound_t **rd);
int rsd_free (rsound_t *rd);
int rsd_start (rsound_t *rd);
int rsd_set_param (rsound_t *rd, int option, void* param);
int rsd_stop (rsound_t *rd);
int rsd_write (rsound_t *rd, const char* buf, size_t size);
int rsd_pointer (rsound_t *rd);
int rsd_get_avail (rsound_t *rd);
int rsd_delay (rsound_t *rd);
int rsd_pause (rsound_t *rd, int enable);

#endif



