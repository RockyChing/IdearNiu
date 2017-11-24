#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include <threads.h>

int thread_create(pthread_t *thread_id, void *(*start_routine)(void *), void *arg)
{
	pthread_attr_t attr;
	int i;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
#ifdef HAVE_PTHREAD_ATTR_SETSTACKSIZE
	pthread_attr_setstacksize(&attr, 1024*250);
#endif

	for (i = 0; i < 10; i ++) {
		if(pthread_create(thread_id, &attr, start_routine, arg) != 0) {
			if (EAGAIN == errno) {
				printf("Interrupt signal EAGAIN caught!\n");
				continue;
			} else {
				printf("Could not create thread, error: %s\n", strerror(errno));
			}
			return 1;
		} else {
			printf("thread created sucessed!\n");
			break;
		}
	}

	pthread_attr_destroy(&attr); /* Not strictly necessary */
	return 0;
}

