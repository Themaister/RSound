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

#ifndef JACK_H
#define JACK_H

#include "../audio.h"

#include <jack/jack.h>

#define MAX_CHANS 6
typedef struct
{
   jack_client_t *client;
   jack_port_t *ports[MAX_CHANS];
   int channels;
   float latency;
} jack_t;

#define JACK_CLIENT_NAME "RSound"

#endif

