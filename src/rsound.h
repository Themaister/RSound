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

#define _POSIX_SOURCE

#include "audio.h"

void parse_input(int, char**);
void new_sound_thread(int);
void pheader(wav_header*);
void print_help(char*);
int get_wav_header(int, wav_header*);
int send_backend_info(int, uint32_t*, uint32_t, int);
int set_up_socket();
int recieve_data(int, char*, size_t, size_t);

extern char device[];
extern char port[];
extern int verbose;
extern int no_threading;
extern void* (*backend) ( void * );
extern int daemonize;

#endif 
