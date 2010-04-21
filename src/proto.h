#ifndef _PROTO_H
#define _PROTO_H

#include "audio.h"

#define RSD_PROTO_CHUNKSIZE 8
#define RSD_PROTO_MAXSIZE 256

// Defines protocol for RSound
enum
{
   RSD_PROTO_NULL = 0x0000,
   RSD_PROTO_STOP = 0x0001,
   RSD_PROTO_INFO = 0x0002
};

int handle_ctl_request(connection_t* conn, void* data);

#endif
