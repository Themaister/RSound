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

#include "jack.h"
#include "../rsound.h"

static void jack_close(void *data)
{
   jack_t *jd = data;

   for (int i = 0; i < jd->channels; i++)
      jack_ringbuffer_free(jd->buffer[i]);

   jack_deactivate(sound->client);
   jack_client_close(sound->client);
   free(sound);
}

/* Opens and sets params */
static int jack_init(void **data)
{
   jack_t *sound = calloc(1, sizeof(jack_t));
   if ( sound == NULL )
      return -1;
   *data = sound;
   return 0;
}

static int process_cb() {}

static int jack_open(void *data, wav_header_t *w)
{
   jack_t *jd = data;
   jd->channels = w->numChannels;

   jd->client = jack_client_open(JACK_CLIENT_NAME, JackNullOption, NULL);
   if (jd->client == NULL)
      return -1;

   jack_set_process_callback(jd->client, process_cb, jd);
   jack_on_shutdown(jd->client, shutdown_cb, jd);

   for (int i = 0; i < jd->channels; i++)
   {
      jd->ports[i] = jack_port_register(jd->client, jd->source_ports[i], JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
      if (jd->ports[i] == NULL)
      {
         fprintf(stderr, "Couldn't create jack ports\n");
         goto error;
      }
   }


   for (int i = 0; i < jd->channels; i++)
      jd->buffer[i] = jack_ringbuffer_create(JACK_BUFFER_SIZE);

   if (jack_activate(jd->client) < 0)
   {
      fprintf(stderr, "Couldn't connect to JACK server\n");
      goto error;
   }

   const char **jports;
   const char *dest_ports[MAX_PORTS];
   jports = jack_get_ports(jd->client, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
   if (jports == NULL)
   {
      fprintf(stderr, "Can't get ports ...\n");
      goto error;
   }

   for (int i = 0; i < MAX_PORTS && jports[i] != NULL; i++)
   {
      dest_ports[i] = jports[i];
   }

   for (int i = 0; i < jd->channels; i++)
   {
      int rd = jack_connect(jd->client, jack_port_name(jd->ports[i]), dest_ports[i]);
      if (rd != 0)
      {
         fprintf(stderr, "Can't connect ports...\n");
         goto error;
      }
   }

   return 0;
}

static void jack_get_backend (void *data, backend_info_t *backend_info)
{
   jack_t *jd = data;
}

static int jack_latency(void* data)
{
   jack_t *jd = data;
   int delay;



   return delay;
}

static size_t jack_write (void *data, const void* buf, size_t size)
{
   jack_t *jd = data;
   return write_buffer(jd, buf, size);
}

const rsd_backend_callback_t rsd_jack = {
   .init = jack_init,
   .open = jack_open,
   .write = jack_write,
   .latency = jack_latency,
   .get_backend_info = jack_get_backend,
   .close = jack_close,
   .backend = "JACK"
};


