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

#include "rsound.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h> 
#include <time.h>
#include <assert.h>

/* 
   *************************************************************************************************   
   *  Naming convention. Functions for use in API are called rsd_*(), internal function are called *
   *  rsnd_*()                                                                                     *
   *************************************************************************************************
*/


static inline int rsnd_is_little_endian(void);
static inline void rsnd_swap_endian_16 ( uint16_t * x );
static inline void rsnd_swap_endian_32 ( uint32_t * x );
static int rsnd_connect_server( rsound_t *rd );
static int rsnd_send_header_info(rsound_t *rd);
static int rsnd_get_backend_info ( rsound_t *rd );
static int rsnd_create_connection(rsound_t *rd);
static size_t rsnd_send_chunk(int socket, char* buf, size_t size);
static int rsnd_start_thread(rsound_t *rd);
static int rsnd_stop_thread(rsound_t *rd);
static size_t rsnd_get_delay(rsound_t *rd);
static size_t rsnd_get_ptr(rsound_t *rd);
static int rsnd_reset(rsound_t *rd);
static void* rsnd_thread ( void * thread_data );

/* Determine whether we're running big- or little endian */
static inline int rsnd_is_little_endian(void)
{
   uint16_t i = 1;
   return *((uint8_t*)&i);
}

/* Simple functions for swapping bytes */
static inline void rsnd_swap_endian_16 ( uint16_t * x )
{
   *x = (*x>>8) | (*x<<8);
}

static inline void rsnd_swap_endian_32 ( uint32_t * x )
{
   *x =  (*x >> 24 ) |
         ((*x<<8) & 0x00FF0000) |
         ((*x>>8) & 0x0000FF00) |
         (*x << 24);
}

/* Creates sockets and attempts to connect to the server. Returns -1 when failed, and 0 when success. */
static int rsnd_connect_server( rsound_t *rd )
{
   struct addrinfo hints, *res;
   memset(&hints, 0, sizeof( hints ));
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   
   getaddrinfo(rd->host, rd->port, &hints, &res);

   rd->conn.socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   rd->conn.ctl_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   if ( rd->conn.socket < 0 || rd->conn.ctl_socket < 0 )
   {
      fprintf(stderr, "Getting sockets failed.\n");
      goto error;
   }

   if ( connect(rd->conn.socket, res->ai_addr, res->ai_addrlen) < 0)
      goto error;

   if ( connect(rd->conn.ctl_socket, res->ai_addr, res->ai_addrlen) < 0 )
      goto error;

/* Uses non-blocking IO since it performed more deterministic with poll()/send() */   
#ifndef __CYGWIN__
   if ( fcntl(rd->conn.socket, F_SETFL, O_NONBLOCK) < 0)
   {
      fprintf(stderr, "Couldn't set socket to non-blocking ...\n");
      goto error;
   }
#endif /* Cygwin doesn't seem to like non-blocking I/O ... */

   freeaddrinfo(res);
   return 0;

   /* Cleanup for errors. */
error:
   fprintf(stderr, "Connecting to server failed.\n");
   freeaddrinfo(res);
   return -1;
}

/* Conjures a WAV-header and sends this to server. Returns -1 when failed, and 0 when success. */
static int rsnd_send_header_info(rsound_t *rd)
{

/* Defines the size of a wave header */
#define HEADER_SIZE 44
   char buffer[HEADER_SIZE] = {0};
   int rc = 0;
   struct pollfd fd;


/* These magic numbers represent the position of the elements in the wave header. 
   We can't simply send a wave struct over the network since the compiler is allowed to
   pad our structs as they like, so sizeof(waveheader) might not be similar on two different
   systems. */

#define RATE 24
#define CHANNEL 22
#define FRAMESIZE 34


   uint32_t sample_rate_temp = rd->rate;
   uint16_t channels_temp = rd->channels;
   /* So far, librsound only support S16_LE samples */
   uint16_t framesize_temp = 16;

   /* Since the values in the wave header we are interested in, are little endian (>_<), we need
      to determine whether we're running it or not, so we can byte swap accordingly. 
      Could determine this compile time, but it was simpler to do it this way. */
   if ( !rsnd_is_little_endian() )
   {
      rsnd_swap_endian_32(&sample_rate_temp);
      rsnd_swap_endian_16(&channels_temp);
      rsnd_swap_endian_16(&framesize_temp);
   }

   /* Not being able to use structs ftw >_< */
   *((uint32_t*)(buffer+RATE)) = sample_rate_temp;
   *((uint16_t*)(buffer+CHANNEL)) = channels_temp;
   *((uint16_t*)(buffer+FRAMESIZE)) = framesize_temp;

   fd.fd = rd->conn.socket;
   fd.events = POLLOUT;

   size_t written = 0;

   /* Really makes sure that we do send the whole header. Sets a timeout of 10 seconds. */
   while ( written < HEADER_SIZE )
   {
      if ( poll(&fd, 1, 10000) < 0 )
      {
         return -1;
      }

      if ( fd.revents & POLLHUP )
      {
         return -1;
      }

      rc = send ( rd->conn.socket, buffer + written, HEADER_SIZE - written, 0);
      if ( rc <= 0 )
      {
         return -1;
      }
      written += rc;
   }

   return 0;
}

/* Recieves backend info from server that is of interest to the client. (This mini-protocol might be extended later on.) */
static int rsnd_get_backend_info ( rsound_t *rd )
{
   #define RSND_HEADER_SIZE 8

   size_t recieved = 0;

   char rsnd_header[RSND_HEADER_SIZE] = {0};
   int rc;

   struct pollfd fd;
   fd.fd = rd->conn.socket;
   fd.events = POLLIN;

   /* Really makes sure we recieve the whole header. (If this doesn't go through in one go, it'd make me really scared.) */
   while ( recieved < RSND_HEADER_SIZE )
   {

      if ( poll(&fd, 1, 10000) < 0 )
      {
         return -1;
      }

      if ( fd.revents & POLLHUP )
      {
         return -1;
      }

      rc = recv(rd->conn.socket, rsnd_header + recieved, RSND_HEADER_SIZE - recieved, 0);
      if ( rc <= 0)
      {
         return -1;
      }

      recieved += rc;
   }

   /* Again, we can't be 100% certain that sizeof(backend_info_t) is equal on every system */
   rd->backend_info.latency = ntohl(*((uint32_t*)(rsnd_header)));
   rd->backend_info.chunk_size = ntohl(*((uint32_t*)(rsnd_header+4)));

   /* Assumes a default buffer size should it cause problems of being too small */
   if ( rd->buffer_size <= 0 || rd->buffer_size < rd->backend_info.chunk_size )
      rd->buffer_size = rd->backend_info.chunk_size * 32;

   /* Reallocs memory each time in case we have changes the buffer size from last time */
   rd->buffer = realloc ( rd->buffer, rd->buffer_size );
   rd->buffer_pointer = 0;

/////////
   int bufsiz = rd->buffer_size;
   setsockopt(rd->conn.socket, SOL_SOCKET, SO_SNDBUF, &bufsiz, sizeof(int));
/////////


   return 0;
}

/* Makes sure that we're connected and done with wave header handshaking. Returns -1 on error, and 0 on success. 
   This goes for all other functions in use. */
static int rsnd_create_connection(rsound_t *rd)
{
   int rc;

   /* Are we connected to the server? If not, these values have been set to <0, so we make sure that we connect */
   if ( rd->conn.socket <= 0 && rd->conn.ctl_socket <= 0 )
   {
      rc = rsnd_connect_server(rd);
      if (rc < 0)
      {
         rsd_stop(rd);
         return -1;
      }
      
      /* After connecting, makes really sure that we have a working connection. */
      struct pollfd fd = {
         .fd = rd->conn.socket,
         .events = POLLOUT
      };

      if ( poll(&fd, 1, 2000) < 0 )
      {
         perror("poll");
         rsd_stop(rd);
         return -1;
      }

      if ( !(fd.revents & POLLOUT) )
      {
         rsd_stop(rd);
         return -1;
      }

   }
   /* Is the server ready for data? The first thing it expects is the wave header */
   if ( !rd->ready_for_data )
   {
      /* Part of the uber simple protocol.
         1. Send wave header.
         2. Recieve backend info like latency and preferred packet size.
         3. Starts the playback thread. */


      rc = rsnd_send_header_info(rd);
      if (rc < 0)
      {
         rsd_stop(rd);
         return -1;
      }

      rc = rsnd_get_backend_info(rd);
      if (rc < 0)
      {
         rsd_stop(rd);
         return -1;
      }

      rc = rsnd_start_thread(rd);
      if (rc < 0)
      {
         rsd_stop(rd);
         return -1;
      }

      rd->ready_for_data = 1;
   }
   
   return 0;
}

/* Sends a chunk over the network. Makes sure that everything is sent. Returns 0 if connection is lost, >0 if success. */
static size_t rsnd_send_chunk(int socket, char* buf, size_t size)
{
   int rc = 0;
   size_t wrote = 0;
   size_t send_size = 0;
   struct pollfd fd;
   fd.fd = socket;
   fd.events = POLLOUT;

#define MAX_PACKET_SIZE 1024

   while ( wrote < size )
   {
      pthread_testcancel();
      if ( poll(&fd, 1, 10000) < 0 )
      {
         perror("poll");
         return 0;
      }

      if ( fd.revents & POLLHUP )
      {
         fprintf(stderr, "*** Remote side hung up! ***\n");
         return 0;
      }

      if ( fd.revents & POLLOUT )
      {
         /* We try to limit ourselves to 1KiB packet sizes. */
         send_size = (size - wrote) > MAX_PACKET_SIZE ? MAX_PACKET_SIZE : size - wrote;
         rc = send(socket, buf + wrote, send_size, 0);
         if ( rc < 0 )
         {
            fprintf(stderr, "Error sending chunk, %s\n", strerror(errno));
            return rc;
         }
         wrote += rc;
      }
      else
         return 0;
      /* If server hasn't stopped blocking after 10 secs, then we should probably shut down the stream. */

   }
   return wrote;
}

/* Calculates how many bytes there are in total in the virtual buffer. This is calculated client side.
   It should be accurate enough unless we have big problems with buffer underruns.
   This function is called by rsd_delay() to determine the latency. 
   This function might be changed in the future to correctly determine latency from server. */
static void rsnd_drain(rsound_t *rd)
{
   /* If the audio playback has started on the server we need to use timers. */
   if ( rd->has_written )
   {
      int64_t temp, temp2;

/* Falls back to gettimeofday() when CLOCK_MONOTONIC is not supported */

/* Calculates the amount of bytes that the server has consumed. */
#ifdef _POSIX_MONOTONIC_CLOCK
      struct timespec now_tv;
      clock_gettime(CLOCK_MONOTONIC, &now_tv);
      
      temp = (int64_t)now_tv.tv_sec - (int64_t)rd->start_tv_nsec.tv_sec;

      /* Multiplies by 2 since we're still only supporting S16_LE samples. */
      temp *= rd->rate * rd->channels * 2;

      temp2 = (int64_t)now_tv.tv_nsec - (int64_t)rd->start_tv_nsec.tv_nsec;
      temp2 *= rd->rate * rd->channels * 2;
      temp2 /= 1000000000;
      temp += temp2;
#else
      struct timeval now_tv;
      gettimeofday(&now_tv, NULL);
      
      temp = (int64_t)now_tv.tv_sec - (int64_t)rd->start_tv_usec.tv_sec;
      temp *= rd->rate * rd->channels * 2;

      temp2 = (int64_t)now_tv.tv_usec - (int64_t)rd->start_tv_usec.tv_usec;
      temp2 *= rd->rate * rd->channels * 2;
      temp2 /= 1000000;
      temp += temp2;
#endif
      /* Calculates the amount of data we have in our virtual buffer. Only used to calculate delay. */
      rd->bytes_in_buffer = (int)((int64_t)rd->total_written + (int64_t)rd->buffer_pointer - temp);
   }
   else
      rd->bytes_in_buffer = rd->buffer_pointer;
}

/* Tries to fill the buffer. Uses signals to determine when the buffer is ready to be filled. Should the thread not be active
   it will treat this as an error. Crude implementation of a blocking FIFO. */ 
static size_t rsnd_fill_buffer(rsound_t *rd, const char *buf, size_t size)
{

   
   /* Wait until we have a ready buffer */
   for (;;)
   {
      /* Should the thread be shut down while we're running, return with error */
      if ( !rd->thread_active )
         return -1;

      pthread_mutex_lock(&rd->thread.mutex);
      if ( rd->buffer_pointer + (int)size <= (int)rd->buffer_size  )
      {
         pthread_mutex_unlock(&rd->thread.mutex);
         break;
      }
      pthread_mutex_unlock(&rd->thread.mutex);
      
      /* Sleeps until we can write to the FIFO. */
      pthread_mutex_lock(&rd->thread.cond_mutex);
      pthread_cond_wait(&rd->thread.cond, &rd->thread.cond_mutex);
      pthread_mutex_unlock(&rd->thread.cond_mutex);
   }
   
   pthread_mutex_lock(&rd->thread.mutex);
   memcpy(rd->buffer + rd->buffer_pointer, buf, size);
   rd->buffer_pointer += (int)size;
   pthread_mutex_unlock(&rd->thread.mutex);

   /* Send signal to thread that buffer has been updated */
   pthread_cond_signal(&rd->thread.cond);

   return size;
}

static int rsnd_start_thread(rsound_t *rd)
{
   int rc;
   if ( !rd->thread_active )
   {
      rd->thread_active = 1;
      rc = pthread_create(&rd->thread.threadId, NULL, rsnd_thread, rd);
      if ( rc < 0 )
      {
         rd->thread_active = 0;
         fprintf(stderr, "Failed to create thread.\n");
         return -1;
      }
      return 0;
   }
   else
      return 0;
}

/* Makes sure that the playback thread has been correctly shut down */
static int rsnd_stop_thread(rsound_t *rd)
{
   if ( rd->thread_active )
   {
      rd->thread_active = 0;
      /* Being really forceful with this unlock, but ... who knows. Better safe than sorry. */

      pthread_mutex_unlock(&rd->thread.mutex);
      pthread_mutex_unlock(&rd->thread.cond_mutex);
      if ( pthread_cancel(rd->thread.threadId) < 0 )
      {
         pthread_cond_signal(&rd->thread.cond);
         fprintf(stderr, "*** Warning, failed to cancel playback thread. ***\n");
         pthread_mutex_unlock(&rd->thread.mutex);
         pthread_mutex_unlock(&rd->thread.cond_mutex);
         return 0;
      }
      pthread_cond_signal(&rd->thread.cond);
      if ( pthread_join(rd->thread.threadId, NULL) < 0 )
         fprintf(stderr, "*** Warning, did not terminate thread. ***\n");
      pthread_mutex_unlock(&rd->thread.mutex);
      pthread_mutex_unlock(&rd->thread.cond_mutex);
      return 0;
   }
   else
      return 0;
}

/* Calculates audio delay in bytes */
static size_t rsnd_get_delay(rsound_t *rd)
{
   size_t ptr;
   pthread_mutex_lock(&rd->thread.mutex);
   rsnd_drain(rd);
   ptr = (size_t)rd->bytes_in_buffer;
   pthread_mutex_unlock(&rd->thread.mutex);

/* Adds the backend latency to the calculated latency. */
   ptr += (size_t)rd->backend_info.latency;
   if ( ptr < 0 )
      ptr = 0;

   return ptr;
}


static size_t rsnd_get_ptr(rsound_t *rd)
{
   int ptr;
   pthread_mutex_lock(&rd->thread.mutex);
   ptr = rd->buffer_pointer;
   pthread_mutex_unlock(&rd->thread.mutex);

   return ptr;
}

/* Ze thread */
static void* rsnd_thread ( void * thread_data )
{
   /* We share data between thread and callable functions */
   rsound_t *rd = thread_data;
   int rc;

   /* Plays back data as long as there is data in the buffer. Else, sleep until it can. */
   /* Two (;;) for loops! :3 Beware! */
   for (;;)
   {
      for(;;)
      {
         /* If the buffer is empty or we've stopped the stream. Jump out of this for loop */
         pthread_mutex_lock(&rd->thread.mutex);
         if ( rd->buffer_pointer < (int)rd->backend_info.chunk_size || !rd->thread_active )
         {
            pthread_mutex_unlock(&rd->thread.mutex);
            break;
         }
         pthread_mutex_unlock(&rd->thread.mutex);

         pthread_testcancel();
         rc = rsnd_send_chunk(rd->conn.socket, rd->buffer, rd->backend_info.chunk_size);

         /* If this happens, we should make sure that subsequent and current calls to rsd_write() will fail. */
         if ( rc <= 0 )
         {
            pthread_testcancel();
            rsnd_reset(rd);

            /* Wakes up a potentially sleeping fill_buffer() */
            pthread_cond_signal(&rd->thread.cond);

            /* This thread will not be joined, so detach. */
            pthread_detach(pthread_self());
            pthread_exit(NULL);
         }
         
         /* If this was the first write, set the start point for the timer. */
         if ( !rd->has_written )
         {
            pthread_mutex_lock(&rd->thread.mutex);
#ifdef _POSIX_MONOTONIC_CLOCK
            clock_gettime(CLOCK_MONOTONIC, &rd->start_tv_nsec);
#else
            gettimeofday(&rd->start_tv_usec, NULL);
#endif
            rd->has_written = 1;
            pthread_mutex_unlock(&rd->thread.mutex);
         }

         /* Increase the total_written counter. Used in rsnd_drain() */
         pthread_mutex_lock(&rd->thread.mutex);
         rd->total_written += rc;
         pthread_mutex_unlock(&rd->thread.mutex);

         /* "Drains" the buffer. This operation looks kinda expensive with large buffers, but hey. D: */
         pthread_mutex_lock(&rd->thread.mutex);
         memmove(rd->buffer, rd->buffer + rd->backend_info.chunk_size, rd->buffer_size - rd->backend_info.chunk_size);
         rd->buffer_pointer -= (int)rd->backend_info.chunk_size;
         pthread_mutex_unlock(&rd->thread.mutex);

         /* Buffer has decreased, signal fill_buffer() */
         pthread_cond_signal(&rd->thread.cond);
                          
      }

      /* If we're still good to go, sleep. We are waiting for fill_buffer() to fill up some data. */
      pthread_testcancel();
      if ( rd->thread_active )
      {
         pthread_mutex_lock(&rd->thread.cond_mutex);
         pthread_cond_wait(&rd->thread.cond, &rd->thread.cond_mutex);
         pthread_mutex_unlock(&rd->thread.cond_mutex);
      }
      /* Abort request, chap. */
      else
      {
         pthread_cond_signal(&rd->thread.cond);
         pthread_exit(NULL);
      }

   }
}

static int rsnd_reset(rsound_t *rd)
{
   close(rd->conn.socket);
   close(rd->conn.ctl_socket);

   /* Pristine stuff, baby! */
   pthread_mutex_lock(&rd->thread.mutex);
   rd->conn.socket = -1;
   rd->conn.ctl_socket = -1;
   rd->total_written = 0;
   rd->ready_for_data = 0;
   rd->has_written = 0;
   rd->bytes_in_buffer = 0;
   rd->thread_active = 0;
   pthread_mutex_unlock(&rd->thread.mutex);
   pthread_mutex_unlock(&rd->thread.cond_mutex);
   pthread_cond_signal(&rd->thread.cond);
   pthread_mutex_unlock(&rd->thread.mutex);
   pthread_mutex_unlock(&rd->thread.cond_mutex);

   return 0;
}


int rsd_stop(rsound_t *rd)
{
   assert(rd != NULL);
   rsnd_stop_thread(rd);
   
/* This might be part of a control interface to the server later. */
/*   
   const char buf[] = "CLOSE";
   send(rd->conn.ctl_socket, buf, strlen(buf) + 1, 0);
*/
   
   rsnd_reset(rd);
   return 0;
}

size_t rsd_write( rsound_t *rsound, const char* buf, size_t size)
{
   assert(rsound != NULL);
   if ( !rsound->ready_for_data )
   {
      return -1;
   }

   size_t result;
   size_t max_write = (rsound->buffer_size - rsound->backend_info.chunk_size)/2;

   size_t written = 0;
   size_t write_size;

/* Makes sure that we can handle arbitrary large write sizes */
   
   while ( written < size )
   {
      write_size = (size - written) > max_write ? max_write : (size - written); 
      result = rsnd_fill_buffer(rsound, buf + written, write_size);

      if ( result <= 0 )
      {
         rsd_stop(rsound);
         return -1;
      }
      written += result;
   }
   return written;
}

int rsd_start(rsound_t *rsound)
{
   assert(rsound != NULL);
   assert(rsound->rate > 0);
   assert(rsound->channels > 0);
   assert(rsound->host != NULL);
   assert(rsound->port != NULL);

   if ( rsnd_create_connection(rsound) < 0 )
   {
      return -1;
   }

   
   return 0;
}

/* ioctl()-ish param setting :D */
int rsd_set_param(rsound_t *rd, int option, void* param)
{
   assert(rd != NULL);
   assert(param != NULL);

   switch(option)
   {
      case RSD_SAMPLERATE:
         rd->rate = *((int*)param);
         break;
      case RSD_CHANNELS:
         rd->channels = *((int*)param);
         break;
      case RSD_HOST:
         if ( rd->host != NULL )
            free(rd->host);
         rd->host = strdup((char*)param);
         break;
      case RSD_PORT:
         if ( rd->port != NULL )
            free(rd->port);
         rd->port = strdup((char*)param);
         break;
      case RSD_BUFSIZE:
         rd->buffer_size = *((int*)param);
         break;
      case RSD_LATENCY:
         rd->max_latency = *((int*)param);
         break;
      default:
         return -1;
   }
   return 0;
         
}

void rsd_delay_wait(rsound_t *rd)
{

   /* When called, we make sure that the latency never goes over the time designated in RSD_LATENCY.
      Useful for certain blocking I/O designs where the latency still needs to be quite low.
      Without this, the latency of the stream will depend on how big the network buffers are.
      ( We simulate that we're a low latency sound card ) */

   /* Should we bother with checking latency at all? */
   if ( rd->max_latency > 0 )
   {
      /* Latency of stream in ms */
      int latency_ms = rsd_delay_ms(rd);

      /* Should we sleep for a while to keep the latency low? */
      if ( rd->max_latency < latency_ms )
      {
         int64_t sleep_ms = latency_ms - rd->max_latency;
         const struct timespec tv = {
            .tv_sec = sleep_ms / 1000,
            .tv_nsec = (sleep_ms * 1000000)%1000000000
         };

         /* Sleepy time */
         nanosleep(&tv, NULL);
      }
   }
}

size_t rsd_pointer(rsound_t *rsound)
{
   assert(rsound != NULL);
   int ptr;

   ptr = rsnd_get_ptr(rsound);   

   return ptr;
}

size_t rsd_get_avail(rsound_t *rd)
{
   assert(rd != NULL);
   int ptr;
   ptr = rsnd_get_ptr(rd);
   return rd->buffer_size - ptr;
}

size_t rsd_delay(rsound_t *rd)
{
   assert(rd != NULL);
   int ptr = rsnd_get_delay(rd);
   if ( ptr < 0 )
      ptr = 0;
   
   return ptr;
}

int rsd_pause(rsound_t* rsound, int enable)
{
   assert(rsound != NULL);
   if ( enable )
      return rsd_stop(rsound);
   else
      return rsd_start(rsound);
}

int rsd_init(rsound_t** rsound)
{
   assert(rsound != NULL);
   *rsound = calloc(1, sizeof(rsound_t));
   if ( *rsound == NULL )
      return -1;
   
   (*rsound)->conn.socket = -1;
   (*rsound)->conn.ctl_socket = -1;

   pthread_mutex_init(&(*rsound)->thread.mutex, NULL);
   pthread_mutex_init(&(*rsound)->thread.cond_mutex, NULL);
   pthread_cond_init(&(*rsound)->thread.cond, NULL);

   /* Checks if environment variable RSD_SERVER and RSD_PORT are set */
   char *rsdhost = getenv("RSD_SERVER");
   char *rsdport = getenv("RSD_PORT");
   if ( rsdhost != NULL && strlen(rsdhost) )
      rsd_set_param(*rsound, RSD_HOST, rsdhost);
   else
      rsd_set_param(*rsound, RSD_HOST, RSD_DEFAULT_HOST);

   if ( rsdport != NULL && strlen(rsdport) )
      rsd_set_param(*rsound, RSD_PORT, rsdport);
   else
      rsd_set_param(*rsound, RSD_PORT, RSD_DEFAULT_PORT);

   return 0;
}

int rsd_free(rsound_t *rsound)
{
   assert(rsound != NULL);
   if (rsound->buffer)
      free(rsound->buffer);
   if (rsound->host)
      free(rsound->host);
   if (rsound->port)
      free(rsound->port);
   free(rsound);
   return 0;
}

