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

static inline int rsnd_is_little_endian(void);
static inline void rsnd_swap_endian_16 ( uint16_t * x );
static inline void rsnd_swap_endian_32 ( uint32_t * x );
static int rsnd_connect_server( rsound_t *rd );
static int rsnd_send_header_info(rsound_t *rd);
static int rsnd_get_backend_info ( rsound_t *rd );
static int rsnd_create_connection(rsound_t *rd);
static int rsnd_send_chunk(int socket, char* buf, size_t size);
static int rsnd_start_thread(rsound_t *rd);
static int rsnd_stop_thread(rsound_t *rd);
static int rsnd_get_delay(rsound_t *rd);
static int rsnd_get_ptr(rsound_t *rd);
static void* rsnd_thread ( void * thread_data );

static inline int rsnd_is_little_endian(void)
{
	uint16_t i = 1;
	return *((uint8_t*)&i);
}

static inline void rsnd_swap_endian_16 ( uint16_t * x )
{
	*x = (*x>>8) | (*x<<8);
}

static inline void rsnd_swap_endian_32 ( uint32_t * x )
{
	*x = 	(*x >> 24 ) |
			((*x<<8) & 0x00FF0000) |
			((*x>>8) & 0x0000FF00) |
			(*x << 24);
}

static int rsnd_connect_server( rsound_t *rd )
{
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof( hints ));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
   
   getaddrinfo(rd->host, rd->port, &hints, &res);

	rd->conn.socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   rd->conn.ctl_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	if ( connect(rd->conn.socket, res->ai_addr, res->ai_addrlen) != 0 )
		return 0;

	if ( connect(rd->conn.ctl_socket, res->ai_addr, res->ai_addrlen) != 0 )
		return 0;

   if ( fcntl(rd->conn.socket, F_SETFL, O_NONBLOCK) < 0)
   {
      fprintf(stderr, "Couldn't set socket to non-blocking ...\n");
      return 0;
   }

	freeaddrinfo(res);
	return 1;
}

static int rsnd_send_header_info(rsound_t *rd)
{
#define HEADER_SIZE 44
	char buffer[HEADER_SIZE] = {0};
	int rc = 0;

#define RATE 24
#define CHANNEL 22
#define FRAMESIZE 34

	uint32_t sample_rate_temp = rd->rate;
	uint16_t channels_temp = rd->channels;
	uint16_t framesize_temp = 16;

	if ( !rsnd_is_little_endian() )
	{
		rsnd_swap_endian_32(&sample_rate_temp);
		rsnd_swap_endian_16(&channels_temp);
		rsnd_swap_endian_16(&framesize_temp);
	}

	*((uint32_t*)(buffer+RATE)) = sample_rate_temp;
	*((uint16_t*)(buffer+CHANNEL)) = channels_temp;
	*((uint16_t*)(buffer+FRAMESIZE)) = framesize_temp;

   struct pollfd fd;
   fd.fd = rd->conn.socket;
   fd.events = POLLOUT;

   if ( poll(&fd, 1, 500) < 0 )
   {
		close(rd->conn.socket);
		close(rd->conn.ctl_socket);
      return -1;
   }

   if (fd.revents & POLLHUP )
   {
		close(rd->conn.socket);
		close(rd->conn.ctl_socket);
      return -1;
   }

	rc = send ( rd->conn.socket, buffer, HEADER_SIZE, 0);
	if ( rc != HEADER_SIZE )
	{
		close(rd->conn.socket);
		close(rd->conn.ctl_socket);
		return -1;
	}

	return 0;
}

static int rsnd_get_backend_info ( rsound_t *rd )
{
	uint32_t chunk_size_temp;
	int rc;

   struct pollfd fd;
   fd.fd = rd->conn.socket;
   fd.events = POLLIN;

   if ( poll(&fd, 1, 500) < 0 )
   {
      close(rd->conn.socket);
      close(rd->conn.ctl_socket);
      return -1;
   }

   if ( fd.revents & POLLHUP )
   {
      close(rd->conn.socket);
      close(rd->conn.ctl_socket);
      return -1;
   }

	rc = recv(rd->conn.socket, &chunk_size_temp, sizeof(uint32_t), 0);
	if ( rc != sizeof(uint32_t))
	{
		close(rd->conn.socket);
      close(rd->conn.ctl_socket);
		return -1;
	}

	chunk_size_temp = ntohl(chunk_size_temp);

   rd->chunk_size = chunk_size_temp;
   
   if ( rd->buffer_size <= 0 )
      rd->buffer_size = rd->chunk_size * 32;
   if ( rd->buffer_size < rd->chunk_size )
      rd->buffer_size = rd->chunk_size;

	rd->buffer = realloc ( rd->buffer, rd->buffer_size );
	rd->buffer_pointer = 0;

	return 0;
}
static int rsnd_create_connection(rsound_t *rd)
{
	int rc;

   if ( rd->conn.socket < 0 && rd->conn.ctl_socket < 0 )
   {
      rc = rsnd_connect_server(rd);
      if (rc < 0)
      {
         close(rd->conn.socket);
         close(rd->conn.ctl_socket);
         rd->conn.socket = -1;
         rd->conn.ctl_socket = -1;
         return -1;
      }
   }
   if ( !rd->ready_for_data )
   {
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

      rd->ready_for_data = 1;
      rc = rsnd_start_thread(rd);
      if (rc < 0)
      {
         rsd_stop(rd);
         return -1;
      }
   }
	
   return 0;
}

static int rsnd_send_chunk(int socket, char* buf, size_t size)
{
	int rc = 0;
   size_t wrote = 0;
   size_t send_size = 0;
   struct pollfd fd;
   fd.fd = socket;
   fd.events = POLLOUT;

   while ( wrote < size )
   {
      if ( poll(&fd, 1, 500) < 0 )
         return 0;

      if ( fd.revents & POLLHUP )
         return 0;

      send_size = (size - wrote) > 1024 ? 1024 : size - wrote;
	   rc = send(socket, buf + wrote, send_size, 0);
      if ( rc <= 0 )
         return 0;

      wrote += rc;
   }
	return wrote;
}

static void rsnd_drain(rsound_t *rd)
{
	if ( rd->has_written )
	{
		int64_t temp, temp2;

		struct timespec now_tv;
#ifdef _POSIX_MONOTONIC_CLOCK
		clock_gettime(CLOCK_MONOTONIC, &now_tv);
#else
      clock_gettime(CLOCK_REALTIME, &now_tv);
#endif
		
		temp = (int64_t)now_tv.tv_sec - (int64_t)rd->start_tv.tv_sec;
		temp *= rd->rate * rd->channels * 2;

		temp2 = (int64_t)now_tv.tv_nsec - (int64_t)rd->start_tv.tv_nsec;
		temp2 *= rd->rate * rd->channels * 2;
		temp2 /= 1000000000;
		temp += temp2;

      rd->bytes_in_buffer = (int)((int64_t)rd->total_written + (int64_t)rd->buffer_pointer - temp);
   }
	else
      rd->bytes_in_buffer = rd->buffer_pointer;
}

static int rsnd_fill_buffer(rsound_t *rd, const char *buf, size_t size)
{
   if ( !rd->thread_active )
   {
      return -1;
   }

   /* Wait until we have a ready buffer */
   for (;;)
   {
      pthread_mutex_lock(&rd->thread.mutex);
      if (rd->buffer_pointer + (int)size <= (int)rd->buffer_size  )
      {
         pthread_mutex_unlock(&rd->thread.mutex);
         break;
      }
      pthread_mutex_unlock(&rd->thread.mutex);

      /* get signal from thread to check again */
      pthread_mutex_lock(&rd->thread.cond_mutex);
      pthread_cond_wait(&rd->thread.cond, &rd->thread.cond_mutex);
      pthread_mutex_unlock(&rd->thread.cond_mutex);
   }

   pthread_mutex_lock(&rd->thread.mutex);
   memcpy(rd->buffer + rd->buffer_pointer, buf, size);
   rd->buffer_pointer += (int)size;
   pthread_mutex_unlock(&rd->thread.mutex);

   /* send signal to thread that buffer has been updated */
   pthread_cond_signal(&rd->thread.cond);

   return size;
}

static int rsnd_start_thread(rsound_t *rd)
{
   int rc;
   if ( !rd->thread_active )
   {
      rc = pthread_create(&rd->thread.threadId, NULL, rsnd_thread, rd);
      if ( rc != 0 )
         return -1;
      rd->thread_active = 1;
      return 0;
   }
   else
      return 0;
}

static int rsnd_stop_thread(rsound_t *rd)
{
   int rc;
   if ( rd->thread_active )
   {
      rc = pthread_cancel(rd->thread.threadId);
      pthread_join(rd->thread.threadId, NULL);
      pthread_mutex_unlock(&rd->thread.mutex);
      pthread_mutex_unlock(&rd->thread.cond_mutex);
      if ( rc != 0 )
         return -1;
      rd->thread_active = 0;
      return 0;
   }
   else
      return 0;
}

static int rsnd_get_delay(rsound_t *rd)
{
   pthread_mutex_lock(&rd->thread.mutex);
   rsnd_drain(rd);
   int ptr = rd->bytes_in_buffer;
   pthread_mutex_unlock(&rd->thread.mutex);
   return ptr;
}

static int rsnd_get_ptr(rsound_t *rd)
{
   pthread_mutex_lock(&rd->thread.mutex);
   int ptr = rd->buffer_pointer;
   pthread_mutex_unlock(&rd->thread.mutex);

   return ptr;
}

static void* rsnd_thread ( void * thread_data )
{
   rsound_t *rd = thread_data;
   int rc;
   struct timespec now;
   int nsecs;

   /* Plays back data as long as there is data in the buffer */
   for (;;)
   {
      while ( rd->buffer_pointer >= (int)rd->chunk_size )
      {
         rc = rsnd_send_chunk(rd->conn.socket, rd->buffer, rd->chunk_size);
         if ( rc <= 0 )
         {
            rsd_stop(rd);
            /* Buffer has terminated, signal fill_buffer */
            pthread_exit(NULL);
         }
         
         if ( !rd->has_written )
         {
            pthread_mutex_lock(&rd->thread.mutex);
#ifdef _POSIX_MONOTONIC_CLOCK
            clock_gettime(CLOCK_MONOTONIC, &rd->start_tv);
#else
            clock_gettime(CLOCK_REALTIME, &rd->start_tv);
#endif
            rd->has_written = 1;
            pthread_mutex_unlock(&rd->thread.mutex);
         }

         pthread_mutex_lock(&rd->thread.mutex);
         rd->total_written += rc;
         pthread_mutex_unlock(&rd->thread.mutex);

         pthread_mutex_lock(&rd->thread.mutex);
         memmove(rd->buffer, rd->buffer + rd->chunk_size, rd->buffer_size - rd->chunk_size);
         rd->buffer_pointer -= (int)rd->chunk_size;
         pthread_mutex_unlock(&rd->thread.mutex);

         /* Buffer has decreased, signal fill_buffer */
         pthread_cond_signal(&rd->thread.cond);

                          
      }
      /* Wait for the buffer to be filled. Test at least every 5ms. */
      clock_gettime(CLOCK_REALTIME, &now);
      nsecs = 5000000;      
      now.tv_nsec += nsecs;
      if ( now.tv_nsec >= 1000000000 )
      {
         now.tv_sec++;
         now.tv_nsec -= 1000000000;
      }

      pthread_mutex_lock(&rd->thread.cond_mutex);
      pthread_cond_timedwait(&rd->thread.cond, &rd->thread.cond_mutex, &now);
      pthread_mutex_unlock(&rd->thread.cond_mutex);
   }
}

int rsd_stop(rsound_t *rd)
{
   rsnd_stop_thread(rd);

   const char buf[] = "CLOSE";

   send(rd->conn.ctl_socket, buf, strlen(buf) + 1, 0);
   close(rd->conn.ctl_socket);
   close(rd->conn.socket);
   
   rd->conn.socket = -1;
   rd->conn.ctl_socket = -1;
   rd->has_written = 0;
   rd->ready_for_data = 0;
   rd->buffer_pointer = 0;
   rd->has_written = 0;
   rd->total_written = 0;

   return 0;
}

int rsd_write( rsound_t *rsound, const char* buf, size_t size)
{
   int result;
   
   result = rsnd_fill_buffer(rsound, buf, size);

   if ( result <= 0 )
   {
      rsd_stop(rsound);
      return -1;
   }
   return result;
}

int rsd_start(rsound_t *rsound)
{
   if ( rsound->rate == 0 || rsound->channels == 0 || rsound->host == NULL || rsound->port == NULL )
      return -1;
   return ( rsnd_create_connection(rsound) );
}

int rsd_set_param(rsound_t *rd, int option, void* param)
{
   switch(option)
   {
      case RSD_SAMPLERATE:
         rd->rate = *((int*)param);
         break;
      case RSD_CHANNELS:
         rd->channels = *((int*)param);
         break;
      case RSD_HOST:
         if ( rd->host )
            free(rd->host);
         rd->host = strdup((char*)param);
         break;
      case RSD_PORT:
         if ( rd->port )
            free(rd->port);
         rd->port = strdup((char*)param);
      case RSD_BUFSIZE:
         rd->buffer_size = *((int*)param);
         break;
      default:
         return -1;
   }
   return 0;
         
}

int rsd_pointer(rsound_t *rsound)
{
   int ptr;

   ptr = rsnd_get_ptr(rsound);	

   return ptr;
}

int rsd_get_avail(rsound_t *rd)
{
   int ptr;
   ptr = rsnd_get_ptr(rd);
   return rd->buffer_size - ptr;
}

int rsd_delay(rsound_t *rd)
{
   int ptr = rsnd_get_delay(rd);
   if ( ptr < 0 )
      ptr = 0;
   
   return ptr;
}

int rsd_pause(rsound_t* rsound, int enable)
{
   if ( enable )
      rsd_stop(rsound);
   else
      rsd_start(rsound);
   return 0;
}

int rsd_init(rsound_t** rsound)
{
	*rsound = calloc(1, sizeof(rsound_t));
	if ( *rsound == NULL )
	{
		return -1;
	}
   
   (*rsound)->conn.socket = -1;
   (*rsound)->conn.ctl_socket = -1;

   pthread_mutex_init(&(*rsound)->thread.mutex, NULL);
   pthread_mutex_init(&(*rsound)->thread.cond_mutex, NULL);
   pthread_cond_init(&(*rsound)->thread.cond, NULL);

   return 0;
}

int rsd_free(rsound_t *rsound)
{
   if (rsound)
   {
      if (rsound->buffer)
         free(rsound->buffer);
      if (rsound->host)
         free(rsound->host);
      if (rsound->port)
         free(rsound->port);
      free(rsound);
      return 0;
   }
   else
      return -1;
}

