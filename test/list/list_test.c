#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>
#include <sys/types.h>
#include <wait.h>
#include <list.h>
#include <utils.h>
#include <log_ext.h>
#include <mutex.h>

static struct list_head g_test_list_head;
static mutex_t g_mutex;

struct test_data {
	struct list_head node;
	void  *data;
	size_t data_len;
};

static void test_data_build(void)
{
	static int count = 0;
	char buffer[30];
	log_debug("test_data_build");
	struct test_data *d = zmalloc(sizeof(struct test_data));

	memset(buffer, 0, sizeof(buffer));
	snprintf(buffer, sizeof(buffer), "data index %d", count);
	d->data_len = strlen(buffer);
	d->data = zmalloc(d->data_len + 1);
	memcpy(d->data, buffer, d->data_len);

	log_debug("malloc p: %p", d);
	log_debug("malloc p->data: %p", d->data);

	lock(g_mutex);
	// list_add(&d->node, &g_test_list_head); FILO, stack
	list_add_tail(&d->node, &g_test_list_head); // FIFO, queue
	count += 1;
	unlock(g_mutex);
	alarm(2);
}

static void sig_handler(int signo)
{
	switch (signo) {
	case SIGALRM: /* don't exit when socket closeed */
		test_data_build();
		break;
	default:
		printf("Opps, got %d signal, process terminated!\n", signo );
		exit(0);
		break;
	}
}

int main(int argc, char *argv[])
{
	struct test_data *pdata = NULL;
	struct list_head *pos;
    log_info("start testing list...\n");

    INIT_LIST_HEAD(&g_test_list_head);
	mutex_init(&g_mutex, MUTEX_PRIVATE);

	struct sigaction sa;
	sa.sa_handler = sig_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, 0);

	alarm(5);
	while (1) {
		if (list_empty(&g_test_list_head)) {
			log_info("list is empty");
		}

		lock(g_mutex);
		list_for_each(pos, &g_test_list_head) {
			pdata = list_entry(pos, struct test_data, node);
			if (pdata) {
				log_debug("data: %s", pdata->data);
			}
		}

		if (pdata) {
			list_del(&pdata->node);
			log_debug("remove '%s'", pdata->data);
			log_debug("free p: %p", pdata);
			log_debug("free p->data: %p", pdata->data);

			xfree(pdata->data);
			xfree(pdata);
		}
		unlock(g_mutex);
		sleep_ms(500);
	}
}

