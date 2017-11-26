#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <time.h>

#include <mutex.h>

/**
 * @mutex the mutex to be initialized,
 * @type allow to specific the process-share attribute
 * Note PTHREAD_PROCESS_PRIVATE is default
 *
 * PTHREAD_PROCESS_PRIVATE: within a process
 * PTHREAD_PROCESS_SHARED: mutex shared between multiple processes
 */
inline void mutex_init(mutex_t mutex, int type)
{
    if (MUTEX_SHARED == type) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    } else {
        pthread_mutex_init(&mutex, NULL);
    }
}

inline void mutex_destroy(mutex_t mutex)
{
    pthread_mutex_destroy(&mutex);
}

/** 0 is OK, error number on failure */
inline int lock(mutex_t mutex)
{
    return pthread_mutex_lock(&mutex);
}

inline int unlock(mutex_t mutex)
{
    return pthread_mutex_unlock(&mutex);
}

inline int trylock(mutex_t mutex)
{
    return pthread_mutex_trylock(&mutex);
}


