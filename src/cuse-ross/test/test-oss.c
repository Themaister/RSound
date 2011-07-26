#include <sys/soundcard.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

int main(int argc, char *argv[])
{
   assert(argc == 2);
   int fd = open(argv[1], O_WRONLY);
   assert(fd >= 0);

   int param = 44100;
   int rc = ioctl(fd, SNDCTL_DSP_SPEED, &param);
   fprintf(stderr, "SPEED: rc = %d, param = %d\n",
         rc, param);

   param = 2;
   rc = ioctl(fd, SNDCTL_DSP_CHANNELS, &param);
   fprintf(stderr, "CHANNELS: rc = %d, param = %d\n",
         rc, param);

   param = AFMT_S16_LE;
   rc = ioctl(fd, SNDCTL_DSP_SETFMT, &param);
   fprintf(stderr, "FMT: rc = %d\n", rc);

   audio_buf_info info;
   ioctl(fd, SNDCTL_DSP_GETOSPACE, &info);
   fprintf(stderr, "GETOSPACE: rc = %d, bytes = %d, fragments = %d, fragsize = %d, fragstotal = %d\n",
         rc, info.bytes, info.fragments, info.fragsize, info.fragstotal);

   assert(fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) == 0);

   uint8_t buf[1024];
   unsigned cnt = 0;

   for (;;)
   {
      struct pollfd pfd = {
         .fd = fd,
         .events = POLLOUT, // POLLIN for shits 'n giggles.
      };

      if (poll(&pfd, 1, -1) < 0)
         break;

      fprintf(stderr, "poll(): POLLOUT = %d, .events = %u, .revents = %u, Count = %u\n",
            (pfd.revents & POLLOUT) ? 1 : 0,
            pfd.events,
            pfd.revents,
            cnt++);

      if ((~pfd.revents) & POLLOUT)
         continue;

      audio_buf_info info;
      ioctl(fd, SNDCTL_DSP_GETOSPACE, &info);
      size_t avail = sizeof(buf) < (size_t)info.bytes ? sizeof(buf) : (size_t)info.bytes;
      fprintf(stderr, "OSS write avail: %u\n", (unsigned)info.bytes);

      if (read(0, buf, avail) <= 0)
         break;

      if (write(fd, buf, avail) < 0)
         break;
   }

   close(fd);
}
