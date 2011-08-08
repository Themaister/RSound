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


#ifndef PTHREAD_H__
#define PTHREAD_H__
#ifdef _WIN32

// Trivial implementation of the subset of features that's needed for Win32, 
// so we can avoid another DLL and link statically :)

#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef HANDLE pthread_t;
typedef HANDLE pthread_cond_t;
typedef HANDLE pthread_mutex_t;

int pthread_mutex_init(pthread_mutex_t * restrict mutex, void * restrict dummy);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_destroy(pthread_mutex_t *mutex);

int pthread_cond_init(pthread_cond_t * restrict cond, void * restrict dummy);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_destroy(pthread_cond_t *cond);

int pthread_cond_wait(pthread_cond_t * restrict cond, pthread_mutex_t * restrict mutex);

int pthread_create(pthread_t *thread, void *dummy, void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t thread, void **dummy);
int pthread_detach(pthread_t thread);

pthread_t pthread_self(void);

#define pthread_exit(RETVAL) return RETVAL


#endif
#endif
