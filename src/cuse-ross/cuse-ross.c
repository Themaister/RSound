#include <string.h>
#include <cuse_lowlevel.h>
#include <fuse_opt.h>
#include <rsound.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <sys/poll.h>
#include <signal.h>

#include <sys/soundcard.h>

#include "../librsound/buffer.h"

#define ROSS_DECL ross_t *ro = (ross_t*)info->fh

#define FRAGSIZE 512
#define FRAGS 64

typedef struct ross
{
   rsound_t *rd;
   bool started;
   bool nonblock;

   pthread_mutex_t poll_lock;
   struct fuse_pollhandle *ph;

   pthread_mutex_t cond_lock;
   pthread_cond_t cond;

   int channels;
   int rate;
   int fragsize;
   int frags;
   int bufsize;
   int bps;

   volatile sig_atomic_t error;

   unsigned write_cnt;

   rsound_fifo_buffer_t *buffer;
} ross_t;

static int ross_latency(ross_t *ro)
{
   if (!ro->started)
      return 0;

   int ret = rsd_delay(ro->rd);
   rsd_callback_lock(ro->rd);
   ret += rsnd_fifo_read_avail(ro->buffer);
   rsd_callback_unlock(ro->rd);
   return ret;
}

static void ross_close(ross_t *ro)
{
   if (ro->started)
      usleep(1000000LLU * ross_latency(ro) / ro->bps);

   rsd_stop(ro->rd);
   rsd_free(ro->rd);

   if (ro->buffer)
      rsnd_fifo_free(ro->buffer);
}

static void ross_reset(ross_t *ro)
{
   rsd_stop(ro->rd);
   ro->started = false;
   ro->error = 0;

   if (ro->buffer)
   {
      rsnd_fifo_free(ro->buffer);
      ro->buffer = NULL;
   }
}

static void ross_release(fuse_req_t req, struct fuse_file_info *info)
{
   ROSS_DECL;
   
   ross_close(ro);

   pthread_mutex_destroy(&ro->poll_lock);
   pthread_mutex_destroy(&ro->cond_lock);
   pthread_cond_destroy(&ro->cond);

   if (ro->ph)
      fuse_pollhandle_destroy(ro->ph);

   free(ro);
   fuse_reply_err(req, 0);
}

static void ross_notify(ross_t *ro)
{
   pthread_mutex_lock(&ro->poll_lock);
   if (ro->ph)
   {
      fuse_lowlevel_notify_poll(ro->ph);
      fuse_pollhandle_destroy(ro->ph);
      ro->ph = NULL;
   }
   pthread_mutex_unlock(&ro->poll_lock);
   pthread_cond_signal(&ro->cond);
}

static void ross_update_notify(ross_t *ro, struct fuse_pollhandle *ph)
{
   pthread_mutex_lock(&ro->poll_lock);
   struct fuse_pollhandle *tmp_ph = ro->ph;
   ro->ph = ph;
   pthread_mutex_unlock(&ro->poll_lock);
   if (tmp_ph)
      fuse_pollhandle_destroy(tmp_ph);
}

static unsigned ross_write_avail(ross_t *ro)
{
   if (!ro->started)
      return ro->bufsize;

   rsd_callback_lock(ro->rd);
   unsigned ret = rsnd_fifo_write_avail(ro->buffer);
   rsd_callback_unlock(ro->rd);
   return ret;
}

static int ross_read_avail(ross_t *ro)
{
   if (!ro->started)
      return 0;

   rsd_callback_lock(ro->rd);
   unsigned ret = rsnd_fifo_read_avail(ro->buffer);
   rsd_callback_unlock(ro->rd);
   return ret;
}


static ssize_t ross_audio_cb(void *data, size_t bytes, void *userdata)
{
   ross_t *ro = userdata;
   ssize_t ret = rsnd_fifo_read_avail(ro->buffer);
   if (ret > bytes)
      ret = bytes;

   if (ret > 0)
   {
      rsnd_fifo_read(ro->buffer, data, ret);
      ross_notify(ro);
   }

   return ret;
}

static void ross_err_cb(void *data)
{
   ross_t *ro = data;
   ro->error = 1;
   ross_notify(ro);
}

static void ross_open(fuse_req_t req, struct fuse_file_info *info)
{
   if ((info->flags & (O_WRONLY | O_RDONLY | O_RDWR)) != O_WRONLY)
   {
      fuse_reply_err(req, EACCES);
      return;
   }

   ross_t *ro = calloc(1, sizeof(*ro));
   if (!ro)
   {
      fuse_reply_err(req, ENOMEM);
      return;
   }

   rsound_t *rd;
   if (rsd_init(&rd) < 0)
   {
      free(ro);
      fuse_reply_err(req, ENOMEM);
      return;
   }

   if (pthread_mutex_init(&ro->poll_lock, NULL) < 0 ||
         pthread_mutex_init(&ro->cond_lock, NULL) < 0 ||
         pthread_cond_init(&ro->cond, NULL) < 0)
   {
      free(ro);
      rsd_free(rd);
      fuse_reply_err(req, ENOMEM);
      return;
   }
   
   // Use some different defaults than regular OSS for convenience. :D
   ro->rate = 44100;
   ro->channels = 2;
   int format = RSD_S16_LE;

   rsd_set_param(rd, RSD_CHANNELS, &ro->channels);
   rsd_set_param(rd, RSD_SAMPLERATE, &ro->rate);
   rsd_set_param(rd, RSD_FORMAT, &format);

   ro->frags = FRAGS;
   ro->fragsize = FRAGSIZE;
   ro->bufsize = FRAGS * FRAGSIZE;

   ro->rd = rd;
   info->fh = (uint64_t)ro;
   info->nonseekable = 1;
   info->direct_io = 1;

   fuse_reply_open(req, info);
}

static int ross_start(ross_t *ro)
{
   ro->bps = ro->channels * ro->rate * rsd_samplesize(ro->rd);
   int latency = (1000 * ro->bufsize) / ro->bps;
   if (latency < 32)
      latency = 32;

   rsd_set_param(ro->rd, RSD_LATENCY, &latency);
   rsd_set_callback(ro->rd, ross_audio_cb, ross_err_cb, ro->fragsize, ro);

   ro->buffer = rsnd_fifo_new(ro->bufsize);
   if (!ro->buffer)
      return -1;

   if (rsd_start(ro->rd) < 0)
      return -1;

   ro->started = true;
   return 0;
}

static void ross_write(fuse_req_t req, const char *data, size_t size, off_t off, struct fuse_file_info *info)
{
   ROSS_DECL;

   if (size == 0)
   {
      fuse_reply_write(req, 0);
      return;
   }

   if (ro->error)
   {
      fuse_reply_err(req, EPIPE);
      return;
   }

   if (!ro->started && (ross_start(ro) < 0))
   {
      fuse_reply_err(req, EIO);
      return;
   }

   bool nonblock = ro->nonblock || (info->flags & O_NONBLOCK);
   size_t written = 0;
   do
   {
      rsd_callback_lock(ro->rd);
      size_t write_avail = rsnd_fifo_write_avail(ro->buffer);

      if (write_avail > size)
         write_avail = size;

      if (write_avail > 0)
         rsnd_fifo_write(ro->buffer, data, write_avail);
      rsd_callback_unlock(ro->rd);

      data += write_avail;
      size -= write_avail;
      written += write_avail;

      if (nonblock || ro->error)
         break;

      if (write_avail == 0)
      {
         pthread_mutex_lock(&ro->cond_lock);
         pthread_cond_wait(&ro->cond, &ro->cond_lock);
         pthread_mutex_unlock(&ro->cond_lock);
      }

   } while (size > 0 && !ro->error);

   if (ro->error)
   {
      fuse_reply_err(req, EPIPE);
      return;
   }

   if (written == 0)
   {
      fuse_reply_err(req, EAGAIN);
      return;
   }

   fuse_reply_write(req, written);
   ro->write_cnt += written;
}

// Almost straight copypasta from OSS Proxy.
// It seems that memory is mapped directly between two different processes.
// Since ioctl() does not contain any size information for its arguments, we first have to tell it how much
// memory we want to map between the two different processes, then ask it to call ioctl() again.
static bool ioctl_prep_uarg(fuse_req_t req,
      void *in, size_t in_size,
      void *out, size_t out_size,
      void *uarg,
      const void *in_buf, size_t in_bufsize, size_t out_bufsize)
{
   bool retry = false;
   struct iovec in_iov = {0};
   struct iovec out_iov = {0};

   if (in)
   {
      if (in_bufsize == 0)
      {
         in_iov.iov_base = uarg;
         in_iov.iov_len = in_size;
         retry = true;
      }
      else
      {
         assert(in_bufsize == in_size);
         memcpy(in, in_buf, in_size);
      }
   }

   if (out)
   {
      if (out_bufsize == 0)
      {
         out_iov.iov_base = uarg;
         out_iov.iov_len = out_size;
         retry = true;
      }
      else
      {
         assert(out_bufsize == out_size);
      }
   }

   if (retry)
      fuse_reply_ioctl_retry(req, &in_iov, 1, &out_iov, 1);

   return retry;
}

#define IOCTL_RETURN(addr) do { \
   fuse_reply_ioctl(req, 0, addr, sizeof(*(addr))); \
} while(0)

#define IOCTL_RETURN_NULL() do { \
   fuse_reply_ioctl(req, 0, NULL, 0); \
} while(0)

#define PREP_UARG(inp, inp_s, outp, outp_s) do { \
   if (ioctl_prep_uarg(req, inp, inp_s, \
            outp, outp_s, uarg, \
            in_buf, in_bufsize, out_bufsize)) \
      return; \
} while(0)

#define PREP_UARG_OUT(outp) PREP_UARG(NULL, 0, outp, sizeof(*(outp)))
#define PREP_UARG_INOUT(inp, outp) PREP_UARG(inp, sizeof(*(inp)), outp, sizeof(*(outp)))

static int oss2rsd_fmt(int ossfmt)
{
   switch (ossfmt)
   {
      case AFMT_MU_LAW:
         return RSD_MULAW;
      case AFMT_A_LAW:
         return RSD_ALAW;
      case AFMT_U8:
         return RSD_U8;
      case AFMT_S16_LE:
         return RSD_S16_LE;
      case AFMT_S16_BE:
         return RSD_S16_BE;
      case AFMT_S8:
         return RSD_S8;
      case AFMT_U16_LE:
         return RSD_U16_LE;
      case AFMT_U16_BE:
         return RSD_U16_BE;

      default:
         return RSD_S16_LE;
   }
}

static int rsd2oss_fmt(int rsdfmt)
{
   switch (rsdfmt)
   {
      case RSD_U8:
         return AFMT_U8;
      case RSD_S8:
         return AFMT_S8;
      case RSD_S16_LE:
         return AFMT_S16_LE;
      case RSD_S16_BE:
         return AFMT_S16_BE;
      case RSD_U16_LE:
         return AFMT_U16_LE;
      case RSD_U16_BE:
         return AFMT_U16_BE;
      case RSD_ALAW:
         return AFMT_A_LAW;
      case RSD_MULAW:
         return AFMT_MU_LAW;

      default:
         return AFMT_S16_LE;
   }
}

static void ross_ioctl(fuse_req_t req, int signed_cmd, void *uarg,
      struct fuse_file_info *info, unsigned flags,
      const void *in_buf, size_t in_bufsize, size_t out_bufsize)
{
   ROSS_DECL;

   unsigned cmd = signed_cmd;
   int i = 0;

   switch (cmd)
   {
#ifdef OSS_GETVERSION
      case OSS_GETVERSION:
         PREP_UARG_OUT(&i);
         i = (3 << 16) | (8 << 8) | (1 << 4) | 0; // 3.8.1
         IOCTL_RETURN(&i);
         break;
#endif

#ifdef SNDCTL_DSP_GETFMTS
      case SNDCTL_DSP_GETFMTS:
         PREP_UARG_OUT(&i);
         i = AFMT_U8 | AFMT_S8 | AFMT_S16_LE | AFMT_S16_BE |
            AFMT_U16_LE | AFMT_U16_BE | AFMT_A_LAW | AFMT_MU_LAW;
         IOCTL_RETURN(&i);
         break;
#endif

#ifdef SNDCTL_DSP_NONBLOCK
      case SNDCTL_DSP_NONBLOCK:
         ro->nonblock = true;
         IOCTL_RETURN_NULL();
         break;
#endif

#ifdef SNDCTL_DSP_RESET
      case SNDCTL_DSP_RESET:
#if defined(SNDCTL_DSP_HALT) && (SNDCTL_DSP_HALT != SNDCTL_DSP_RESET)
      case SNDCTL_DSP_HALT:
#endif
         ross_reset(ro);
         IOCTL_RETURN_NULL();
         break;
#endif

#ifdef SNDCTL_DSP_SPEED
      case SNDCTL_DSP_SPEED:
         PREP_UARG_INOUT(&i, &i);
         rsd_set_param(ro->rd, RSD_SAMPLERATE, &i);
         ro->rate = i;
         IOCTL_RETURN(&i);
         break;
#endif

#ifdef SNDCTL_DSP_SETFMT
      case SNDCTL_DSP_SETFMT:
         PREP_UARG_INOUT(&i, &i);
         i = oss2rsd_fmt(i);
         rsd_set_param(ro->rd, RSD_FORMAT, &i);
         i = rsd2oss_fmt(i);
         IOCTL_RETURN(&i);
         break;
#endif

#ifdef SNDCTL_DSP_CHANNELS
      case SNDCTL_DSP_CHANNELS:
         PREP_UARG_INOUT(&i, &i);
         rsd_set_param(ro->rd, RSD_CHANNELS, &i);
         ro->channels = i;
         IOCTL_RETURN(&i);
         break;
#endif

#ifdef SNDCTL_DSP_STEREO
      case SNDCTL_DSP_STEREO:
      {
         PREP_UARG_INOUT(&i, &i);
         int chans = i ? 2 : 1;
         rsd_set_param(ro->rd, RSD_CHANNELS, &chans);
         ro->channels = chans;
         IOCTL_RETURN(&i);
         break;
      }
#endif

#ifdef SNDCTL_DSP_GETOSPACE
      case SNDCTL_DSP_GETOSPACE:
      {
         unsigned bytes = ross_write_avail(ro);

         audio_buf_info audio_info = {
            .bytes = bytes,
            .fragments = bytes / ro->fragsize,
            .fragsize = ro->fragsize,
            .fragstotal = ro->frags
         };

         PREP_UARG_OUT(&audio_info);
         IOCTL_RETURN(&audio_info);
         break;
      }
#endif

#ifdef SNDCTL_DSP_GETBLKSIZE
      case SNDCTL_DSP_GETBLKSIZE:
         PREP_UARG_OUT(&i);
         i = ro->fragsize;
         IOCTL_RETURN(&i);
         break;
#endif

#ifdef SNDCTL_DSP_SETFRAGMENT
      case SNDCTL_DSP_SETFRAGMENT:
      {
         if (ro->started)
            fuse_reply_err(req, EINVAL);

         PREP_UARG_INOUT(&i, &i);
         int frags = (i >> 16) & 0xffff;
         int fragsize = 1 << (i & 0xffff);

         if (fragsize < 512)
            fragsize = 512;
         if (frags < 8)
            frags = 8;

         ro->frags = frags;
         ro->fragsize = fragsize;
         ro->bufsize = frags * fragsize;

         IOCTL_RETURN(&i);
         break;
      }
#endif

#ifdef SNDCTL_DSP_GETODELAY
      case SNDCTL_DSP_GETODELAY:
         PREP_UARG_OUT(&i);
         i = ross_latency(ro);
         IOCTL_RETURN(&i);
         break;
#endif

#ifdef SNDCTL_DSP_SYNC
      case SNDCTL_DSP_SYNC:
         if (ro->started)
            usleep((1000000LLU * ross_latency(ro)) / ro->bps);
         IOCTL_RETURN_NULL();
         break;
#endif

#ifdef SNDCTL_DSP_COOKEDMODE
      case SNDCTL_DSP_COOKEDMODE:
         PREP_UARG_INOUT(&i, &i);
         IOCTL_RETURN(&i);
         break;
#endif

#ifdef SNDCTL_DSP_GETOPTR
      case SNDCTL_DSP_GETOPTR:
      {
         unsigned played_bytes = ro->write_cnt - ross_read_avail(ro);

         count_info ci = {
            .bytes = played_bytes,
            .blocks = played_bytes / ro->fragsize,
            .ptr = played_bytes % ro->bufsize
         };

         PREP_UARG_OUT(&ci);
         IOCTL_RETURN(&ci);
         break;
      }
#endif

#ifdef SNDCTL_DSP_SETPLAYVOL
      case SNDCTL_DSP_SETPLAYVOL:
         PREP_UARG_INOUT(&i, &i);
         i = (100 << 8) | 100; // We don't have a volume control :D
         IOCTL_RETURN(&i);
         break;
#endif

#ifdef SNDCTL_DSP_GETPLAYVOL
      case SNDCTL_DSP_GETPLAYVOL:
         PREP_UARG_OUT(&i);
         i = (100 << 8) | 100; // We don't have a volume control :D
         IOCTL_RETURN(&i);
         break;
#endif

#ifdef SNDCTL_DSP_SETTRIGGER
      case SNDCTL_DSP_SETTRIGGER:
         PREP_UARG_INOUT(&i, &i);
         IOCTL_RETURN(&i); // No reason to care about this for now. Maybe when/if mmap() gets implemented.
         break;
#endif

      default:
         fuse_reply_err(req, EINVAL);
   }
}


static void ross_poll(fuse_req_t req, struct fuse_file_info *info,
      struct fuse_pollhandle *ph)
{
   ROSS_DECL;

   ross_update_notify(ro, ph);

   if (ro->error)
      fuse_reply_poll(req, POLLHUP);
   else if (!ro->started || (ross_write_avail(ro) > 0))
      fuse_reply_poll(req, POLLOUT);
   else
      fuse_reply_poll(req, 0);
}

static const struct cuse_lowlevel_ops ross_op = {
   .open = ross_open,
   .write = ross_write,
   .ioctl = ross_ioctl,
   .poll = ross_poll,
   .release = ross_release,
};

#define ROSS_OPT(t, p) { t, offsetof(struct ross_param, p), 1 }
struct ross_param
{
   unsigned major;
   unsigned minor;
   char *dev_name;
};

static const struct fuse_opt ross_opts[] = {
   ROSS_OPT("-M %u", major),
   ROSS_OPT("--maj=%u", major),
   ROSS_OPT("-m %u", minor),
   ROSS_OPT("--min=%u", minor),
   ROSS_OPT("-n %s", dev_name),
   ROSS_OPT("--name=%s", dev_name),
   FUSE_OPT_KEY("-h", 0),
   FUSE_OPT_KEY("--help", 0),
   FUSE_OPT_END
};

static void print_help(void)
{
   fprintf(stderr, "CUSE-ROSS Usage:\n");
   fprintf(stderr, "\t-M major, --maj=major\n");
   fprintf(stderr, "\t-m minor, --min=minor\n");
   fprintf(stderr, "\t-n name, --name=name (default: ross)\n");
   fprintf(stderr, "\t\tDevice will be created in /dev/$name.\n");
   fprintf(stderr, "\n");
}

static int process_arg(void *data, const char *arg, int key,
      struct fuse_args *outargs)
{
   switch (key)
   {
      case 0:
         print_help();
         return fuse_opt_add_arg(outargs, "-ho");
      default:
         return 1;
   }
}

int main(int argc, char *argv[])
{
   assert(sizeof(void*) <= sizeof(uint64_t));
   // We use the uint64_t FH to contain a pointer.
   // Make sure that this is possible ...

   struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
   struct ross_param param = {0}; 

   char dev_name[128] = {0};
   const char *dev_info_argv[] = { dev_name };

   if (fuse_opt_parse(&args, &param, ross_opts, process_arg))
   {
      fprintf(stderr, "Failed to parse ...\n");
      return 1;
   }

   snprintf(dev_name, sizeof(dev_name), "DEVNAME=%s", param.dev_name ? param.dev_name : "ross");

   struct cuse_info ci = {
      .dev_major = param.major,
      .dev_minor = param.minor,
      .dev_info_argc = 1,
      .dev_info_argv = dev_info_argv,
      .flags = CUSE_UNRESTRICTED_IOCTL,
   };

   return cuse_lowlevel_main(args.argc, args.argv, &ci, &ross_op, NULL);
}
