/*  RSound - A PCM audio client/server
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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
#include <string.h>
#include <stdlib.h>

static void jack_close(void *data)
{
   jack_t *jd = data;

   if (jd->client != NULL)
   {
      jack_deactivate(jd->client);
      jack_client_close(jd->client);
   }

   for (int i = 0; i < jd->channels && i < MAX_CHANS; i++)
      if (jd->buffer[i] != NULL)
         jack_ringbuffer_free(jd->buffer[i]);

   free(jd);
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

static int process_cb(jack_nframes_t nframes, void *data) 
{
   jack_t *jd = data;
   if (nframes <= 0)
      return 0;

   jack_default_audio_sample_t *out;

   jack_nframes_t min_avail = jack_ringbuffer_read_space(jd->buffer[0]);
   for (int i = 1; i < jd->channels; i++)
   {
      jack_nframes_t avail = jack_ringbuffer_read_space(jd->buffer[i]);
      if (avail < min_avail)
         min_avail = avail;
   }
   min_avail /= sizeof(jack_default_audio_sample_t);

   if (min_avail > nframes)
      min_avail = nframes;

   for (int i = 0; i < jd->channels; i++)
   {
      out = jack_port_get_buffer(jd->ports[i], nframes);
      jack_ringbuffer_read(jd->buffer[i], (char*)out, min_avail * sizeof(jack_default_audio_sample_t));

      for (jack_nframes_t f = min_avail; f < nframes; f++)
         out[f] = 0.0f;
   }
   return 0;
}

static void shutdown_cb(void *data)
{
   jack_t *jd = data;
   jd->shutdown = 1;
}

static inline int audio_conv_op(enum rsd_format format)
{
   int op = RSD_NULL;
   if (rsnd_format_to_bytes(format) == 4)
      op = converter_fmt_to_s32ne(format) | RSD_S32_TO_FLOAT;
   else
      op = converter_fmt_to_s16ne(format) | RSD_S16_TO_FLOAT;

   return op;
}

static int parse_ports(char **dest_ports, int max_ports, const char *port_list)
{
   if (port_list == NULL)
      return 0;
   if (strlen(port_list) == 0)
      return 0;
   if (!strcmp(port_list, "default"))
      return 0;

   char *tmp_port_list = strdup(port_list);

   const char *tmp = strtok(tmp_port_list, ",");
   int num_used = 0;
   for (int i = 0; i < max_ports && tmp != NULL; i++)
   {
      dest_ports[i] = strdup(tmp);
      tmp = strtok(NULL, ",");
      num_used++;
   }

   free(tmp_port_list);
   return num_used;
}

static int jack_open(void *data, wav_header_t *w)
{
   const char **jports = NULL;
   char *dest_ports[MAX_PORTS];
   int num_parsed_ports = 0;
   jack_t *jd = data;
   jd->channels = w->numChannels;
   jd->format = w->rsd_format;
   jd->conv_op = audio_conv_op(jd->format);
   jd->rate = w->sampleRate;

   if (jd->channels > MAX_PORTS)
   {
      log_printf("Too many audio channels ...\n");
      return -1;
   }

   jd->client = jack_client_open(JACK_CLIENT_NAME, JackNullOption, NULL);
   if (jd->client == NULL)
      return -1;

   jack_set_process_callback(jd->client, process_cb, jd);
   jack_on_shutdown(jd->client, shutdown_cb, jd);


   for (int i = 0; i < jd->channels; i++)
   {
      char buf[64];
      if (i == 0)
         strcpy(buf, "left");
      else if (i == 1)
         strcpy(buf, "right");
      else
         sprintf(buf, "%s%d", "channel", i);

      jd->ports[i] = jack_port_register(jd->client, buf, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
      if (jd->ports[i] == NULL)
      {
         log_printf("Couldn't create jack ports\n");
         goto error;
      }
   }

   jack_nframes_t bufsize;
   jack_nframes_t jack_bufsize = jack_get_buffer_size(jd->client) * sizeof(jack_default_audio_sample_t);
   // We want some headroom, so just use double buffer size.
   if (JACK_BUFFER_SIZE > jack_bufsize * 2)
      bufsize = JACK_BUFFER_SIZE;
   else
      bufsize = jack_bufsize * 2;

   for (int i = 0; i < jd->channels; i++)
   {
      jd->buffer[i] = jack_ringbuffer_create(bufsize);
      if (jd->buffer[i] == NULL)
      {
         log_printf("Couldn't create ringbuffer\n");
         goto error;
      }
   }

   if (jack_activate(jd->client) < 0)
   {
      log_printf("Couldn't connect to JACK server\n");
      goto error;
   }

   num_parsed_ports = parse_ports(dest_ports, MAX_PORTS, device);

   jports = jack_get_ports(jd->client, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
   if (jports == NULL)
   {
      log_printf("Can't get ports ...\n");
      goto error;
   }

   for (int i = num_parsed_ports; i < MAX_PORTS && jports[i] != NULL; i++)
   {
      dest_ports[i] = (char*)jports[i - num_parsed_ports];
   }

   for (int i = 0; i < jd->channels; i++)
   {
      int rd = jack_connect(jd->client, jack_port_name(jd->ports[i]), dest_ports[i]);
      if (rd != 0)
      {
         log_printf("Can't connect ports...\n");
         goto error;
      }
   }


   jack_free(jports);
   for (int i = 0; i < num_parsed_ports; i++)
      free(dest_ports[i]);
   return 0;

error:
   for (int i = 0; i < num_parsed_ports; i++)
      free(dest_ports[i]);

   if (jports != NULL)
      jack_free(jports);
   return -1;
}

static jack_nframes_t jack_internal_latency(jack_t *jd)
{
   jack_latency_range_t range;
   jack_nframes_t latency = 0;
   for (int i = 0; i < jd->channels; i++)
   {
      jack_port_get_latency_range(jd->ports[i], JackPlaybackLatency, &range);
      if (range.max > latency)
         latency = range.max;
   }
   return latency;
}

static void jack_get_backend(void *data, backend_info_t *backend_info)
{
   jack_t *jd = data;
   backend_info->chunk_size = DEFAULT_CHUNK_SIZE;
   if (jack_get_sample_rate(jd->client) != jd->rate)
   {
      backend_info->resample = 1;
      backend_info->ratio = (float)jack_get_sample_rate(jd->client) / jd->rate;
      // If we're resampling, we're resampling to S16_NE or S32_NE, so update that here.
      if (rsnd_format_to_bytes(jd->format) == 4)
         jd->format = is_little_endian() ? RSD_S32_LE : RSD_S32_BE;
      else
         jd->format = is_little_endian() ? RSD_S16_LE : RSD_S16_BE;
      jd->conv_op = audio_conv_op(jd->format);
   }
   else
      backend_info->resample = 0;

   backend_info->latency = jack_internal_latency(jd) * jd->channels * rsnd_format_to_bytes(jd->format);
}

static int jack_latency(void* data)
{
   jack_t *jd = data;

   jack_nframes_t frames = jack_internal_latency(jd) + jack_ringbuffer_read_space(jd->buffer[0]) / sizeof(jack_default_audio_sample_t);
   return frames * jd->channels * rsnd_format_to_bytes(jd->format);
}

static size_t write_buffer(jack_t *jd, const void* buf, size_t size)
{
   // Convert our data to float, deinterleave and write.
   float out_buffer[BYTES_TO_SAMPLES(size, jd->format)];
   float out_deinterleaved_buffer[jd->channels][BYTES_TO_SAMPLES(size, jd->format)/jd->channels];
   memcpy(out_buffer, buf, size);
   audio_converter(out_buffer, jd->format, jd->conv_op, size);

   for (int i = 0; i < jd->channels; i++)
      for (size_t j = 0; j < BYTES_TO_SAMPLES(size, jd->format)/jd->channels; j++)
         out_deinterleaved_buffer[i][j] = out_buffer[j * jd->channels + i];

   // Stupid busy wait for available write space.
   for(;;)
   {
      if (jd->shutdown)
         return 0;

      size_t min_avail = jack_ringbuffer_write_space(jd->buffer[0]);
      for (int i = 1; i < jd->channels; i++)
      {
         size_t avail = jack_ringbuffer_write_space(jd->buffer[i]);
         if (avail < min_avail)
            min_avail = avail;
      }

      if (min_avail >= sizeof(jack_default_audio_sample_t) * BYTES_TO_SAMPLES(size, jd->format) / jd->channels)
         break;

      // TODO: Need to do something more intelligent here!
      usleep(1000);
   }

   for (int i = 0; i < jd->channels; i++)
      jack_ringbuffer_write(jd->buffer[i], (const char*)out_deinterleaved_buffer[i], BYTES_TO_SAMPLES(size, jd->format) * sizeof(jack_default_audio_sample_t) / jd->channels);
   return size;
}

static size_t jack_write (void *data, const void* buf, size_t size)
{
   jack_t *jd = data;
   if (jd->shutdown)
      return 0;

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


