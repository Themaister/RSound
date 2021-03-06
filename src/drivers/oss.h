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

#ifndef OSS_H
#define OSS_H

#include "../audio.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../config.h"
#if HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#elif HAVE_SOUNDCARD_H
#include <soundcard.h>
#else
#error "Can not find suitable OSS header"
#endif

typedef struct
{
   int audio_fd;
   int conv;
   enum rsd_format fmt;
   int latency_enum;
   int latency_denom;
} oss_t;

#define OSS_DEVICE "/dev/dsp"

#endif

