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

#include "proto.h"
#include "endian.h"
#include "audio.h"
#include <poll.h>

extern const rsd_backend_callback_t *backend;

typedef struct rsd_proto
{
   int proto;
   int64_t client_ptr;
   int64_t serv_ptr;
   char identity[256];
} rsd_proto_t;

static int get_proto(rsd_proto_t *proto, char *rsd_proto_header);
static int send_proto(int ctl_sock, rsd_proto_t *proto);

// Here we handle all requests from the client that are available in the network buffer. We are using non-blocking socket.
// If recv() returns less than we expect, we bail out as there is not more data to be read.
int handle_ctl_request(connection_t *conn, void *data)
{

   char rsd_proto_header[RSD_PROTO_MAXSIZE + 1];
   rsd_proto_t proto;

   struct pollfd fd = {
      .fd = conn->ctl_socket,
      .events = POLLIN
   };

   int rc;
   for(;;)
   {
      if ( poll(&fd, 1, 0) < 0 )
      {
         perror("poll");
         return -1;
      }

      if ( !(fd.revents & POLLIN) )
      {
         // We're done here.
         return 0;
      }

      memset(rsd_proto_header, 0, sizeof(rsd_proto_header));
      rc = recv(conn->ctl_socket, rsd_proto_header, RSD_PROTO_CHUNKSIZE, 0);

      if ( rc <= 0 )
      {
         return 0;
      }

      char *substr;
      // Makes sure we have a valid header before reading any more.
      if ( (substr = strstr(rsd_proto_header, "RSD")) == NULL )
      {
         continue;
      }

      while ( *substr != ' ' && *substr != '\0' )
         substr++;

      // Recieve length on the message from client.
      long int len = strtol(substr, NULL, 0);
      if ( len > RSD_PROTO_MAXSIZE )
      {
         continue;
      }

      memset(rsd_proto_header, 0, sizeof(rsd_proto_header));
      rc = recv(conn->ctl_socket, rsd_proto_header, len, 0);

      if ( rc <= 0 )
         return 0;

      else if ( rc != len )
      {
         // We're done here.
         return 0;
      }

      // Let's parse this.

      if ( get_proto(&proto, rsd_proto_header) < 0 )
         // We recieved invalid proto
         continue;

      switch ( proto.proto )
      {
         case RSD_PROTO_NULL:
            break;
         case RSD_PROTO_STOP:
            return -1;
            break;

         case RSD_PROTO_INFO:
            proto.serv_ptr = conn->serv_ptr;
            if ( backend->latency != NULL )
            {
               proto.serv_ptr -= backend->latency(data);
            }
            if ( send_proto(conn->ctl_socket, &proto) < 0 )
               return -1;
            break;

         case RSD_PROTO_IDENTITY:
            strncpy(conn->identity, proto.identity, sizeof(conn->identity));
            break;

         case RSD_PROTO_CLOSECTL:
            send_proto(conn->ctl_socket, &proto);
            if ( conn->ctl_socket != 0 )
               close(conn->ctl_socket);
            conn->ctl_socket = 0;
            break;

         default:
            return -1;
      }

   }
}

static int get_proto(rsd_proto_t *proto, char *rsd_proto_header)
{
   const char *substr;

   // Jumps forward in the buffer until we hit a valid character as per protocol.
   while ( *rsd_proto_header == ' ' )
      rsd_proto_header++;

   if ( strstr(rsd_proto_header, "NULL") != NULL )
   {
      proto->proto = RSD_PROTO_NULL;
      return 0;
   }

   else if ( strstr(rsd_proto_header, "STOP") != NULL )
   {
      proto->proto = RSD_PROTO_STOP;
      return 0;
   }

   else if ( (substr = strstr(rsd_proto_header, "INFO ")) != NULL )
   {
      proto->proto = RSD_PROTO_INFO;
      // Jump forward after INFO
      rsd_proto_header += 5;
      int64_t client_ptr;
      client_ptr = strtoull(rsd_proto_header, NULL, 10);
      proto->client_ptr = client_ptr;
      return 0;
   }
   else if ( (substr = strstr(rsd_proto_header, "IDENTITY ")) != NULL )
   {
      proto->proto = RSD_PROTO_IDENTITY;
      rsd_proto_header += strlen("IDENTITY ");
      strncpy(proto->identity, rsd_proto_header, sizeof(proto->identity));
      proto->identity[sizeof(proto->identity)-1] = '\0';
      return 0;
   }
   else if ( (substr = strstr(rsd_proto_header, "CLOSECTL")) != NULL )
   {
      proto->proto = RSD_PROTO_CLOSECTL;
      return 0;
   }

   return -1;
}

static int send_proto(int ctl_sock, rsd_proto_t *proto)
{

   struct pollfd fd = {
      .fd = ctl_sock,
      .events = POLLOUT
   };

   if ( poll(&fd, 1, 0) < 0 )
   {
      perror("poll");
      return -1;
   }

   if ( fd.revents & POLLOUT )
   {

      char sendbuf[RSD_PROTO_MAXSIZE] = {0};
      char tempbuf[RSD_PROTO_MAXSIZE] = {0};
      int rc;
      switch ( proto->proto )
      {
         case RSD_PROTO_INFO:
            snprintf(tempbuf, RSD_PROTO_MAXSIZE - 1, " INFO %lld %lld", (long long int)proto->client_ptr, (long long int)proto->serv_ptr);
            snprintf(sendbuf, RSD_PROTO_MAXSIZE - 1, "RSD%5d%s", (int)strlen(tempbuf), tempbuf);
            //fprintf(stderr, "Sent info: \"%s\"\n", sendbuf);
            rc = send(ctl_sock, sendbuf, strlen(sendbuf), 0);
            if ( rc < 0 )
               return -1;
            break;

         case RSD_PROTO_CLOSECTL:
            strncpy(sendbuf, "RSD   12 CLOSECTL OK", sizeof(sendbuf)-1);
            rc = send(ctl_sock, sendbuf, strlen(sendbuf), 0);
            if ( rc < 0 )
               return -1;
            break;
            

         default:
            return -1;
      }
   }
   return 0;
}






