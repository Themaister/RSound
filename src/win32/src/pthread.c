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


#include "pthread.h"
#include <stdlib.h>

struct winthread_param
{
   void *(*proc)(void*);
   void *data;
};

pthread_t pthread_self(void)
{
   return GetCurrentThread();
}

static DWORD CALLBACK winthread_entry(void *param_)
{
   struct winthread_param param = *(const struct winthread_param*)param_;
   free(param_);
   param.proc(param.data);
   ExitThread(0);
   return 0;
}

int pthread_mutex_init(pthread_mutex_t * restrict mutex, void * restrict dummy)
{
   (void)dummy;

   *mutex = CreateMutex(NULL, FALSE, NULL);
   if (!*mutex)
      return -1;
   return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
   WaitForSingleObject(*mutex, INFINITE);
   return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
   ReleaseMutex(*mutex);
   return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
   if (*mutex)
      CloseHandle(*mutex);
   return 0;
}

int pthread_cond_init(pthread_cond_t * restrict cond, void * restrict dummy)
{
   (void)dummy;

   *cond = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (!*cond)
      return -1;
   return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
   if (*cond)
      CloseHandle(*cond);
   return 0;
}

int pthread_cond_wait(pthread_cond_t * restrict cond, pthread_mutex_t * restrict mutex)
{
   WaitForSingleObject(*cond, 0); // Clear out current wait condition.
   pthread_mutex_unlock(mutex);
   // This has some different semantics than condition variables, but we don't really care ;D
   WaitForSingleObject(*cond, INFINITE);
   pthread_mutex_lock(mutex);

   return 0;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
   SetEvent(*cond);
   return 0;
}

int pthread_create(pthread_t *thread, void *dummy, void *(*start_routine)(void*), void *arg)
{
   (void)dummy;

   struct winthread_param *param = malloc(sizeof(*param));
   if (!param)
      return -1;
   param->proc = start_routine;
   param->data = arg;

   *thread = CreateThread(NULL, 0, winthread_entry, param, 0, NULL);
   if (!*thread)
      return -1;
   return 0;
}

int pthread_join(pthread_t thread, void **dummy)
{
   (void)dummy;

   WaitForSingleObject(thread, INFINITE);
   CloseHandle(thread);
   return 0;
}

int pthread_detach(pthread_t thread)
{
   // Not sure if we even need to do anything here ...
   (void)thread;

   return 0;
}
