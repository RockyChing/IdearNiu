#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <type_def.h>
#include <log_util.h>
#include <threads.h>

#define PERIOD (1000 * 100) /* loop period 100 ms*/
// int thread_create(pthread_t *thread_id, void *(*start_routine)(void *), void *arg)

static void *start_routine(void *arg)
{
	sys_debug(0, "This is the %lu thread.\n", *((u32 *)arg));
	while (1) {
		sleep(5);
	}
	return NULL;
}

void thread_limits_test_entry()
{
	func_enter();

	int ret;
	u32 thread_total = 0;
	pthread_t tid;
	while (1) {
		thread_total += 1;
		ret = thread_create(&tid, start_routine, &thread_total);
		if (ret != 0)
			break;
		usleep(PERIOD);
	}

	sys_debug(0, "totally created threads: %lu\n", thread_total);
	func_exit();
}

