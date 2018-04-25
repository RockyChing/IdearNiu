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

#define SHOW_PTHREAD_PRIORITY 0

#if SHOW_PTHREAD_PRIORITY == 1
/**
 * SCHED_OTHER: 分时调度策略
 * SCHED_FIFO: 实时调度策略，先到先服务。一旦占用cpu则一直运行。一直运行直到有更高优先级任务到达或自己放弃
 * SCHED_RR: 实时调度策略，时间片轮转。当进程的时间片用完，系统将重新分配时间片，并置于就绪队列尾。放在队列尾保证了所有具有相同优先级的RR任务的调度公平
 *
 * Refer to Linux command 'chrt' to obtain these message, like:
 * $ chrt -m
 *   SCHED_OTHER min/max priority	: 0/0
 *   SCHED_FIFO min/max priority	: 1/99
 *   SCHED_RR min/max priority		: 1/99
 *   SCHED_BATCH min/max priority	: 0/0
 *   SCHED_IDLE min/max priority	: 0/0
 *
 * $ chrt -p $$
 *   pid 3297's current scheduling policy: SCHED_OTHER
 *   pid 3297's current scheduling priority: 0
 *
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

#if 0
	if (type != THREAD_ONE_SHOT) {
		rwlock_wrlock(&thread_rwlock);
		if (restart) {
			thread_objs[index].ptrd_id = *thread_id;
			thread_objs[index].type = type;
			thread_objs[index].runner = start_routine;
			thread_objs[index].arg = arg;
		} else {
			if (pthrd_index >= MAX_THREAD_NUMBER) {
				error("Too many threads have created!");
				rwlock_unlock(&thread_rwlock);
				return 1;
			}

			for (i = 0; i < pthrd_index; i ++) {
				if (!strcmp(thread_objs[i].name, name)) {
					// same thread exist;
					break;
				}
			}

			thread_objs[i].state = THREAD_RUNNING;
			thread_objs[i].ptrd_id = *thread_id;
			thread_objs[i].type = type;
			thread_objs[i].runner = start_routine;
			thread_objs[i].arg = arg;
			if (i == pthrd_index) {
				// not register
				thread_objs[i].name = strdup(name);
				pthrd_index ++;
			}
		}

		rwlock_unlock(&thread_rwlock);
		debug("'%s' registered!!!!!!!!!!!!!!!!!!!!!!\n\n\n\n\n", thread_objs[i].name);
	}
#endif
	pthread_attr_destroy(&attr); /* Not strictly necessary */
	return 0;
}

#if 0
void thread_pool_init()
{
	memset(thread_objs, 0, sizeof(thread_objs));
	rwlock_init(&thread_rwlock);
}

void thread_pool_destroy()
{
	rwlock_destroy(&thread_rwlock);
}

int thread_kill(const char *name)
{
	int i;
	debug("pthrd_index: %d", pthrd_index);
	rwlock_wrlock(&thread_rwlock);
	for (i = 0; i < pthrd_index; i ++) {
		if (!strcmp(thread_objs[i].name, name)) {
			if (thread_objs[i].state == THREAD_RUNNING) {
				if (pthread_cancel(thread_objs[i].ptrd_id)) {
					warning("Cancel thread error: %s", strerror(errno));
				}

				if (pthread_join(thread_objs[i].ptrd_id, NULL)) {
					warning("Join thread error: %s", strerror(errno));
				}

				thread_objs[i].state = THREAD_EXITED;
				info("Kill \"%s\" done!", thread_objs[i].name);
			}
		}
	}
	rwlock_unlock(&thread_rwlock);
}

int thread_get_state(const char *name)
{
	int i;
	rwlock_rdlock(&thread_rwlock);
	for (i = 0; i < pthrd_index; i ++) {
		if (!strcmp(thread_objs[i].name, name)) {
			rwlock_unlock(&thread_rwlock);
			return thread_objs[i].state;
		}
	}
	rwlock_unlock(&thread_rwlock);

	return THREAD_EXITED;
}

void thread_set_state(const char *name, int state)
{
	int i;
	rwlock_wrlock(&thread_rwlock);
	for (i = 0; i < pthrd_index; i ++) {
		if (!strcmp(thread_objs[i].name, name)) {
			thread_objs[i].state = state;
		}
	}
	rwlock_unlock(&thread_rwlock);
}

void thread_enable_cancellation()
{
	int now, before;
	now = PTHREAD_CANCEL_ENABLE;
	/* int pthread_setcancelstate(int state, int *oldstate); */
	pthread_setcancelstate(now, &before);

	/* int pthread_setcanceltype(int type, int *oldtype); */
	now = PTHREAD_CANCEL_ASYNCHRONOUS;
	pthread_setcanceltype(now, &before);
}

/**
 * start_daemon_thread - Listen on running threads, if someone exits we'll restart it
 *
 * pthread_kill - send a signal to a thread
 * If sig is 0, then no signal is sent, but error checking is still performed.
 * On success, pthread_kill() returns 0;
 * on error, it returns  an  error number, and no signal is sent.
 */
void start_daemon_thread()
{
	int i;
	for (; ;) {
		sleep(50);
	}
#if 0
		if (get_wifi_config_state())
			continue;
		debug("totally %d daemon threads are running...", pthrd_index);
		for (i = 0; i < pthrd_index; i++) {
			if (pthread_kill(thread_objs[i].ptrd_id, 0) != 0) {
				/* pthread exits, restart it */
				if (!thread_create_internal(&thread_objs[i].ptrd_id, thread_objs[i].runner, thread_objs[i].arg,
							thread_objs[i].type, 1, i)) {
					warning("thread restart success!");
				}
			}
		}
	}
#endif
}
#endif

