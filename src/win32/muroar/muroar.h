//muroar.h:

/*
 *      Copyright (C) Philipp 'ph3-der-loewe' Schafft - 2009,2010
 *
 *  This file is part of µRoar,
 *  a minimalist library to access a RoarAudio Sound Server.
 *
 *  µRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  µRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with µRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MUROAR_H_
#define _MUROAR_H_

#ifdef __cplusplus
extern "C" {
#endif

// types:
#include <unistd.h>

// BYTE_ORDER and friends:
#ifdef HAVE_HEADER_ENDIAN_H
#include <endian.h>
#endif

// Some defaults:
#define MUROAR_PORT              16002
#define MUROAR_HOST              "localhost"
#define MUROAR_GSOCK             "/tmp/roar"
#define MUROAR_OBJECT            "roar"

// AID = Audio Info Defaults
#define MUROAR_AID_RATE          44100
#define MUROAR_AID_BITS             16
#define MUROAR_AID_CHANNELS          MUROAR_CM_STEREO

#define MUROAR_CM_MONO               1
#define MUROAR_CM_STEREO             2

#define MUROAR_CMD_NOOP              0
#define MUROAR_CMD_IDENTIFY          1
#define MUROAR_CMD_AUTH              2
#define MUROAR_CMD_NEW_STREAM        3
#define MUROAR_CMD_SET_META          4
#define MUROAR_CMD_EXEC_STREAM       5
#define MUROAR_CMD_QUIT              6
#define MUROAR_CMD_GET_STANDBY       7
#define MUROAR_CMD_SET_STANDBY       8
#define MUROAR_CMD_SERVER_INFO       9
#define MUROAR_CMD_SERVER_STATS     10
#define MUROAR_CMD_SERVER_OINFO     11
#define MUROAR_CMD_ADD_DATA         12
#define MUROAR_CMD_EXIT             13
#define MUROAR_CMD_LIST_STREAMS     14
#define MUROAR_CMD_LIST_CLIENTS     15
#define MUROAR_CMD_GET_CLIENT       16
#define MUROAR_CMD_GET_STREAM       17
#define MUROAR_CMD_KICK             18
#define MUROAR_CMD_SET_VOL          19
#define MUROAR_CMD_GET_VOL          20
#define MUROAR_CMD_CON_STREAM       21
#define MUROAR_CMD_GET_META         22
#define MUROAR_CMD_LIST_META        23
#define MUROAR_CMD_BEEP             24
#define MUROAR_CMD_GET_ACL          25
#define MUROAR_CMD_SET_ACL          26
#define MUROAR_CMD_GET_STREAM_PARA  27
#define MUROAR_CMD_SET_STREAM_PARA  28
#define MUROAR_CMD_ATTACH           29
#define MUROAR_CMD_PASSFH           30
#define MUROAR_CMD_GETTIMEOFDAY     31
#define MUROAR_CMD_WHOAMI           32

// special CMDs:
#define MUROAR_CMD_OK              254 /* return value OK    */
#define MUROAR_CMD_ERROR           255 /* return value ERROR */

#define MUROAR_PLAY_WAVE             1
#define MUROAR_PLAY_MIDI            12
#define MUROAR_PLAY_LIGHT           14
#define MUROAR_RECORD_WAVE           2
#define MUROAR_MONITOR_WAVE          3
#define MUROAR_MONITOR_MIDI         13
#define MUROAR_MONITOR_LIGHT        15
#define MUROAR_FILTER_WAVE           4

#define MUROAR_CODEC_PCM_S_LE     0x01
#define MUROAR_CODEC_PCM_S_BE     0x02
#define MUROAR_CODEC_PCM_S_PDP    0x03
#define MUROAR_CODEC_PCM_U_LE     0x05
#define MUROAR_CODEC_PCM_U_BE     0x06
#define MUROAR_CODEC_PCM_U_PDP    0x07
#define MUROAR_CODEC_OGG_VORBIS   0x10
#define MUROAR_CODEC_FLAC         0x11
#define MUROAR_CODEC_OGG_SPEEX    0x12
#define MUROAR_CODEC_OGG_FLAC     0x14
#define MUROAR_CODEC_OGG_CELT     0x16
#define MUROAR_CODEC_ROAR_CELT    0x1a
#define MUROAR_CODEC_ROAR_SPEEX   0x1b
#define MUROAR_CODEC_RIFF_WAVE    0x20
#define MUROAR_CODEC_RIFX         0x22
#define MUROAR_CODEC_AU           0x24
#define MUROAR_CODEC_AIFF         0x28
#define MUROAR_CODEC_ALAW         0x30
#define MUROAR_CODEC_MULAW        0x34
#define MUROAR_CODEC_GSM          0x38
#define MUROAR_CODEC_GSM49        0x39
#define MUROAR_CODEC_MIDI         0x60
#define MUROAR_CODEC_DMX512       0x70
#define MUROAR_CODEC_ROARDMX      0x71

#ifdef __WIN32
#define MUROAR_CODEC_PCM_S      MUROAR_CODEC_PCM_S_LE
#define MUROAR_CODEC_PCM_U      MUROAR_CODEC_PCM_U_LE
#else

#if BYTE_ORDER == BIG_ENDIAN
#define MUROAR_CODEC_PCM_S      MUROAR_CODEC_PCM_S_BE
#define MUROAR_CODEC_PCM_U      MUROAR_CODEC_PCM_U_BE
#elif BYTE_ORDER == LITTLE_ENDIAN
#define MUROAR_CODEC_PCM_S      MUROAR_CODEC_PCM_S_LE
#define MUROAR_CODEC_PCM_U      MUROAR_CODEC_PCM_U_LE
#else
// most likely a PDP
#define MUROAR_CODEC_PCM_S      MUROAR_CODEC_PCM_S_PDP
#define MUROAR_CODEC_PCM_U      MUROAR_CODEC_PCM_U_PDP
#endif

#endif

#define MUROAR_CODEC_PCM        MUROAR_CODEC_PCM_S

// muroar.c:
int muroar_connect(char * server, char * name);
int muroar_close  (int fh);
int muroar_quit   (int fh);
int muroar_beep   (int fh);
int muroar_stream (int fh, int dir, int * stream, int codec, int rate, int channels, int bits);

// muroario.c:
ssize_t muroar_write  (int fh, const void * buf, size_t len);
ssize_t muroar_read   (int fh,       void * buf, size_t len);

// muroar_noop.c:
int muroar_noop   (int fh);

#ifdef __cplusplus
}
#endif

#endif

//ll
