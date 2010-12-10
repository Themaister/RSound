/*
    DeaDBeeF - ultimate music player for GNU/Linux systems with X11
    Copyright (C) 2009-2010 Alexey Yakovenko <waker@users.sourceforge.net>

    RSound output plugin for DeaDBeeF
    Copyright (C) 2010 Hans-Kristian Arntzen <maister@archlinux.us>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <rsound.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <deadbeef.h>

//#define trace(...) { fprintf(stderr, __VA_ARGS__); }
#define trace(fmt,...)

#define LOCK {deadbeef->mutex_lock (mutex); /*fprintf (stderr, "alsa lock %s:%d\n", __FILE__, __LINE__);*/}
#define UNLOCK {deadbeef->mutex_unlock (mutex); /*fprintf (stderr, "alsa unlock %s:%d\n", __FILE__, __LINE__);*/}

static DB_output_t plugin;
DB_functions_t *deadbeef;

static rsound_t *rd = NULL;
static volatile int rsound_terminate;
static volatile int state; // one of output_state_t
static uintptr_t mutex;
static intptr_t rsound_tid;

static char conf_rsound_host[256];
static char conf_rsound_port[16];
static int rsound_rate = 44100;

static int
prsound_callback (char *stream, int len);

static void
prsound_thread (void *context);

static int
prsound_init (void);

static int
prsound_free (void);

static int
prsound_change_rate (int rate);

static int
prsound_play (void);

static int
prsound_stop (void);

static int
prsound_pause (void);

static int
prsound_unpause (void);

static int
prsound_get_rate (void);

static int
prsound_get_bps (void);

static int
prsound_get_channels (void);

static int
prsound_get_endianness (void);


int
prsound_init (void) {
    trace("init\n");
    int channels, fmt;
    int delay;
    rsound_tid = 0;
    mutex = 0;

    // get and cache conf variables
    strncpy (conf_rsound_host, deadbeef->conf_get_str ("rsound.host", ""), sizeof(conf_rsound_host));
    conf_rsound_host[sizeof(conf_rsound_host) - 1] = '\0';
    strncpy (conf_rsound_port, deadbeef->conf_get_str ("rsound.port", ""), sizeof(conf_rsound_port));
    conf_rsound_port[sizeof(conf_rsound_port) - 1] = '\0';
    rsound_rate = deadbeef->conf_get_int("rsound.rate", 0);
    if (rsound_rate <= 0) {
        rsound_rate = 44100;
    }
    
    delay = deadbeef->conf_get_int("rsound.delay", 0);

    trace ("rsound host: %s\n", conf_rsound_host);
    trace ("rsound port: %s\n", conf_rsound_port);
    trace ("rsound rate: %d\n", rsound_rate);
    trace ("rsound delay: %d\n", delay);

    state = OUTPUT_STATE_STOPPED;
    if (rsd_init(&rd) < 0) {
        fprintf(stderr, "rsd_init() failed!\n");
        return -1;
    }

    mutex = deadbeef->mutex_create ();

    if (strlen(conf_rsound_host) > 0) {
        rsd_set_param(rd, RSD_HOST, conf_rsound_host);
    }
    if (strlen(conf_rsound_port) > 0) {
        rsd_set_param(rd, RSD_PORT, conf_rsound_port);
    }

    channels = 2;
    fmt = RSD_S16_NE;
    rsd_set_param(rd, RSD_CHANNELS, &channels);
    rsd_set_param(rd, RSD_SAMPLERATE, &rsound_rate);
    rsd_set_param(rd, RSD_FORMAT, &fmt);
    rsd_set_param(rd, RSD_IDENTITY, "DeaDBeeF");
    rsd_set_param(rd, RSD_LATENCY, &delay);

    rsound_terminate = 0;
    rsound_tid = deadbeef->thread_start (prsound_thread, NULL);

    return 0;
}

int
prsound_change_rate (int rate) {
    trace("change_rate\n");
    if (!rd) {
        return rate;
    }

    if (rate == rsound_rate) {
       return rate;
    }

    LOCK;
    state = OUTPUT_STATE_STOPPED;

    rsd_stop(rd);
    rsd_set_param(rd, RSD_SAMPLERATE, &rate);
    if (rsd_start(rd) == 0) {
       state = OUTPUT_STATE_PLAYING;
       rsound_rate = rate;
    }
    UNLOCK;

    return rsound_rate;
}

int
prsound_free (void) {
    trace ("prsound_free\n");
    if (rd && !rsound_terminate) {
        rsound_terminate = 1;
        trace ("waiting for rsound thread to finish\n");
        if (rsound_tid) {
            deadbeef->thread_join (rsound_tid);
            rsound_tid = 0;
        }
        rsd_stop(rd);
        rsd_free(rd);
        rd = NULL;
        if (mutex) {
            deadbeef->mutex_free (mutex);
            mutex = 0;
        }
        state = OUTPUT_STATE_STOPPED;
        rsound_terminate = 0;
    }
    return 0;
}

int
prsound_play (void) {
    trace("play\n");
    int err;
    if (state == OUTPUT_STATE_STOPPED) {
        if (!rd) {
            if (prsound_init () < 0) {
                state = OUTPUT_STATE_STOPPED;
                return -1;
            }
        }
    }
    if (state != OUTPUT_STATE_PLAYING) {
       LOCK;
       if (rsd_start(rd) < 0) {
          state = OUTPUT_STATE_STOPPED;
          UNLOCK;
          return -1;
       }
       state = OUTPUT_STATE_PLAYING;
       UNLOCK;
    }
    return 0;
}


int
prsound_stop (void) {
    trace("stop\n");
    if (!rd) {
        return 0;
    }
    LOCK;
    state = OUTPUT_STATE_STOPPED;
    rsd_stop(rd);

    deadbeef->streamer_reset (1);
    UNLOCK;
    return 0;
}

int
prsound_pause (void) {
    trace("pause\n");
    if (state == OUTPUT_STATE_STOPPED || !rd) {
        return -1;
    }
    // set pause state
    LOCK;
    rsd_pause(rd, 1);
    state = OUTPUT_STATE_PAUSED;
    UNLOCK;
    return 0;
}

int
prsound_unpause (void) {
    trace("unpause\n");
    // unset pause state
    //
    if (state == OUTPUT_STATE_PLAYING || !rd) {
        return -1;
    }

    LOCK;
    if (rsd_pause(rd, 0) < 0) {
       trace("unpause failed!\n");
       state = OUTPUT_STATE_STOPPED;
       UNLOCK;
       return -1;
    }
    state = OUTPUT_STATE_PLAYING;
    UNLOCK;
    return 0;
}

int
prsound_get_rate (void) {
    if (!rd) {
        prsound_init ();
    }
    return rsound_rate;
}

int
prsound_get_bps (void) {
    return 16;
}

int
prsound_get_channels (void) {
    return 2;
}

static int
prsound_get_endianness (void) {
#if WORDS_BIGENDIAN
    return 1;
#else
    return 0;
#endif
}

static void
prsound_thread (void *context) {
    size_t rc;
    size_t has_read;

    usleep(10000);

    for (;;) {
        if (rsound_terminate) {
            break;
        }
        if (state != OUTPUT_STATE_PLAYING || !deadbeef->streamer_ok_to_read (-1)) {
            usleep (10000);
            continue;
        }

        rsd_delay_wait(rd);
        LOCK;
        size_t avail = rsd_get_avail(rd);

        while (has_read < avail) {
           char buffer[avail];
           if (rsound_terminate || state != OUTPUT_STATE_PLAYING || !deadbeef->streamer_ok_to_read(-1)) {
              break;
           }
           rc = prsound_callback (buffer, avail);

           if (rc >= 4) {
              if (rsd_write(rd, buffer, rc) == 0) {
                 trace("write failed!\n");
                 state = OUTPUT_STATE_STOPPED;
                 break;
              }
              avail -= rc;
           }
           else {
              UNLOCK;
              usleep(1000);
              LOCK;
           }
        }
        UNLOCK;
        usleep(1000);
    }
}

static int
prsound_callback (char *i_stream, int len) {
    int16_t *stream = (int16_t*)i_stream; // Unsafe, but other plugins do this, so might as well.
    int bytesread = deadbeef->streamer_read ((char*)stream, len);

    int16_t ivolume = deadbeef->volume_get_amp () * 1000;
    for (int i = 0; i < bytesread/2; i++) {
        stream[i] = (int16_t)(((int32_t)((stream)[i])) * ivolume / 1000);
    }

    return bytesread;
}

static int
prsound_configchanged (DB_event_t *ev, uintptr_t data) {
    (void)ev;
    (void)data;
    deadbeef->sendmessage(M_REINIT_SOUND, 0, 0, 0);
    return 0;
}

static int
prsound_get_state (void) {
    return state;
}

static int
rsound_start (void) {
    deadbeef->ev_subscribe (DB_PLUGIN (&plugin), DB_EV_CONFIGCHANGED, DB_CALLBACK (prsound_configchanged), 0);
    return 0;
}

static int
rsound_stop (void) {
    deadbeef->ev_unsubscribe (DB_PLUGIN (&plugin), DB_EV_CONFIGCHANGED, DB_CALLBACK (prsound_configchanged), 0);
    return 0;
}

DB_plugin_t *
rsound_load (DB_functions_t *api) {
    deadbeef = api;
    return DB_PLUGIN (&plugin);
}

static const char settings_dlg[] =
    "property \"RSound server\" entry rsound.host \"\";\n"
    "property \"RSound port\" entry rsound.port \"\";\n"
    "property \"Audio rate\" entry rsound.rate \"\";\n"
    "property \"Latency (ms)\" entry rsound.delay \"\";\n"
;

// define plugin interface
static DB_output_t plugin = {
    DB_PLUGIN_SET_API_VERSION
    .plugin.version_major = 0,
    .plugin.version_minor = 1,
    .plugin.nostop = 1,
    .plugin.type = DB_PLUGIN_OUTPUT,
    .plugin.name = "RSound output plugin",
    .plugin.descr = "plays audio through network with RSound.",
    .plugin.author = "Hans-Kristian Arntzen",
    .plugin.email = "maister@archlinux.us",
    .plugin.website = "http://www.rsound.org",
    .plugin.start = rsound_start,
    .plugin.stop = rsound_stop,
    .plugin.configdialog = settings_dlg,
    .init = prsound_init,
    .free = prsound_free,
    .change_rate = prsound_change_rate,
    .play = prsound_play,
    .stop = prsound_stop,
    .pause = prsound_pause,
    .unpause = prsound_unpause,
    .state = prsound_get_state,
    .samplerate = prsound_get_rate,
    .bitspersample = prsound_get_bps,
    .channels = prsound_get_channels,
    .endianness = prsound_get_endianness,
    .enum_soundcards = NULL,
};
