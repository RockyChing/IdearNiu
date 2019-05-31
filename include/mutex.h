#ifndef _MUTEX_H
#define _MUTEX_H
#include <pthread.h>

typedef pthread_mutex_t mutex_t;

enum {
    MUTEX_PRIVATE = 0,
    MUTEX_SHARED = 1
};

void mutex_init(mutex_t *mutex, int type);
void mutex_destroy(mutex_t *mutex);

#define lock(mutex) pthread_mutex_lock(&(mutex))
#define trylock(mutex) pthread_mutex_trylock(&(mutex))
#define unlock(mutex) pthread_mutex_unlock(&(mutex))

#endif
