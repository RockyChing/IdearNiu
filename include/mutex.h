#ifndef _MUTEX_H
#define _MUTEX_H
#include <pthread.h>

typedef pthread_mutex_t mutex_t;

enum {
    MUTEX_PRIVATE = 0,
    MUTEX_SHARED = 1
};

inline void mutex_init(mutex_t mutex, int type);
inline void mutex_destroy(mutex_t mutex);
inline int lock(mutex_t mutex);
inline int trylock(mutex_t mutex);
inline int unlock(mutex_t mutex);


#endif
