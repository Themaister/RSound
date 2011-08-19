//muroar.c:

/*
 *      Copyright (C) Philipp 'ph3-der-loewe' Schafft - 2009,2010
 *
 *  This file is part of µRoar,
 *  a minimalist library to access a RoarAudio Sound Server.
 *
 *  µRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  µRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with µRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "muroar.h"
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h> /* for snprintf() */

#ifdef __WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <errno.h> /* for errno keeping in _CLOSE() */
#define _HAVE_ERRNO
#include <sys/socket.h>
#include <netinet/in.h>
#if defined(AF_UNIX)
#include <sys/un.h>
#endif
#include <netdb.h>
#endif

#ifdef HAVE_LIB_DNET
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#endif

#define MUROAR_IOBUF         128

#define _MU_STR              "\265"
#define _DEFAULT_CLIENT_NAME _MU_STR "Roar Client";

/* Format of RoarAudio Message format 0:
 *
 * | 0      | 1      | 2      | 3      | Byte
 * +--------+--------+--------+--------+------
 * |Version |Command | Stream ID       | 0-3
 * +--------+--------+--------+--------+------
 * | Stream Position                   | 4-7
 * +--------+--------+--------+--------+------
 * | Data Length     | Data ....       | 7-11
 * +--------+--------+--------+--------+------
 *
 * All data is in network byte order.
 * Version         : Version number of message, always 0.
 * Command         : Command, one of MUROAR_CMD_*.
 * Stream ID       : ID of Stream we use this command on,
 *                   or -1 (0xFFFF) on non-stream commands.
 * Stream Position : Position of Stream in units depending on stream type.
                     For waveform streams samples (not frames) played.
 * Data Length     : Length of data in the data part of this message
                     in bytes.
 */

// Win32 needs different close function for sockets because of
// it's broken design.
#ifdef __WIN32
#define _CLOSE(x) closesocket((x))
#else
#define _CLOSE(x) { int __olderr = errno; close((x)); errno = __olderr; }
#endif

// Set errno in case we have it
#ifdef _HAVE_ERRNO
#define _SET_ERRNO(x) (errno = (x))
#else
#define _SET_ERRNO(x)
#endif

// Need to init the network layer on win32 because of it's
// broken design.
#ifdef __WIN32
static void muroar_init_win32 (void) {
 static int inited = 0;
 WSADATA wsadata;

 if ( !inited ) {
  WSAStartup(MAKEWORD(1,1) , &wsadata);
  inited++;
 }
}
#endif

// Open Socket to server.
static int muroar_open_socket (char * server) {
 struct hostent     * he;
 struct sockaddr_in   in;
#if !defined(__WIN32) && defined(AF_UNIX)
 struct sockaddr_un   un;
#endif
 int fh = -1;
#ifdef HAVE_LIB_DNET
 char * buf, * node, * object;
 static char localnode[16] = {0};
 struct dn_naddr      *binaddr;
 struct nodeent       *dp;
#endif

#ifdef __WIN32
 muroar_init_win32();
#endif

 if ( *server == '/' ) {
// Handle AF_UNIX Sockets,
// do not build on broken systems like win32 not
// supporting the AF_UNIX sockets.
#if !defined(__WIN32) && defined(AF_UNIX)
  if ( (fh = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 )
   return -1;

  un.sun_family = AF_UNIX;
  strncpy(un.sun_path, server, sizeof(un.sun_path) - 1);

  if ( connect(fh, (struct sockaddr *)&un, sizeof(struct sockaddr_un)) == -1 ) {
   _CLOSE(fh);
   return -1;
  }

  return fh;
#else
  return -1;
#endif
 } else if ( strstr(server, "::") != NULL ) {
#ifdef HAVE_LIB_DNET
  // alloc a temp buffer so we can change the string at will:
  buf = strdup(server);

  // cut node::object into buf and object
  object  = strstr(buf, "::");
  *object = 0;
   object += 2;

  // use default if we have a zero-size node name:
  if ( *buf == 0 ) {
   if ( !localnode[0] ) {
    if ( (binaddr=getnodeadd()) == NULL) {
     free(buf);
     return -1;
    }

    if ( (dp = getnodebyaddr((char*)binaddr->a_addr, binaddr->a_len, AF_DECnet)) == NULL ) {
     free(buf);
     return -1;
    }

    strncpy(localnode, dp->n_name, sizeof(localnode)-1);
    localnode[sizeof(localnode)-1] = 0;
   }

   node = localnode;
  } else {
   node = buf;
  }

  // use default if we have a zero size object name:
  if ( *object == 0 ) {
   object = MUROAR_OBJECT;
  }

  fh = dnet_conn(node, object, SOCK_STREAM, NULL, 0, NULL, 0);

  // free buffer when we are done.
  free(buf);

  return fh;
#else
  return -1;
#endif
 }

 if ( (he = gethostbyname(server)) == NULL ) {
  return -1;
 }

 if ( (fh = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
  return -1;

 memcpy((struct in_addr *)&in.sin_addr, he->h_addr, sizeof(struct in_addr));

 // TODO: add support to set port to something non-default.
 in.sin_family = AF_INET;
 in.sin_port   = htons(MUROAR_PORT);

 if ( connect(fh, (const struct sockaddr *)&in, sizeof(in)) == -1 ) {
  _CLOSE(fh);
  return -1;
 }

 return fh;
}

// Open Control connection
int muroar_connect(char * server, char * name) {
 char   useraddr[80] = "/invalid";
 char * addr[] = {useraddr, MUROAR_GSOCK, MUROAR_HOST, NULL};
 unsigned char buf[MUROAR_IOBUF];
 uint16_t tmpu16;
 int fh = -1;
 int i;
#if !defined(__WIN32)
 ssize_t len;
#endif

 // Prepare server address:
 if ( server != NULL && *server == 0 )
  server = NULL;

 if ( server == NULL )
  server = getenv("ROAR_SERVER");

#if !defined(__WIN32)
 if ( server == NULL ) {
  if ( (len = readlink("/etc/roarserver", useraddr, sizeof(useraddr))) != -1 ) {
   useraddr[len < (ssize_t)sizeof(useraddr) ? (size_t)len : sizeof(useraddr)-1] = 0;
   server = useraddr;
  }
 }
#endif

 // Connect to server:
 if ( server != NULL ) {
  if ( (fh = muroar_open_socket(server)) == -1 )
   return -1;
 } else {
  // build string for ~/.roar
  snprintf(useraddr, sizeof(useraddr), "/%s/.roar", getenv("HOME"));
  useraddr[sizeof(useraddr)-1] = 0;

  // go thru list of possible defaults:
  for (i = 0; fh == -1 && addr[i] != NULL; i++) {
   fh = muroar_open_socket(addr[i]);
  }

  if ( fh == -1 ) {
   _SET_ERRNO(ENOENT);
   return -1;
  }
 }

 // Prepare client name:
 if ( name == NULL || *name == 0 )
  name = _DEFAULT_CLIENT_NAME;

 // Send IDENTIFY command to server:
 memset(buf, 0, sizeof(buf));
 buf[1] = MUROAR_CMD_IDENTIFY;

 // Calculate the length for the data part of the package.
 // Its 5 bytes plus the length of the name string.
 tmpu16 = strlen(name) + 5;

 // check if we have space for 5 bytes + length of name + tailing \0
 // in the buffer.
 if ( tmpu16 >= MUROAR_IOBUF ) {
  _SET_ERRNO(EINVAL);
  return -1;
 }

 buf[8] = (tmpu16 & 0xFF00) >> 8;
 buf[9] = (tmpu16 & 0x00FF);

 if ( muroar_write(fh, buf, 10) != 10 ) {
  _CLOSE(fh);
  return -1;
 }

 buf[0] = 1;
 *(uint32_t*)(&(buf[1])) = htonl(getpid());

 // sizes are already checked.
 strcpy((char*)&(buf[5]), name);

 if ( muroar_write(fh, buf, tmpu16) != tmpu16 ) {
  _CLOSE(fh);
  return -1;
 }

 if ( muroar_read(fh, buf, 10) != 10 ) {
  _CLOSE(fh);
  return -1;
 }

 if ( buf[1] != MUROAR_CMD_OK ) {
  _CLOSE(fh);
  _SET_ERRNO(EACCES);
  return -1;
 }

 // Send AUTH command to server:
 // We send zero-byte AUTH command
 // (type=NONE).
 memset(buf, 0, 10);
 buf[1] = MUROAR_CMD_AUTH;

 if ( muroar_write(fh, buf, 10) != 10 ) {
  _CLOSE(fh);
  return -1;
 }

 if ( muroar_read(fh, buf, 10) != 10 ) {
  _CLOSE(fh);
  return -1;
 }

 if ( buf[1] != MUROAR_CMD_OK ) {
  _CLOSE(fh);
  _SET_ERRNO(EACCES);
  return -1;
 }

 // We now have a working control connection, return it.
 return fh;
}

// Close a data connection
int muroar_close(int fh) {

 if ( fh == -1 ) {
  _SET_ERRNO(EBADF);
  return -1;
 }

 _CLOSE(fh);

 return 0;
}

// Close a control connection by sending QUIT command.
int muroar_quit   (int fh) {
 char quit[] = "\0\6\0\0\0\0\0\0\0\0"; // QUIT command
 int ret = 0;

 if ( fh == -1 ) {
  _SET_ERRNO(EBADF);
  return -1;
 }

 if ( muroar_write(fh, quit, 10) != 10 )
  ret = -1;

 // read in case the server response
 // ignore errors as the server do not necessary
 // response to our request.
 muroar_read(fh, quit, 10);

 if ( muroar_close(fh) == -1 )
  ret = -1;

 return ret;
}

// Open a Stream
int muroar_stream(int fh, int dir, int * stream, int codec, int rate, int channels, int bits) {
 unsigned char buf[24];
 uint16_t sid;
 uint32_t * data = (uint32_t*)buf;
 int i;

 if ( fh == -1 ) {
  _SET_ERRNO(EBADF);
  return -1;
 }

 // Send NEW_STREAM command:
 memset(buf, 0, 10);
 buf[1] = MUROAR_CMD_NEW_STREAM;
 buf[9] = 24; // 6 * int32 = 24 Byte

 if ( muroar_write(fh, buf, 10) != 10 ) {
  _CLOSE(fh);
  return -1;
 }

 data[0] = dir;      // Stream Direction
 data[1] = -1;       // Rel Pos ID
 data[2] = rate;     // Sample Rate
 data[3] = bits;     // Bits per Sample
 data[4] = channels; // Number of Channels
 data[5] = codec;    // Used Codec

 for (i = 0; i < 6; i++)
  data[i] = htonl(data[i]);

 if ( muroar_write(fh, buf, 24) != 24 ) {
  _CLOSE(fh);
  return -1;
 }

 if ( muroar_read(fh, buf, 10) != 10 ) {
  _CLOSE(fh);
  return -1;
 }

 if ( buf[1] != MUROAR_CMD_OK ) {
  _CLOSE(fh);
  _SET_ERRNO(EINVAL);
  return -1;
 }

 // Stream ID of new stream is in byte 2 and 3 of header,
 // encoded in network byte order.
 sid = (buf[2] << 8) | buf[3];

 // Send EXEC_STREAM command:
 memset(buf, 0, 10);
 buf[1] = MUROAR_CMD_EXEC_STREAM;

 // set Stream ID:
 buf[2] = (sid & 0xFF00) >> 8;
 buf[3] = (sid & 0x00FF);

 if ( muroar_write(fh, buf, 10) != 10 ) {
  _CLOSE(fh);
  return -1;
 }

 if ( muroar_read(fh, buf, 10) != 10 ) {
  _CLOSE(fh);
  return -1;
 }

 if ( buf[1] != MUROAR_CMD_OK ) {
  _CLOSE(fh);
  return -1;
 }

 // Set Stream ID in case caller want to know (passed non-NULL):
 if ( stream != NULL )
  *stream = sid;

 // we converted the control connection to a stream connection,
 // return it.
 return fh;
}

//ll
