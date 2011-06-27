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
