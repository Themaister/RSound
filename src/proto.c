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
} rsd_proto_t;

static int get_proto(rsd_proto_t *proto, char *rsd_proto_header);
static int send_proto(int ctl_sock, rsd_proto_t *proto);

// Here we handle all requests from the client that are available in the network buffer. We are using non-blocking socket.
// If recv() returns less than we expect, we bail out as there is not more data to be read.
int handle_ctl_request(connection_t conn, void *data)
{

   char rsd_proto_header[RSD_PROTO_MAXSIZE + 1];
   rsd_proto_t proto;

   struct pollfd fd = {
      .fd = conn.ctl_socket,
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
      rc = recv(conn.ctl_socket, rsd_proto_header, RSD_PROTO_CHUNKSIZE, 0);
      
      if ( rc <= 0 )
      {
         fprintf(stderr, "CTL socket is closed.\n");
         return -1;
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
      rc = recv(conn.ctl_socket, rsd_proto_header, len, 0);


      if ( rc <= 0 )
         return -1;

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
            proto.serv_ptr = conn.serv_ptr;
            //proto.serv_ptr -= backend->latency(data);
            if ( send_proto(conn.ctl_socket, &proto) < 0 )
               return -1;
            break;

         default:
            return -1;
      }

   }
}

static int get_proto(rsd_proto_t *proto, char *rsd_proto_header)
{
   const char *substr;

   // Oops! Looks like we have a broken header.
   if ( strstr(rsd_proto_header, "RSD") != NULL )
      return -1;

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
   return -1;
}

static int send_proto(int ctl_sock, rsd_proto_t *proto)
{
   char sendbuf[RSD_PROTO_MAXSIZE] = {0};
   char tempbuf[RSD_PROTO_MAXSIZE] = {0};
   switch ( proto->proto )
   {
      case RSD_PROTO_INFO:
         snprintf(tempbuf, RSD_PROTO_MAXSIZE - 1, " INFO %lld %lld", (long long int)proto->client_ptr, (long long int)proto->serv_ptr);
         snprintf(sendbuf, RSD_PROTO_MAXSIZE - 1, "RSD%5d%s", (int)strlen(tempbuf), tempbuf);
         //fprintf(stderr, "Sent info: \"%s\"\n", sendbuf);
         int rc = send(ctl_sock, sendbuf, strlen(sendbuf), 0);
         if ( rc < 0 )
            return -1;
         break;

      default:
         return -1;
   }
   return 0;
}
         
         


      
   
