#include "pthread.h"

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
   const struct winthread_param *param = param_;
   param->proc(param->data);
   ExitThread(0);
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
   CloseHandle(*mutex);
   return 0;
}

int pthread_cond_init(pthread_cond_t * restrict cond, void * restrict dummy)
{
   *cond = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (!*cond)
      return -1;
   return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
   CloseHandle(*cond);
   return 0;
}

int pthread_cond_wait(pthread_cond_t * restrict cond, pthread_mutex_t * restrict mutex)
{
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
   struct winthread_param param = {
      .proc = start_routine,
      .data = arg
   };
   *thread = CreateThread(NULL, 0, winthread_entry, &param, 0, NULL);
   if (!*thread)
      return -1;
   return 0;
}

int pthread_join(pthread_t thread, void **dummy)
{
   (void)dummy;
   WaitForSingleObject(thread, INFINITE);
   return 0;
}

int pthread_detach(pthread_t thread)
{
   // Not sure if we even need to do anything here ...
   (void)thread;
   return 0;
}
