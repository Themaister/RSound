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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include <rsound.h>
#include <stdarg.h>
#include <sys/soundcard.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

#define FD_MAX 16

#define OSS_FRAGSIZE 512

#if defined(RTLD_NEXT)
#define REAL_LIBC RTLD_NEXT
#else
#define REAL_LIBC ((void*) -1L)
#endif

// Makes sure GCC doesn't refine these as macros
#undef open
#undef open64

static int lib_open = 0;
static int open_generic(const char* path, int largefile, int flags, mode_t mode);

struct rsd_oss
{
   int fd; // Fake fd that is returned to the API caller. This will be duplicated.
   rsound_t *rd;
   int nonblock;
} _rd[FD_MAX];

// Checks if a file descriptor is an rsound fd.
static rsound_t* fd2handle(int fd)
{
   rsound_t *rd = NULL;
   int i;
   for ( i = 0; i < FD_MAX; i++ )
   {
      if ( fd == _rd[i].fd )
         rd = _rd[i].rd;
   }

   return rd;
}

static int fd2index(int fd)
{
   int i;
   for ( i = 0; i < FD_MAX; i++ )
   {
      if ( fd == _rd[i].fd )
         return i;
   }
   return -1;
}

static int start_rsd(int fd, rsound_t *rd)
{
   if ( rd->conn.socket == -1 )
   {
      if ( rsd_start(rd) < 0 )
         return -1;
   }

   // Now we should reroute our socket to fd

   if ( dup2(rd->conn.socket, fd) < 0 )
      return -1;

   return 0;
}


struct os_calls
{
   int (*open)(const char*, int, ...);
   int (*open64)(const char*, int, ...);
   int (*close)(int);
   int (*ioctl)(int, unsigned long int, ...);
   ssize_t (*write)(int, const void*, size_t);
   ssize_t (*read)(int, void*, size_t);
} _os;

static void init_lib(void)
{
   int i;
   if ( lib_open == 0 ) // We need to init the lib
   {
      memset(_rd, 0, sizeof(_rd));

      for ( i = 0; i < FD_MAX; i++ )
      {
         _rd[i].fd = -1;
      }

      memset(&_os, 0, sizeof(_os));

      // Let's open the real calls from LIBC

      assert(_os.open = dlsym(REAL_LIBC, "open"));
      assert(_os.open64 = dlsym(REAL_LIBC, "open64"));
      assert(_os.close = dlsym(REAL_LIBC, "close"));
      assert(_os.ioctl = dlsym(REAL_LIBC, "ioctl"));
      assert(_os.write = dlsym(REAL_LIBC, "write"));
      assert(_os.read = dlsym(REAL_LIBC, "read"));

      lib_open++;
   }

}

static int is_oss_path(const char* path)
{
   const char *oss_paths[] = {
      "/dev/dsp",
      "/dev/audio",
      "/dev/sound/dsp",
      "/dev/sound/audio",
      NULL
   };

   int is_path = 0;

   int i;
   for ( i = 0; oss_paths[i] != NULL ; i++ )
   {
      if ( !strcmp(path, oss_paths[i]) )
      {
         is_path = 1;
         break;
      }
   }

   return is_path;
}

static int open_generic(const char* path, int largefile, int flags, mode_t mode)
{
   if ( path == NULL )
   {
      errno = EFAULT;
      return -1;
   }

   // We should use the OS calls directly.
   if ( !is_oss_path(path) )
   {
      if ( largefile )
         return _os.open64(path, flags, mode);
      else
         return _os.open(path, flags, mode);
   }

   // Let's fake this call! :D
   // Search for a vacant fd

   int pos = -1;
   int i;
   for ( i = 0; i < FD_MAX; i++ )
   {
      if ( _rd[i].fd == -1 )
      {
         pos = i;
         break;
      }
   }

   if ( pos == -1 ) // We couldn't find a vacant fd.
      return -1;

   if ( rsd_init(&_rd[i].rd) < 0 )
      return -1;

   // Sets some sane defaults
   int rate = 44100;
   int channels = 2;
   rsd_set_param(_rd[i].rd, RSD_SAMPLERATE, &rate);
   rsd_set_param(_rd[i].rd, RSD_CHANNELS, &channels);

   int fds[2];
   if ( pipe(fds) < 0 )
   {
      rsd_free(_rd[i].rd);
      _rd[i].rd = NULL;
      return -1;
   }

   close(fds[1]);
   _rd[i].fd = fds[0];

   // Let's check the flags
   if ( flags & O_NONBLOCK )
   {
      _rd[i].nonblock = 1;
   }

   if ( flags & O_RDONLY ) // We do not support this.
   {
      errno = -EINVAL;
      rsd_free(_rd[i].rd);
      _rd[i].rd = NULL;
      return -1;
   }

   return fds[0];
}

int open(const char* path, int flags, ...)
{
   init_lib();
   fprintf(stderr, "open(): %s\n", path);
   mode_t mode = 0;

   if ( flags & O_CREAT )
   {
      va_list args;
      va_start(args, flags);
      mode = va_arg(args, mode_t);
      va_end(args);
   }
   return open_generic(path, 0, flags, mode);
}

int open64(const char* path, int flags, ...)
{
   init_lib();
   fprintf(stderr, "open64(): %s\n", path);
   mode_t mode = 0;
   if ( flags & O_CREAT )
   {
      va_list args;
      va_start(args, flags);
      mode = va_arg(args, mode_t);
      va_end(args);
   }
   return open_generic(path, 1, flags, mode);
}


ssize_t write(int fd, const void* buf, size_t count)
{
   init_lib();

   rsound_t *rd;

   rd = fd2handle(fd);
   int i = fd2index(fd);

   if ( rd == NULL )
   {
      return _os.write(fd, buf, count);
   }

   // We now need a working connection.
   if ( start_rsd(fd, rd) < 0 )
      return -1;

   // Now we can write.

   // Checks for non-blocking.
   size_t write_size = count;
   if ( _rd[i].nonblock )
   {
      if ( (size_t)rsd_get_avail(rd) > count )
         write_size = count;
      else
         write_size = (size_t)rsd_get_avail(rd);
   }

   return rsd_write(rd, buf, write_size);
}

ssize_t read(int fd, void* buf, size_t count)
{
   if ( fd2handle(fd) != NULL )
   {
      errno = EBADF;
      return -1; // Can't read from an rsound socket.
   }
   else
      return _os.read(fd, buf, count);
}

int close(int fd)
{

   init_lib();

   rsound_t *rd;

   rd = fd2handle(fd);
   int i = fd2index(fd);

   if ( rd == NULL )
   {
      return _os.close(fd);
   }

   rsd_stop(rd);
   _os.close(fd);
   rsd_free(rd);
   _rd[i].rd = NULL;
   _rd[i].fd = -1;

   return 0;
}

static int ossfmt2rsd(int format)
{
   switch(format)
   {
      case AFMT_S16_LE:
         return RSD_S16_LE;
      case AFMT_S16_BE:
         return RSD_S16_BE;
      case AFMT_U16_BE:
         return RSD_U16_BE;
      case AFMT_U16_LE:
         return RSD_U16_LE;
      case AFMT_S8:
         return RSD_S8;
      case AFMT_U8:
         return RSD_U8;
   }
   return RSD_S16_LE;
}

int ioctl(int fd, unsigned long int request, ...)
{
   init_lib();

   va_list args;
   void* argp;
   va_start(args, request);
   argp = va_arg(args, void*);
   va_end(args);

   int arg;
   audio_buf_info *zz;

   rsound_t *rd;
   int i;

   rd = fd2handle(fd);
   i = fd2index(fd);
   if ( rd == NULL )
   {
      return _os.ioctl(fd, request, argp);
   }

   switch(request)
   {
      case SNDCTL_DSP_GETFMTS:
         *(int*)argp = AFMT_U8 | AFMT_S8 | AFMT_S16_LE 
            | AFMT_S16_BE | AFMT_U16_LE | AFMT_U16_BE;
         break;

      case SNDCTL_DSP_SETFMT:
         arg = ossfmt2rsd(*(int*)argp);
         rsd_set_param(rd, RSD_FORMAT, &arg);
         if ( arg == RSD_S16_LE )
            *(int*)argp = AFMT_S16_LE;
         break;

      case SNDCTL_DSP_STEREO:
         arg = (*(int*)argp == 1) ? 2 : 1;
         rsd_set_param(rd, RSD_CHANNELS, &arg);
         break;

      case SNDCTL_DSP_CHANNELS:
         arg = *(int*)argp;
         rsd_set_param(rd, RSD_CHANNELS, &arg);
         break;

      case SNDCTL_DSP_RESET:
         rsd_stop(rd);
         break;

      case SNDCTL_DSP_SYNC:
         rsd_stop(rd);
         break;

      case SNDCTL_DSP_SPEED:
         arg = *(int*)argp;
         rsd_set_param(rd, RSD_SAMPLERATE, &arg);
         break;

      case SNDCTL_DSP_GETBLKSIZE:
         *(int*)argp = OSS_FRAGSIZE;
         break;

      case SNDCTL_DSP_GETOSPACE:
         zz = argp;
         zz->fragsize = OSS_FRAGSIZE;
         zz->fragments = rsd_get_avail(rd) / OSS_FRAGSIZE;
         zz->bytes = rsd_get_avail(rd);
         break;

      case SNDCTL_DSP_GETODELAY:
         *(int*)argp = (int) rsd_delay(rd);
         break;

      case SNDCTL_DSP_NONBLOCK:
         _rd[i].nonblock = 1;
         break;

      default:
         break;

   }

   return 0;
}




