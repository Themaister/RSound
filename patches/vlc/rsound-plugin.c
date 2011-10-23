/*****************************************************************************
 * rsound.c : RSound module for vlc
 *****************************************************************************
 * Authors: Hans-Kristian Arntzen <maister@archlinux.us>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

//#include <stdio.h>
#include <rsound.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fs.h>

#include <vlc_aout.h>

#define N_(str) (str)

/*****************************************************************************
 * aout_sys_t: RSound audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 *****************************************************************************/
struct aout_sys_t
{
    rsound_t *rd;
    vlc_thread_t thread;
    volatile int cancel;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

static void Play         ( audio_output_t *, block_t *);
static void* RSDThread   ( void * );

static mtime_t BufferDuration( audio_output_t * p_aout );

#define CONNECT_STRING_OPTION_HOST "rsd-connect-host"
#define CONNECT_STRING_HOST N_("RSound server host:")
#define CONNECT_STRING_LONGTEXT_HOST N_("Defines which host to connect to. A blank field equals the default (RSD_SERVER environmental variable or 'localhost').")

#define CONNECT_STRING_OPTION_PORT "rsd-connect-port"
#define CONNECT_STRING_PORT N_("RSound server port:")
#define CONNECT_STRING_LONGTEXT_PORT N_("Defines which port to connect to. A blank field equals the default (RSD_PORT environmental variable or '12345').")

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_shortname( "RSound" )
    set_description( N_("RSound audio output") )

    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )

    add_string(CONNECT_STRING_OPTION_HOST, NULL, CONNECT_STRING_HOST,
          CONNECT_STRING_LONGTEXT_HOST, true)
    add_string(CONNECT_STRING_OPTION_PORT, NULL, CONNECT_STRING_PORT,
          CONNECT_STRING_LONGTEXT_PORT, true)

    set_capability( "audio output", 160 )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Open: open the connection
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    audio_output_t * p_aout = (audio_output_t *)p_this;
    struct aout_sys_t * p_sys;

    /* Allocate structure */
    p_aout->sys = p_sys = calloc( 1, sizeof( aout_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    /* Inits rsound_t */
    if ( rsd_init(&p_sys->rd) < 0 )
        return VLC_ENOMEM;

    int format = RSD_S16_NE;
    int channels = aout_FormatNbChannels( &p_aout->format );
    int rate = p_aout->format.i_rate;

    char *host = var_InheritString ( p_aout, CONNECT_STRING_OPTION_HOST );
    if ( host != NULL && strlen(host) > 0 )
        rsd_set_param(p_sys->rd, RSD_HOST, host);
    if ( host != NULL )
       free(host);

    char *port = var_InheritString ( p_aout, CONNECT_STRING_OPTION_PORT );
    if ( port != NULL && strlen(port) > 0 )
        rsd_set_param(p_sys->rd, RSD_PORT, port);
    if ( port != NULL )
       free(port);

#ifdef RSD_IDENTITY
    rsd_set_param(p_sys->rd, RSD_IDENTITY, "VLC");
#endif

    /* Sets up channel mapping */
    switch(channels)
    {
        case 8:
            p_aout->format.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT
                | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                | AOUT_CHAN_LFE;
            break;
        case 6:
            p_aout->format.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                | AOUT_CHAN_LFE;
            break;

        case 4:
            p_aout->format.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;

        case 2:
            p_aout->format.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;

        case 1:
            p_aout->format.i_physical_channels = AOUT_CHAN_CENTER;
            break;

        default:
            msg_Err(p_aout,"Invalid number of channels");
            rsd_free(p_sys->rd);
            free(p_sys);
            return VLC_EGENERIC;
    }

    rsd_set_param(p_sys->rd, RSD_SAMPLERATE, &rate);
    rsd_set_param(p_sys->rd, RSD_CHANNELS, &channels);
    rsd_set_param(p_sys->rd, RSD_FORMAT, &format);

    p_aout->format.i_format = VLC_CODEC_S16N;
    p_aout->pf_play = Play;

    aout_VolumeSoftInit( p_aout );

    if ( rsd_start(p_sys->rd) < 0 )
    {
       msg_Err ( p_aout, "Cannot connect to server.");
       rsd_free( p_sys->rd );
       free( p_sys );
       return VLC_EGENERIC;
    }

    /* Create RSound thread and wait for its readiness. */
    if( vlc_clone( &p_sys->thread, RSDThread, p_aout,
                           VLC_THREAD_PRIORITY_OUTPUT ) )
    {
        msg_Err( p_aout, "cannot create RSound thread (%m)" );
        rsd_free( p_sys->rd );
        free( p_sys );
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Play: nothing to do
 *****************************************************************************/
static void Play( audio_output_t *p_aout, block_t *p_block )
{
    VLC_UNUSED(p_aout);
    VLC_UNUSED(p_block);
}

/*****************************************************************************
 * Close: close the connection
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    audio_output_t *p_aout = (audio_output_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->sys;

    p_sys->cancel = 1;
    vlc_join( p_sys->thread, NULL );
    p_aout->b_die = false;

    rsd_stop(p_sys->rd);
    rsd_free(p_sys->rd);

    free( p_sys );
}

/*****************************************************************************
 * BufferDuration: buffer status query
 *****************************************************************************
 * This function returns the duration in microseconds of the current buffer.
 *****************************************************************************/
static mtime_t BufferDuration( audio_output_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->sys;

    return (mtime_t)(rsd_delay_ms(p_sys->rd) * 1000); 
}

/*****************************************************************************
 * Thread loop
 ****************************************************************************/

#define MIN_LATENCY_US 50000

static void* RSDThread( void *p_this )
{
    audio_output_t * p_aout = p_this;
    struct aout_sys_t * p_sys = p_aout->sys;

    // Fill up the client side buffer before we start
    uint8_t tmpbuf[rsd_get_avail(p_sys->rd)];
    memset(tmpbuf, 0, sizeof(tmpbuf));
    rsd_write(p_sys->rd, tmpbuf, sizeof(tmpbuf));

    while ( !p_sys->cancel )
    {
        aout_buffer_t * p_buffer = NULL;
        uint8_t * p_bytes;
        int i_size;

        mtime_t buffered = BufferDuration( p_aout );

        p_buffer = aout_PacketNext( p_aout, mdate() + buffered );

        if ( p_buffer == NULL )
        {
           /* We avoid buffer underruns since they make latency handling more inaccurate. We simply fill up the client side buffer. */
           if ( BufferDuration(p_aout) < MIN_LATENCY_US && rsd_get_avail(p_sys->rd) > 0 )
           {
              msg_Dbg(p_aout, "Possible underrun detected, filling up buffer.");
              uint8_t tmpbuf[rsd_get_avail(p_sys->rd)];
              memset(tmpbuf, 0, sizeof(tmpbuf));
              rsd_write(p_sys->rd, tmpbuf, sizeof(tmpbuf));
           }
           /* We will need to sleep for some time anyways. */
           else
           {
              msleep(BufferDuration(p_aout)/8);
           }
           continue;
        }

        p_bytes = p_buffer->p_buffer;
        i_size = p_buffer->i_buffer;

        if ( rsd_write(p_sys->rd, p_bytes, i_size) == 0 )
        {
            msg_Err(p_aout, "rsd_write() failed. Connection was closed?");
        }

        aout_BufferFree(p_buffer);
    }

    return NULL;
}
