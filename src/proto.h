#include "audio.h"

// Defines protocol for RSound
enum rsd_proto
{
   RSD_PROTO_NULL = 0x0000,
   RSD_PROTO_STOP = 0x0001,
   RSD_PROTO_INFO = 0x0002,
}


int handle_ctl_request(connection_t* conn, void* data);

