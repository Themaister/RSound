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

typedef struct
{
   uint32_t latency;
   uint32_t chunk_size;
} backend_info_t;

void parse_input(int, char**);
void new_sound_thread(connection_t);
void pheader(wav_header*);
void print_help(char*);
int get_wav_header(connection_t, wav_header*);
int send_backend_info(connection_t, backend_info_t);
int set_up_socket();
int recieve_data(connection_t, char*, size_t);

extern char device[];
extern char port[];
extern int verbose;
extern int no_threading;
extern void* (*backend) ( void * );
extern int daemonize;

#endif 
