#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <threads.h>
#include <sockets.h>
#include <utils.h>
#include <delay.h>
#include <log_util.h>

static void setup_udp_listeners()
{
	server_info.udp_listen_sock = sock_get_server_socket(SOCK_TYPE_UDP, server_info.udp_port);
	if (INVALID_SOCKET == server_info.udp_listen_sock) {
		sys_debug(1, "ERROR, sock_get_server_socket() return invalid socket");
		return;
	}

	server_info.udp_running = SERVER_RUNNING;
	return;
}

static void handle_udp_connection(int sock)
{
	int ret;
	socklen_t sin_len;
	fd_set rfds;
	struct timeval tv;
	int maxfd = 0;
	char buf[BUFSIZE];
	struct sockaddr_in sin;

	/* setup sockaddr structure */
	sin_len = sizeof(struct sockaddr_in);
	memset((void *) &sin, 0, sin_len);
  
	/* try to accept a connection */
	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);
	if (sock > maxfd)
		maxfd = sock;
	maxfd += 1;

	tv.tv_sec = 0;
	tv.tv_usec = 30000;

	if ((ret = select(maxfd, &rfds, NULL, NULL, &tv)) > 0) {
		if (!(sock_valid(sock) && FD_ISSET(sock, &rfds))) {
			return;
		}
	} else if (ret == 0) {
		/* timeout, no connection come in */
		return;
	} else {
		/* error occurs*/
		sys_debug(1, "ERROR: select() complains: %s", strerror(errno));
		return;
	}

	ret = recvfrom(sock, buf, BUFSIZE, 0, (struct sockaddr *) &sin, &sin_len);
	if (ret >= 0) {
		char *client = make_host(&(sin.sin_addr));
		sys_debug(2, "DEBUG: recv data from client %s", client == NULL ? "unknown client" : client);
		sock_sendto(sock, buf, strlen(buf), 0, (struct sockaddr *) &sin, sin_len);

		memset(buf, 0, BUFSIZE);
		if (client) {
			free(client);
			client = NULL;
		}
	}

	if (ret == -1 && !is_recoverable(errno))
		sys_debug(1, "WARNING: accept() failed with on socket %d, max: %d, [%d:%s]", sock, maxfd, 
			  errno, strerror(errno));
}

static void *socket_udp_server_thread(void *arg)
{
	func_enter();

	/* Setup listeners */
	setup_udp_listeners();

	while (server_info.udp_running == SERVER_RUNNING) {
		handle_udp_connection(server_info.udp_listen_sock);

	}

	if (server_info.udp_listen_sock >= 0)
		sock_close(server_info.udp_listen_sock);
	return NULL;
}

void socket_udp_server_test_entry()
{
	func_enter();
	pthread_t udp_server_thread;
	printf("start UDP server thread...\n");

	if (thread_create(&udp_server_thread, socket_udp_server_thread, NULL)) {
		printf("start UDP server thread failed!\n");
	}

	func_exit();
}

