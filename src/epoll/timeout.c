/*
 * Epoll demo from Bluetooth protocol stack for Linux
 */
#include <stdlib.h>

#include "epoll_loop.h"
#include "timeout.h"
#include "utils.h"

struct timeout_data {
	int id;
	timeout_func_t func;
	timeout_destroy_func_t destroy;
	unsigned int timeout;
	void *user_data;
};

static void timeout_callback(int id, void *user_data)
{
	struct timeout_data *data = user_data;

	if (data->func(data->user_data) &&
			!epoll_modify_timeout(data->id, data->timeout))
		return;

	epoll_remove_timeout(data->id);
}

static void timeout_destroy(void *user_data)
{
	struct timeout_data *data = user_data;

	if (data->destroy)
		data->destroy(data->user_data);

	free(data);
}

unsigned int timeout_add(unsigned int timeout, timeout_func_t func,
			void *user_data, timeout_destroy_func_t destroy)
{
	struct timeout_data *data;

	data = new0(struct timeout_data, 1);
	data->func = func;
	data->user_data = user_data;
	data->timeout = timeout;
	data->destroy = destroy;

	data->id = epoll_add_timeout(timeout, timeout_callback, data,
							timeout_destroy);
	if (data->id < 0) {
		free(data);
		return 0;
	}

	return (unsigned int) data->id;
}

void timeout_remove(unsigned int id)
{
	if (!id)
		return;

	epoll_remove_timeout((int) id);
}
