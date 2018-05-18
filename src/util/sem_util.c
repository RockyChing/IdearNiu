#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>

#include <log_util.h>

int sem_lock_init(sem_t *sem, unsigned int value)
{
	int ret;
	ret = sem_init(sem, 0, value);
	if (ret == -1) {
		error("Error in sem_lock_init(): %s", strerror(errno));
	}

	return ret;
}

void sem_lock(sem_t *sem)
{
    int ret;
    do {
        ret = sem_wait(sem);
    } while (ret < 0 && (errno == EINTR || errno == EAGAIN));

    if (ret < 0) {
        error("Error in UART lock: %s", strerror(errno));
    }
}

void sem_unlock(sem_t *sem)
{
    if (sem_post(sem) == -1) {
		if ((errno == EINTR || errno == EAGAIN)) {
			if (sem_post(sem) == -1) {
				error("Error in UART unlock: %s\n", strerror(errno));
            }
        }
	}
}

void sem_lock_destroy(sem_t *sem)
{
	int ret;
	ret = sem_destroy(sem);
	if (ret == -1) {
		error("Error in sem_lock_destroy(): %s", strerror(errno));
	}
}

