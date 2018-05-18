#ifndef _SEM_UTIL_H
#define _SEM_UTIL_H

int sem_lock_init(sem_t *sem, unsigned int value);
void sem_lock(sem_t *sem);
void sem_unlock(sem_t *sem);
void sem_lock_destroy(sem_t *sem);

#endif

