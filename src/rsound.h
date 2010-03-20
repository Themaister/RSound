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

#ifndef RSOUND_H
#define RSOUND_H

#include "audio.h"

#define RSD_VERSION "0.6"

void parse_input(int, char**);
void new_sound_thread(connection_t);
int set_up_socket();
void write_pid_file(void);
void cleanup(int);
void initialize_audio(void);

extern char device[];
extern char port[];
extern int verbose;
extern int no_threading;
extern rsd_backend_callback_t *backend;
extern int daemonize;
extern int debug;

#endif 
