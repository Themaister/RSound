#include "proto.h"
#include "endian.h"
#include "audio.h"

extern const rsd_backend_callback_t *backend;

typedef rsd_proto
{
   int proto;
   int64_t client_ptr;
   int64_t serv_ptr;
} rsd_proto_t;


// Here we handle all requests from the client that are available in the network buffer. We are using non-blocking socket.
// If recv() returns less than we expect, we bail out as there is not more data to be read.
int handle_ctl_request(connection_t conn, void *data)
{
   char rsd_proto_header[RSD_PROTO_MAXSIZE];
   rsd_proto_t proto;

   int rc;
   for(;;)
   {
      memset(rsd_proto_header, 0, sizeof(rsd_proto_header));
      rc = recv(conn.ctl_sock, rsd_proto_header, RSD_PROTO_MAXSIZE, 0);

      if ( rc < 0 )
         return -1;

      else if ( rc == 0 )
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
            proto.serv_ptr -= backend->latency(data);
            if ( send_proto(ctl_sock, data, &proto) < 0 )
               return -1;
            break;

         default:
            return -1;
      }

   }
}

      
   
