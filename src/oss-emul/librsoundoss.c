#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include <rsound.h>
#include <stdarg.h>

#define FD_MAX 16

static int fd_open = 0;

struct rsd_oss
{
   int fd; // Fake fd that is returned to the API caller. This will be duplicated.
   rsound_t *rd;
   flags_t flags;
} _rd[16];

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

   // Now we should dup2 :D
   if ( dup2(rd->conn.socket, fd) < 0 )
      return -1;
}


struct os_calls
{
   int (*open)(const char*, int, mode_t);
   int (*close)(int);
   int (*ioctl)(int, int, ...);
   size_t (*write)(int, const void*, size_t);
} _os;

static void init_lib(void)
{
   int i;
   if ( fd_open == 0 ) // We need to init the lib
   {
      memset(_rd, 0, sizeof(_rd));

      for ( i = 0; i < FD_MAX; i++ )
      {
         _rd[i].fd = -1;
      }

      memset(_os, 0, sizeof(_os));

      // Let's open the real calls from LIBC
      
      _os.open = dlsym(REAL_LIBC, "open");
      _os.close = dlsym(REAL_LIBC, "close");
      _os.ioctl = dlsym(REAL_LIBC, "ioctl");
      _os.write = dlsym(REAL_LIBC, "write");
   
      fd_open++;
   }

}

static void close_lib(void)
{
   if ( fd_open == 0 )
      return;

   fd_open--;
}

int open(const char* path, int flags, mode_t mode)
{
   init_lib();

   if ( strcmp(path, "/dev/dsp") )
      return _os.open(path, flags, mode); // We route the call to the OS.

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

   int fds[2];
   if ( pipe(fds) < 0 )
   {
      rsd_free(_rd[i].rd);
      return -1;
   }

   close(fds[1]);
   _rd[i].fd = fds[0];

   return fds[0];
}

size_t write(int fd, const void* buf, size_t count)
{
   rsound_t *rd;

   rd = fd2handle(fd);

   if ( rd == NULL )
   {
      return _os.write(fd, buf, count);
   }

   // We now need a working connection.
   if ( start_rsd(fd, rd) < 0 )
      return -1;

   // Now we can write.
   return rsd_write(rd, buf, count);
}

int close(int fd)
{
   rsound_t *rd;

   rd = fd2handle(fd);
   int i = fd2index(fd);

   if ( rd == NULL )
   {
      return _os.close(fd);
   }

   rsd_stop(rd);
   rsd_free(rd);
   _rd[i].rd = NULL;
   _rd[i].fd = -1;

   close_lib();

   return 0;
}

int ioctl(int fd, int request, ...)
{
   va_list args;
   void* argp;
   va_start(args, request);
   argp = va_arg(args, void*);
   va_end(args);

   rsound_t *rd;

   rd = fd2handle(fd);
   if ( rd == NULL )
   {
      return _os.ioctl(fd, request, argp);
   }

   switch(request)
   {
      case SNDCTL_DSP_SETFMT:
         int format = ossfmt2rsd(*(int*)argp);
         rsd_set_param(rd, RSD_FORMAT, &format);
         break;

      case SNDCTL_DSP_STEREO:
         int channels = (*(int*)argp == 1) ? 2 : 1;
         rsd_set_param(rd, RSD_CHANNELS, &channels);
         break;

      case SNDCTL_DSP_SPEED:
         int rate = *(int*)argp;
         rsd_set_param(rd, RSD_SAMPLERATE, &rate);
         break;

      case SNDCTL_DSP_GETOSPACE:
         audio_buf_info *zz = argp;
         zz->fragsize = 512;
         break;

      case SNDCTL_DSP_GETODELAY:
         *(int*)argp = (int) rsd_delay(rd);
         break;

   }

   return 0;
}




