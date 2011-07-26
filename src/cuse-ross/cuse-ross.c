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

#include <sys/soundcard.h>

#define ROSS_DECL ross_t *ro = (ross_t*)info->fh

// Dummy values for programs that are _very_ legacy.
#define FRAGSIZE 2048
#define FRAGS 16

typedef struct ross
{
   rsound_t *rd;
   bool started;
   bool nonblock;

   pthread_mutex_t event_lock;
   struct fuse_pollhandle *ph;

   int channels;
   int rate;
   int fragsize;
   int frags;
   int latency;

   bool use_latency;
   // Unless SETFRAGMENT is used, we really don't care about high latency at all.
   // We don't really use this yet.
} ross_t;

static void ross_release(fuse_req_t req, struct fuse_file_info *info)
{
   ROSS_DECL;
   
   // OSS requires close() call to wait until all buffers are flushed out.
   if (ro->started)
   {
      int usec = rsd_delay_ms(ro->rd) * 1000;
      close(rsd_exec(ro->rd));
      usleep(usec);
   }
   else
      rsd_free(ro->rd);

   pthread_mutex_destroy(&ro->event_lock);

   if (ro->ph)
      fuse_pollhandle_destroy(ro->ph);

   free(ro);
   fuse_reply_err(req, 0);
}

static void ross_event_cb(void *data)
{
   ross_t *ro = data;
   pthread_mutex_lock(&ro->event_lock);
   struct fuse_pollhandle *ph = ro->ph;
   if (ph)
      fuse_lowlevel_notify_poll(ph);
   pthread_mutex_unlock(&ro->event_lock);
}

static void ross_open(fuse_req_t req, struct fuse_file_info *info)
{
   ross_t *ro = calloc(1, sizeof(*ro));
   if (!ro)
   {
      fuse_reply_err(req, ENOMEM);
      return;
   }

   if (pthread_mutex_init(&ro->event_lock, NULL) < 0)
   {
      free(ro);
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

   // Use some different defaults than regular OSS for convenience. :D
   int channels = 2;
   int rate = 44100;
   int format = RSD_S16_LE;
   ro->rate = 44100;
   ro->channels = 2;

   rsd_set_param(rd, RSD_CHANNELS, &channels);
   rsd_set_param(rd, RSD_SAMPLERATE, &rate);
   rsd_set_param(rd, RSD_FORMAT, &format);
   rsd_set_event_callback(rd, ross_event_cb, ro);

   ro->frags = FRAGS;
   ro->fragsize = FRAGSIZE;

   ro->rd = rd;
   info->fh = (uint64_t)ro;
   info->nonseekable = 1;
   info->direct_io = 1;

   fuse_reply_open(req, info);
}

static void ross_write(fuse_req_t req, const char *data, size_t size, off_t off, struct fuse_file_info *info)
{
   (void)off;

   ROSS_DECL;

   if (size == 0)
   {
      fuse_reply_write(req, 0);
      return;
   }

   if (!ro->started)
   {
      ro->latency = (1000 * ro->frags * ro->fragsize) / (ro->channels * ro->rate * rsd_samplesize(ro->rd));
      if (ro->latency < 64)
         ro->latency = 64;

      rsd_set_param(ro->rd, RSD_LATENCY, &ro->latency);

      if (rsd_start(ro->rd) < 0)
      {
         fuse_reply_err(req, EIO);
         return;
      }
      ro->started = true;

   }

   size_t avail = size;
   bool nonblock = ro->nonblock || (info->flags & O_NONBLOCK);
   if (nonblock)
   {
      avail = rsd_get_avail(ro->rd);
      if (avail > size)
         avail = size;

      if (avail == 0)
      {
         fuse_reply_err(req, EAGAIN);
         return;
      }
   }

   int ret;
   if ((ret = rsd_write(ro->rd, data, avail)) == 0)
   {
      fuse_reply_err(req, EIO);
      return;
   }

   fuse_reply_write(req, ret);
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
         rsd_stop(ro->rd);
         ro->started = false;
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
         unsigned bufsize = ro->frags * ro->fragsize;
         unsigned bytes = ro->frags * ro->fragsize;
         if (ro->started)
            bytes = rsd_get_avail(ro->rd);

         if (bytes > bufsize)
            bytes = bufsize;

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
         ro->use_latency = true;

         i = (frags << 16) | fragsize;
         IOCTL_RETURN(&i);
         break;
      }
#endif

#ifdef SNDCTL_DSP_GETODELAY
      case SNDCTL_DSP_GETODELAY:
         PREP_UARG_OUT(&i);
         i = ro->started ? rsd_delay(ro->rd) : 0;
         IOCTL_RETURN(&i);
         break;
#endif

#ifdef SNDCTL_DSP_SYNC
      case SNDCTL_DSP_SYNC:
         if (ro->started)
            usleep(rsd_delay_ms(ro->rd) * 1000);
         IOCTL_RETURN_NULL();
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

   pthread_mutex_lock(&ro->event_lock);
   struct fuse_pollhandle *tmp_ph = ro->ph;
   ro->ph = ph;
   pthread_mutex_unlock(&ro->event_lock);
   if (tmp_ph)
      fuse_pollhandle_destroy(tmp_ph);

   if (!ro->started || (rsd_get_avail(ro->rd) > 0))
      fuse_reply_poll(req, POLLOUT);
   //else
      //fuse_reply_poll(req, 0);
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
   (void)data;
   (void)arg;

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
