#ifndef _THREADS_H
#define _THREADS_H
#include <time.h>
#include <pthread.h>

#define THREAD_CREATED -1
#define THREAD_RUNNING  1
#define THREAD_KILLED  -2
#define THREAD_EXITED  -3

#if 0
typedef struct _thread_t {
	pthread_t thread_id;
	time_t created;
	int running;
} thread_t;
#endif

int thread_create(pthread_t *thread_id, void *(*start_routine)(void *), void *arg);
#if 0
inline pthread_t thread_self()
{
    return pthread_self();
}
#else
#define thread_self() {}
#endif









#endif

