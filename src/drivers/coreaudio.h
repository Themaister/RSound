/*  RSound - A PCM audio client/server
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *  Copyright (C) 2011 - Chris Moeller
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

#ifndef COREAUDIO_H
#define COREAUDIO_H

#include "../audio.h"
#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AUComponent.h>
#include <pthread.h>

typedef struct
{
   pthread_mutex_t mutex;
   pthread_cond_t cond;
   ComponentInstance audio_unit;

   int unit_allocated;
   int started;
   int stopping;
   void *buffer;

   unsigned int buffer_byte_count;
   unsigned int valid_byte_offset;
   unsigned int valid_byte_count;
} coreaudio_t;

#endif
