/*
 * Epoll demo from Bluetooth protocol stack for Linux
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

#include "epoll_loop.h"
#include "timeout.h"
#include "uart.h"
#include "utils.h"
#include "log_ext.h"

static void signal_cb(int signum, void *user_data)
{
	log_warn("Got signal %d\n", signum);
	switch (signum) {
	case SIGINT:
	case SIGTERM:
		epoll_quit();
		break;
	default:
		break;
	}
}

int main(int argc, char *argv[])
{
	sigset_t mask;
	epoll_init();

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);

	epoll_set_signal(&mask, signal_cb, NULL, NULL);
	uart_init();

	epoll_run();
	return EXIT_SUCCESS;
}
