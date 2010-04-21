#include "proto.h"
#include "endian.h"
#include "audio.h"

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
int handle_ctl_request(connection_t* conn, void *data)
{

   fprintf(stderr, "Enter handle_ctl_req.\n");

   char rsd_proto_header[RSD_PROTO_MAXSIZE + 1];
   rsd_proto_t proto;

   int rc;
   for(;;)
   {
      memset(rsd_proto_header, 0, sizeof(rsd_proto_header));
      rc = recv(conn->ctl_socket, rsd_proto_header, RSD_PROTO_CHUNKSIZE, 0);

      fprintf(stderr, "Read data.\n");

      if ( rc < 0 )
      {
         fprintf(stderr, ":<\n");
         return -1;
      }

      else if ( rc == 0 )
      {
         fprintf(stderr, ":::<\n");
         // We're done here.
         return 0;
      }

      fprintf(stderr, "Recieved small header: \"%s\"\n", rsd_proto_header); 

      char *substr;
      // Makes sure we have a valid header before reading any more.
      if ( (substr = strstr("RSD", rsd_proto_header)) == NULL )
         continue;

      // Recieve length on the message from client.
      long int len = strtol(substr, NULL, 0);
      if ( len > RSD_PROTO_MAXSIZE )
         continue;

      memset(rsd_proto_header, 0, sizeof(rsd_proto_header));
      rc = recv(conn->ctl_socket, rsd_proto_header, len, 0);

      if ( rc < 0 )
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
            proto.serv_ptr = conn->serv_ptr;
            //proto.serv_ptr -= backend->latency(data);
            if ( send_proto(conn->ctl_socket, &proto) < 0 )
               return -1;
            break;

         default:
            return -1;
      }

   }
}

static int get_proto(rsd_proto_t *proto, char *rsd_proto_header)
{
   if ( strcmp("RSD", rsd_proto_header) != 0 )
      return -1;

   while ( *rsd_proto_header == ' ' )
      rsd_proto_header++;

   if ( strcmp("NULL", rsd_proto_header) == 0 )
   {
      proto->proto = RSD_PROTO_NULL;
      return 0;
   }

   else if ( strcmp("STOP", rsd_proto_header) == 0 )
   {
      proto->proto = RSD_PROTO_STOP;
      return 0;
   }

   else if ( strcmp("INFO", rsd_proto_header) == 0 )
   {
      proto->proto = RSD_PROTO_INFO;
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
         snprintf(tempbuf, RSD_PROTO_MAXSIZE - 1, "INFO %lld %lld", proto->client_ptr, proto->serv_ptr);
         snprintf(sendbuf, RSD_PROTO_MAXSIZE - 1, "RSD%5d %s", (int)strlen(tempbuf), tempbuf);
         int rc = send(ctl_sock, sendbuf, strlen(sendbuf), 0);
         if ( rc < 0 )
            return -1;
         break;

      default:
         return -1;
   }
   return 0;
}
         
         


      
   
