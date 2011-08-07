//muroario.c:

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
#include <unistd.h>

#ifdef __WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif

/* The following IO functions write or read data in a loop
 * unless a they wrote or read all the requested size or
 * IO error occurs.
 * This may be needed as underlying IO layer may only work
 * with sizes less than the one we try to use or got interrupted
 * while the operation or something else happens leading in
 * incomplete processing of the block of data.
 * This was first written because of bugs in win32's network
 * stack.
 */

ssize_t muroar_write  (int fh, const void * buf, size_t len) {
 ssize_t ret;
 ssize_t done = 0;

 if ( fh == -1 || buf == NULL )
  return -1;

 while (len) {
  if ( (ret = send(fh, buf, len, 0)) < 1 )
   return done;

  done += ret;
  buf   = (const char *)buf + ret;
  len  -= ret;
 }

 return done;
}

ssize_t muroar_read   (int fh,       void * buf, size_t len) {
 ssize_t ret;
 ssize_t done = 0;

 if ( fh == -1 || buf == NULL )
  return -1;

 while (len) {
  if ( (ret = recv(fh, buf, len, 0)) < 1 )
   return done;

  done += ret;
  buf   = (char *)buf + ret;
  len  -= ret;
 }

 return done;
}

//ll
