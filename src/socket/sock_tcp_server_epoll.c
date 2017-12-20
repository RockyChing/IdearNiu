#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <threads.h>
#include <sockets.h>
#include <utils.h>
#include <delay.h>
#include <log_util.h>

#define MAX_EPOLL_FDS	(1024 * 10)

#if 0
typedef union epoll_data {
   void        *ptr;
   int          fd;
   uint32_t     u32;
   uint64_t     u64;
} epoll_data_t;

struct epoll_event {
   uint32_t     events;      /* Epoll events */
   epoll_data_t data;        /* User data variable */
};
#endif

static connection_t *get_connection(SOCKET sock)
{
	int sockfd = -1;
	connection_t *con = NULL;

	socklen_t sin_len;
	struct sockaddr_in sin;

	/* setup sockaddr structure */
	sin_len = sizeof(struct sockaddr_in);
	memset(&sin, 0, sin_len);

	/* try to accept a connection */
	sockfd = sock_accept(sock, (struct sockaddr *)&sin, &sin_len);
	assert_return(sockfd >= 0);

	con = create_connection();
	assert_return(con);

	con->sin = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
	assert_return(con->sin);
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

static void *socket_tcp_epoll_thread(void *arg)
{
	connection_t *con = NULL;
	char buff[BUFSIZE] = {0};
	struct epoll_event ev, events[MAX_EPOLL_FDS];
	int epfd, nready_fds;
	int ret, i;
	ssize_t nr;
	func_enter();

	/* Setup listeners */
	setup_tcp_listeners();
	bzero(&events, sizeof(struct epoll_event) * MAX_EPOLL_FDS);

	/*
	 * epoll_create() creates an epoll instance. Since Linux 2.6.8, the
     * size argument is ignored, but must be greater than zero.
     *
     * On success, these system calls return a nonnegative file descriptor.
     * On error, -1 is returned, and errno is set to indicate the error.
	 */
	epfd = epoll_create(MAX_EPOLL_FDS);
	assert_return(epfd != -1);
	ev.data.fd = server_info.tcp_listen_sock;
	ev.events = EPOLLIN | EPOLLET;
	/*
	 * register epoll events
	 *
	 * On success, returns zero.
	 * On error, returns -1 and errno is set appropriately.
	 */
	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, server_info.tcp_listen_sock, &ev);
	assert_return(ret == 0);
	//ret = epoll_ctl(epfd, EPOLL_CTL_DEL, server_info.tcp_listen_sock, &ev);
	//assert_return(ret == 0);

	while (server_info.tcp_running == SERVER_RUNNING) {
		/*
		 * int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
		 * waits for events on the epoll instance referred to by the file descriptor epfd.
		 *
		 * On success, returns the number of file descriptors ready for the requested I/O,
		               or zero if no file descriptor became ready during the requested timeout milliseconds.
		 * On error, returns -1 and errno is set appropriately.
		 */
		nready_fds = epoll_wait(epfd, events, MAX_EPOLL_FDS, 1500);
		sys_debug(3, "DEBUG: nready_fds %d", nready_fds);
		for (i = 0; i < nready_fds; i ++) {
			if (events[i].data.fd == server_info.tcp_listen_sock) {
				con = get_connection(server_info.tcp_listen_sock);

				if (con) {
					//pthread_t ptid;
					/* handle the new connection it in a new thread */
					//thread_create(&ptid, handle_connection, (void *) con);
					sock_set_blocking(con->sock, SOCKET_NONBLOCK);
					ev.data.ptr = con;
					/* The default behavior for epoll is Level Triggered
					 * In this mode we will got 'EPOLLIN' continuously, even when
					 * the socket closed by the clients
					 */
					ev.events = EPOLLIN | EPOLLET;
					ret = epoll_ctl(epfd, EPOLL_CTL_ADD, con->sock, &ev);
					if (ret != 0) {
						sys_debug(1,
			 				"ERROR: epoll_ctl() EPOLL_CTL_ADD error");
						clean_connection(con);
						ev.data.ptr = NULL;
						con = NULL;
					}
				}
			} else if (events[i].events & EPOLLIN) {
				printf("epoll IN event\n");
				if (events[i].data.fd <= 0)
					continue;
				if (events[i].data.ptr) {
					con = (connection_t *) events[i].data.ptr;
					/**
					 * length of message in bytes that received,
					 * 0 if no messages are available and peer has done an orderly shutdown,
					 * or −1 on error
					 */
					nr = recv(con->sock, buff, BUFSIZE, 0);
					if (nr > 0) {
		                con->read_statistics += nr;
						for (i = 0; i < nr; i ++) {
							printf("%c", buff[i]);
						}
						memset(buff, 0, BUFSIZE);
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
						//con->running = CLIENT_DYING;
						sys_debug(1, "peer has done an orderly shutdown");
						/* recycle */
						clean_connection(con);
						events[i].data.ptr = NULL;
						con = NULL;
					} else {
						/* recv() error */
						if (EAGAIN == errno) {
							sys_debug(1, "recv() got EAGAIN");
						} else {
							//con->running = CLIENT_DYING;
							sys_debug(1, "recv() errno,, %s", strerror(errno));
							/* recycle */
							clean_connection(con);
							events[i].data.ptr = NULL;
							con = NULL;
						}
					}
				}

				if (con != NULL) {
					#if 0
					ev.data.fd = con->sock;
					/* register send event */
					ev.events = EPOLLOUT | EPOLLET;
					ret = epoll_ctl(epfd, EPOLL_CTL_MOD, con->sock, &ev);
					if (ret != 0) {
						sys_debug(1,
			 				"ERROR: epoll_ctl() EPOLL_CTL_ADD error");
						clean_connection(con);
						ev.data.ptr = NULL;
						con = NULL;
					}
					#endif
				}
			} else if (events[i].events & EPOLLOUT) {
				printf("epoll OUT event\n");
				#if 0
				int sockfd = events[i].data.fd;
				write(sockfd, "ssssss", strlen("ssssss"));
				ev.data.fd = sockfd;
				ev.data.ptr = events[i].data.ptr;
				ev.events = EPOLLIN | EPOLLET;
				/* register recv event */
				epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
				#endif
			}
		}
	}

	if (con) {
		clean_connection(con);
		con = NULL;
	}
	return NULL;
}




void socket_tcp_server_epoll_test_entry()
{
	func_enter();
	pthread_t tcp_epoll_thread;
	printf("start TCP server thread...\n");

	if (thread_create(&tcp_epoll_thread, socket_tcp_epoll_thread, NULL)) {
		printf("start TCP server thread failed!\n");
	}

	func_exit();
}

