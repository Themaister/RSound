#include <rsound.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

static void generate_sine(int16_t *out, int samples, int rate, int freq)
{
   int i;
   for ( i = 0; i < samples; i+=2 )
   {
      out[i] = (int16_t)((double)0x7FFE * sin((M_PI * i * freq) / (double)rate));
      out[i+1] = out[i];
   }
}

int main(void)
{
   rsound_t *rd;
   int rc;
   int16_t sinbuf[44100 * 8];
   generate_sine(sinbuf, sizeof(sinbuf)/sizeof(sinbuf[0]), 44100, 440);

   puts("Testing rsd_simple_start()");

   puts("Calling with params:\n"
        "\tHost:     NULL\n"
        "\tPort:     NULL\n"
        "\tIdent:    NULL\n"
        "\tRate:     44100\n"
        "\tChannels: 2\n"
        "\tFormat:   RSD_S16_NE");

   rc = rsd_simple_start(&rd, NULL, NULL, NULL, 44100, 2, RSD_S16_NE);

   if ( rc < 0 )
   {
      puts("rsd_simple_start returned with error, aborting.");
      return 1;
   }

   puts("Writing 4 seconds of 440Hz sine wave with rsd_write()");

   rc = rsd_write(rd, sinbuf, sizeof(sinbuf));

   if ( rc != (int)sizeof(sinbuf) )
   {
      printf("rsd_write() returned %d, expected %d.\n", rc, (int)sizeof(sinbuf));
      puts("rsd_write failed. Aborting test.");
      rsd_stop(rd);
      rsd_free(rd);
      return 1;
   }

   rsd_stop(rd);
   rsd_free(rd);

   puts("rsd_simple_start() with default params seems to work.");
   puts("=====================================================\n");

   puts("Testing rsd_simple_start()");
   puts("Calling with params:\n"
        "\tHost:     \"localhost\"\n"
        "\tPort:     \"12345\"\n"
        "\tIdent:    \"rsd_simple_start test app\"\n"
        "\tRate:     44100\n"
        "\tChannels: 2\n"
        "\tFormat:   RSD_S16_NE");

   rc = rsd_simple_start(&rd, "localhost", "12345", "rsd_simple_start test app", 44100, 2, RSD_S16_NE);

   if ( rc < 0 )
   {
      puts("rsd_simple_start returned with error, aborting.");
      return 1;
   }

   puts("Writing 4 seconds of 440Hz sine wave with rsd_write()");

   rc = rsd_write(rd, sinbuf, sizeof(sinbuf));

   if ( rc != (int)sizeof(sinbuf) )
   {
      printf("rsd_write() returned %d, expected %d.\n", rc, (int)sizeof(sinbuf));
      puts("rsd_write failed. Aborting test.");
      rsd_stop(rd);
      rsd_free(rd);
      return 1;
   }

   rsd_stop(rd);
   rsd_free(rd);

   puts("rsd_simple_start() seems to work.");


   return 0;
}


