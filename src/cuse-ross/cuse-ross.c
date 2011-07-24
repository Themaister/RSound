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

#define ROSS_DECL ross_t *ro = (ross_t*)info->fh

typedef struct ross
{
   rsound_t *rd;
   bool started;
} ross_t;

static void ross_release(fuse_req_t req, struct fuse_file_info *info)
{
   ROSS_DECL;
   rsd_stop(ro->rd);
   rsd_free(ro->rd);
   free(ro);
   fuse_reply_err(req, 0);
}

static void ross_open(fuse_req_t req, struct fuse_file_info *info)
{
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

   int channels = 2;
   int rate = 44100;
   rsd_set_param(rd, RSD_CHANNELS, &channels);
   rsd_set_param(rd, RSD_SAMPLERATE, &rate);

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
      if (rsd_start(ro->rd) < 0)
      {
         fuse_reply_err(req, EIO);
         return;
      }
      ro->started = true;
   }

   int ret;
   if ((ret = rsd_write(ro->rd, data, size)) == 0)
   {
      fuse_reply_err(req, EIO);
      return;
   }

   fuse_reply_write(req, ret);
}

static void ross_read(fuse_req_t req, size_t size, off_t off,
      struct fuse_file_info *info)
{
   fuse_reply_err(req, ENOSYS);
}

static void ross_ioctl(fuse_req_t req, int cmd, void *arg,
      struct fuse_file_info *info, unsigned flags,
      const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
   //fuse_reply_ioctl(req, 0, NULL, 0);
   fuse_reply_err(req, EINVAL);
}

static const struct cuse_lowlevel_ops ross_op = {
   .open = ross_open,
   .read = ross_read,
   .write = ross_write,
   .ioctl = ross_ioctl,
   .release = ross_release,
};

#define ROSS_OPT(t, p) { t, offsetof(struct ross_param, p), 1 }
struct ross_param
{
   unsigned major;
   unsigned minor;
   char *dev_name;
   bool is_help;
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

static int process_arg(void *data, const char *arg, int key,
      struct fuse_args *outargs)
{
   struct ross_param *param = data;
   (void)arg;

   switch (key)
   {
      case 0:
         param->is_help = true;
         return fuse_opt_add_arg(outargs, "-ho");
      default:
         return 1;
   }
}

int main(int argc, char *argv[])
{
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
