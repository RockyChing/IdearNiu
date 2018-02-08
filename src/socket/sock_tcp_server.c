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


#ifdef SOMAXCONN
#define LISTEN_QUEUE SOMAXCONN
#else
#define LISTEN_QUEUE 50
#endif

void setup_tcp_listeners()
{
	int ret = 0;
	do {
		server_info.tcp_listen_sock = sock_get_server_socket(SOCK_TYPE_TCP, server_info.tcp_port);
		if (INVALID_SOCKET == server_info.tcp_listen_sock) {
			sys_debug(1, "ERROR, sock_get_server_socket() return invalid socket");
			ret = INVALID_SOCKET;
			break;
		}

		/* Set the socket to nonblocking */
		sock_set_blocking(server_info.tcp_listen_sock, SOCKET_NONBLOCK);
		if (listen(server_info.tcp_listen_sock, LISTEN_QUEUE) == SOCKET_ERROR) {
			sys_debug(1, "ERROR, listen() return error");
			ret = SOCKET_ERROR;
			break;
		}
	} while (0);

	if (0 == ret) {
		server_info.tcp_running = SERVER_RUNNING;
		/** add this socket to a manage list */
	} else {
		if (server_info.tcp_listen_sock != INVALID_SOCKET)
		sock_close(server_info.tcp_listen_sock);
	}
}

connection_t *create_connection()
{
	connection_t *con = (connection_t *) xmalloc(sizeof (connection_t));
	if (!con) return NULL;

	con->type = SOCK_TYPE_UNKNOWN;
    con->connect_time = 0;
    con->read_statistics = 0;
	con->host = NULL;
	con->hostname = NULL;
	con->hostip = NULL;
	con->sin = NULL;
	con->sinlen = con->sock = -1;
	con->port = 0;
	return con;
}

void clean_connection(connection_t *con)
{
	if (!con) return;
	if (con->host) {
		free(con->host);
	} else if (con->hostname) {
		free(con->hostname);
	} else if (con->hostip) {
		free(con->hostip);
	}  else if (con->sin) {
		free(con->sin);
	} else if (con->sock >= 0) {
		sock_close(con->sock);
		con->sock = -1;
	} else {
	}

	free(con);
}

static void handle_recv(const connection_t *new_connection)
{
	char buf[BUFSIZE] = {0};
	int i, ret, nr;
	fd_set rfd;
	struct timeval tv;
    connection_t *con = (connection_t *) new_connection;
	if (!con) return;

	FD_ZERO(&rfd);
	FD_SET(con->sock,&rfd);

	tv.tv_sec =0;
	tv.tv_usec = 30000;

	if ((ret = select(con->sock + 1,&rfd, NULL, NULL, &tv)) > 0) {
		if (FD_ISSET(con->sock, &rfd)) {
			/**
			 * length of message in bytes that received,
			 * 0 if no messages are available and peer has done an orderly shutdown,
			 * or −1 on error
			 */
			nr = recv(con->sock, buf, BUFSIZE, 0);
			if (nr > 0) {
                con->read_statistics += nr;
				for (i = 0; i < nr; i ++) {
					printf("%c", buf[i]);
				}
				memset(buf, 0, BUFSIZE);
				printf("\n");
				char wb[BUFSIZE] = {0};
				int nw = snprintf(wb, BUFSIZE, "data received: %ld\n", con->read_statistics);
                if (sock_write_bytes(con->sock, wb, nw) < 0) {
					con->running = CLIENT_DYING;
					sys_debug(1, "ERROR, sent data to TCP client occurs \"%s\"", strerror(errno));
				}
                printf("-------------------------\n"
                       "data received: %ld\n"
                       "-------------------------\n\n", con->read_statistics);
			} else if (nr == 0) {
				con->running = CLIENT_DYING;
				sys_debug(1, "peer has done an orderly shutdown");
			} else {
				/* recv() error */
				if (EAGAIN == errno) {
					sys_debug(1, "recv() got EAGAIN");
				} else {
					con->running = CLIENT_DYING;
					sys_debug(1, "recv() errno,, %s", strerror(errno));
				}
			}
		}
	} else if (ret == 0) {
	} else {
		if (EAGAIN == errno) {
			sys_debug(1, "select() got EAGAIN");
		} else {
			con->running = CLIENT_DYING;
			sys_debug(1, "select() errno, %s", strerror(errno));
		}
	}
}

/*
 * This is called to handle a brand new connection, in it's own thread.
 * Nothing is know about the type of the connection.
 */
static void *handle_connection(void *arg)
{
	connection_t *con = (connection_t *) arg;
	func_enter();

	if (!con) {
		sys_debug(1, "ERROR, handle_connection: got NULL connection");
		return NULL;
	}

	sock_set_blocking(con->sock, SOCKET_NONBLOCK);
	con->running = CLIENT_RUNNING;
	while (CLIENT_RUNNING == con->running) {
		handle_recv(con);
	}

	func_exit();
	clean_connection(con);
	return NULL;
}

static connection_t *get_connection(SOCKET sock)
{
	int ret = -1;
	int sockfd = -1;
	connection_t *con = NULL;

	fd_set rfds;
	struct timeval tv;
	int maxfd = 0;

	socklen_t sin_len;
	struct sockaddr_in sin;

	/* setup sockaddr structure */
	sin_len = sizeof(struct sockaddr_in);
	memset(&sin, 0, sin_len);
  
	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);
	if (sock > maxfd)
		maxfd = sock;

	tv.tv_sec = 0;
	tv.tv_usec = 30000;

	/**
	 * @select returns: > 0 count of ready descriptors, 0 on timeout, −1 on error
	 */
	if ((ret = select(maxfd + 1, &rfds, NULL, NULL, &tv)) > 0) {
		if (!(sock_valid(sock) && FD_ISSET(sock, &rfds))) {
			return NULL;
		}
	} else if (ret == 0) {
		/* timeout, no connection come in */
		return NULL;
	} else {
		/* error occurs*/
		sys_debug(1, "ERROR: select() complains: %s", strerror(errno));
		return NULL;
	}

	/* try to accept a connection */
	sockfd = sock_accept(sock, (struct sockaddr *)&sin, &sin_len);
	if (sockfd >= 0) {
		con = create_connection();

		if (!con) {
			sys_debug(1, "ERROR: NULL create_connection");
			return NULL;
		}

		con->sin = (struct sockaddr_in *) xmalloc(sizeof(struct sockaddr_in));
		if (!con->sin) {
			sys_debug(1, "WARNING: Weird stuff in create_connection. nmalloc returned NULL sin");
			clean_connection(con);
			return NULL;
		}

		con->type = SOCK_TYPE_TCP;
        con->connect_time = get_time();
        con->read_statistics = 0;
		con->sock = sockfd;
		memcpy((void *)con->sin, (void *)&sin, sin_len);
		con->sinlen = sin_len;
		con->host = make_host(&(sin.sin_addr));
		sys_debug(2, "DEBUG: Getting new connection on socket %d from host %s, connect time %s",
					sockfd, con->host == NULL ? "(null)" : con->host, get_ctime(&con->connect_time));
		return con;
	}

	if (!is_recoverable(errno))
		sys_debug(1, "WARNING: accept() failed with on socket %d, max: %d, [%d:%s]", sock, maxfd, 
			  errno, strerror(errno));

	return NULL;
}

static void *socket_tcp_server_thread(void *arg)
{
	connection_t *con = NULL;
	func_enter();

	/* Setup listeners */
	setup_tcp_listeners();

	while (server_info.tcp_running == SERVER_RUNNING) {
		con = get_connection(server_info.tcp_listen_sock);

		if (con) {
			pthread_t ptid;
			/* one-thread-per-client: handle the new connection it in a new thread */
			thread_create(&ptid, handle_connection, (void *) con);
		}
	}

	if (con) {
		clean_connection(con);
		con = NULL;
	}
	return NULL;
}

void socket_tcp_server_test_entry()
{
	func_enter();
	pthread_t tcp_server_thread;
	printf("start TCP server thread...\n");

	if (thread_create(&tcp_server_thread, socket_tcp_server_thread, NULL)) {
		printf("start TCP server thread failed!\n");
	}

	func_exit();
}


