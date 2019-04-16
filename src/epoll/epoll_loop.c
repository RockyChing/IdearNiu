/*
 * Epoll demo from Bluetooth protocol stack for Linux
 */
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

#include "epoll_loop.h"
#include "utils.h"
#include "log_ext.h"

#define MAX_EPOLL_EVENTS 10

static int epoll_fd;
static int epoll_terminate;
static int exit_status = EXIT_SUCCESS;

struct mainloop_data {
	int fd;
	uint32_t events;
	mainloop_event_func callback;
	mainloop_destroy_func destroy;
	void *user_data;
};

#define MAX_MAINLOOP_ENTRIES 128

static struct mainloop_data *mainloop_list[MAX_MAINLOOP_ENTRIES];

struct timeout_data {
	int fd;
	mainloop_timeout_func callback;
	mainloop_destroy_func destroy;
	void *user_data;
};

struct signal_data {
	int fd;
	sigset_t mask;
	mainloop_signal_func callback;
	mainloop_destroy_func destroy;
	void *user_data;
};

static struct signal_data *g_signal_data;

void epoll_init(void)
{
	unsigned int i;

	epoll_fd = epoll_create1(EPOLL_CLOEXEC);

	for (i = 0; i < MAX_MAINLOOP_ENTRIES; i++)
		mainloop_list[i] = NULL;

	epoll_terminate = 0;
}

void epoll_quit(void)
{
	epoll_terminate = 1;
}

void epoll_exit_success(void)
{
	exit_status = EXIT_SUCCESS;
	epoll_terminate = 1;
}

void epoll_exit_failure(void)
{
	exit_status = EXIT_FAILURE;
	epoll_terminate = 1;
}

static void signal_callback(int fd, uint32_t events, void *user_data)
{
	struct signal_data *data = user_data;
	struct signalfd_siginfo si;
	ssize_t result;

	if (events & (EPOLLERR | EPOLLHUP)) {
		epoll_quit();
		return;
	}

	result = read(fd, &si, sizeof(si));
	if (result != sizeof(si))
		return;

	if (data->callback)
		data->callback(si.ssi_signo, data->user_data);
}

int epoll_run(void)
{
	unsigned int i;

	if (g_signal_data) {
		if (sigprocmask(SIG_BLOCK, &g_signal_data->mask, NULL) < 0)
			return EXIT_FAILURE;

		g_signal_data->fd = signalfd(-1, &g_signal_data->mask,
						SFD_NONBLOCK | SFD_CLOEXEC);
		if (g_signal_data->fd < 0)
			return EXIT_FAILURE;

		if (epoll_add_fd(g_signal_data->fd, EPOLLIN,
				signal_callback, g_signal_data, NULL) < 0) {
			close(g_signal_data->fd);
			return EXIT_FAILURE;
		}
	}

	while (!epoll_terminate) {
		struct epoll_event events[MAX_EPOLL_EVENTS];
		int n, nfds;

		nfds = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);
		if (nfds < 0)
			continue;

		for (n = 0; n < nfds; n++) {
			struct mainloop_data *data = events[n].data.ptr;

			data->callback(data->fd, events[n].events,
							data->user_data);
		}
	}

	if (g_signal_data) {
		epoll_remove_fd(g_signal_data->fd);
		close(g_signal_data->fd);

		if (g_signal_data->destroy)
			g_signal_data->destroy(g_signal_data->user_data);
	}

	for (i = 0; i < MAX_MAINLOOP_ENTRIES; i++) {
		struct mainloop_data *data = mainloop_list[i];

		mainloop_list[i] = NULL;

		if (data) {
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, data->fd, NULL);

			if (data->destroy)
				data->destroy(data->user_data);

			free(data);
		}
	}

	close(epoll_fd);
	epoll_fd = 0;

	return exit_status;
}

int epoll_add_fd(int fd, uint32_t events, mainloop_event_func callback,
				void *user_data, mainloop_destroy_func destroy)
{
	struct mainloop_data *data;
	struct epoll_event ev;
	int err;

	if (fd < 0 || fd > MAX_MAINLOOP_ENTRIES - 1 || !callback)
		return -EINVAL;

	data = malloc(sizeof(*data));
	if (!data)
		return -ENOMEM;

	memset(data, 0, sizeof(*data));
	data->fd = fd;
	data->events = events;
	data->callback = callback;
	data->destroy = destroy;
	data->user_data = user_data;

	memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.ptr = data;

	err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, data->fd, &ev);
	if (err < 0) {
		free(data);
		return err;
	}

	mainloop_list[fd] = data;

	return 0;
}

int epoll_modify_fd(int fd, uint32_t events)
{
	struct mainloop_data *data;
	struct epoll_event ev;
	int err;

	if (fd < 0 || fd > MAX_MAINLOOP_ENTRIES - 1)
		return -EINVAL;

	data = mainloop_list[fd];
	if (!data)
		return -ENXIO;

	memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.ptr = data;

	err = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, data->fd, &ev);
	if (err < 0)
		return err;

	data->events = events;

	return 0;
}

int epoll_remove_fd(int fd)
{
	struct mainloop_data *data;
	int err;

	if (fd < 0 || fd > MAX_MAINLOOP_ENTRIES - 1)
		return -EINVAL;

	data = mainloop_list[fd];
	if (!data)
		return -ENXIO;

	mainloop_list[fd] = NULL;

	err = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, data->fd, NULL);

	if (data->destroy)
		data->destroy(data->user_data);

	free(data);

	return err;
}

static void timeout_destroy(void *user_data)
{
	struct timeout_data *data = user_data;

	close(data->fd);
	data->fd = -1;

	if (data->destroy)
		data->destroy(data->user_data);

	free(data);
}

static void timeout_callback(int fd, uint32_t events, void *user_data)
{
	struct timeout_data *data = user_data;
	uint64_t expired;
	ssize_t result;

	if (events & (EPOLLERR | EPOLLHUP))
		return;

	result = read(data->fd, &expired, sizeof(expired));
	if (result != sizeof(expired))
		return;

	if (data->callback)
		data->callback(data->fd, data->user_data);
}

static inline int timeout_set(int fd, unsigned int msec)
{
	struct itimerspec itimer;
	unsigned int sec = msec / 1000;

	memset(&itimer, 0, sizeof(itimer));
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_nsec = 0;
	itimer.it_value.tv_sec = sec;
	itimer.it_value.tv_nsec = (msec - (sec * 1000)) * 1000 * 1000;

	return timerfd_settime(fd, 0, &itimer, NULL);
}

int epoll_add_timeout(unsigned int msec, mainloop_timeout_func callback,
				void *user_data, mainloop_destroy_func destroy)
{
	struct timeout_data *data;

	if (!callback)
		return -EINVAL;

	data = malloc(sizeof(*data));
	if (!data)
		return -ENOMEM;

	memset(data, 0, sizeof(*data));
	data->callback = callback;
	data->destroy = destroy;
	data->user_data = user_data;

	data->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (data->fd < 0) {
		free(data);
		return -EIO;
	}

	if (msec > 0) {
		if (timeout_set(data->fd, msec) < 0) {
			close(data->fd);
			free(data);
			return -EIO;
		}
	}

	if (epoll_add_fd(data->fd, EPOLLIN | EPOLLONESHOT,
				timeout_callback, data, timeout_destroy) < 0) {
		close(data->fd);
		free(data);
		return -EIO;
	}

	return data->fd;
}

int epoll_modify_timeout(int id, unsigned int msec)
{
	if (msec > 0) {
		if (timeout_set(id, msec) < 0)
			return -EIO;
	}

	if (epoll_modify_fd(id, EPOLLIN | EPOLLONESHOT) < 0)
		return -EIO;

	return 0;
}

int epoll_remove_timeout(int id)
{
	return epoll_remove_fd(id);
}

int epoll_set_signal(sigset_t *mask, mainloop_signal_func callback,
				void *user_data, mainloop_destroy_func destroy)
{
	struct signal_data *data;

	if (!mask || !callback)
		return -EINVAL;

	data = malloc(sizeof(*data));
	if (!data)
		return -ENOMEM;

	memset(data, 0, sizeof(*data));
	data->callback = callback;
	data->destroy = destroy;
	data->user_data = user_data;

	data->fd = -1;
	memcpy(&data->mask, mask, sizeof(sigset_t));

	free(g_signal_data);
	g_signal_data = data;

	return 0;
}
