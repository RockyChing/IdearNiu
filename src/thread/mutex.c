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
void mutex_init(mutex_t *mutex, int type)
{
    if (MUTEX_SHARED == type) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    } else {
        pthread_mutex_init(mutex, NULL);
    }
}

void mutex_destroy(mutex_t *mutex)
{
    pthread_mutex_destroy(mutex);
}

