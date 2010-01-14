/*  RSound - A PCM audio client/server
 *  Copyright (C) 2009 - Hans-Kristian Arntzen
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

#ifndef PORTA_H
#define PORTA_H

#include "audio.h"
#include "portaudio.h"


typedef struct
{
   PaStream *stream;
   char *buffer;
   size_t size;
} porta_t;



static int init_porta(porta_t*, wav_header*);
static void clean_porta_interface(porta_t*);
void* porta_thread( void* socket );
extern int verbose;
#endif

