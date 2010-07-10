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

#ifndef RSOUND_H
#define RSOUND_H

#include "audio.h"

#undef CONST_CAST
#ifdef _WIN32
#undef close
#define close(x) closesocket(x)
#define CONST_CAST (const char*)
#else
#define CONST_CAST
#endif

void parse_input(int, char**);
void new_sound_thread(connection_t);
int set_up_socket();
void write_pid_file(void);
#ifdef _WIN32
void cleanup(void);
#else
void cleanup(int);
#endif
void initialize_audio(void);

extern char device[];
extern char bindaddr[];
extern char port[];
extern char unix_sock[];
extern int verbose;
extern int no_threading;
extern const rsd_backend_callback_t *backend;
extern int daemonize;
extern int debug;
extern int rsd_conn_type;

#endif 
