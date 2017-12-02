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

#define SHOW_PTHREAD_PRIORITY 1

#if SHOW_PTHREAD_PRIORITY == 1
/**
 * SCHED_OTHER: 分时调度策略
 * SCHED_FIFO: 实时调度策略，先到先服务。一旦占用cpu则一直运行。一直运行直到有更高优先级任务到达或自己放弃
 * SCHED_RR: 实时调度策略，时间片轮转。当进程的时间片用完，系统将重新分配时间片，并置于就绪队列尾。放在队列尾保证了所有具有相同优先级的RR任务的调度公平
 */
static int get_pthread_policy(pthread_attr_t *attr)
{
    int policy;
    int ret = pthread_attr_getschedpolicy (attr, &policy);
    assert(ret == 0);

    printf("pthread schedule policy: ");
    switch (policy) {
    case SCHED_FIFO:
        printf("SCHED_FIFO\n");
        break;
    case SCHED_RR:
        printf("SCHED_RR");
        break;
    case SCHED_OTHER:
        printf("SCHED_OTHER\n");
        break;
    default:
        printf("UNKNOWN\n");
        break;
    }

    return policy;
}

static void set_pthread_policy (pthread_attr_t *attr,int policy)
{
    int ret = pthread_attr_setschedpolicy(attr, policy);
    assert (ret == 0);
}

static void show_pthread_priority_min_max(pthread_attr_t *attr, int policy)
{
    int priority = sched_get_priority_min(policy);
    assert(priority != -1);
    printf("pthread min_priority = %d\n", priority);

    priority = sched_get_priority_max(policy);
    assert(priority != -1);
    printf ("pthread max_priority = %d\n", priority);
}

static int get_pthread_priority (pthread_attr_t *attr)
{
    struct sched_param param;
    int ret = pthread_attr_getschedparam(attr, &param);
    assert (ret == 0);

    printf ("current pthread priority = %d\n", param.__sched_priority);
    return param.__sched_priority;
}

#endif

int thread_create(pthread_t *thread_id, void *(*start_routine)(void *), void *arg)
{
	pthread_attr_t attr;
	int i;

	pthread_attr_init(&attr);

#if SHOW_PTHREAD_PRIORITY == 1
	int policy = get_pthread_policy(&attr);

	//set_pthread_policy(&attr, SCHED_FIFO);
	//policy = get_pthread_policy(&attr);

	show_pthread_priority_min_max(&attr, policy);
	get_pthread_priority(&attr);
#endif
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

