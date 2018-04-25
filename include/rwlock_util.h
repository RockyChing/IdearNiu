#ifndef _RWLOCK_UTIL_H
#define _RWLOCK_UTIL_H

typedef pthread_rwlock_t rwlock_t;

/**
 * Both return: 0 if OK, error number on failure
 */
#define rwlock_init(rwlock) \
	pthread_rwlock_init(rwlock, NULL)

#define rwlock_destroy(rwlock) \
	pthread_rwlock_destroy(rwlock)

/**
 * All return: 0 if OK, error number on failure
 */
#define rwlock_rdlock(rwlock) \
	pthread_rwlock_rdlock(rwlock)

#define rwlock_wrlock(rwlock) \
	pthread_rwlock_wrlock(rwlock)

#define rwlock_unlock(rwlock) \
	pthread_rwlock_unlock(rwlock)

#endif

