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

typedef enum {CONNECT_TYPE_TCP = 0, CONNECT_TYPE_UDP = 1, CONNECT_TYPE_UNKNOWN = 3 } contype_t;

typedef struct {
	char *myhostname;	/* NULL unless bind to specific ip */
	char *server_name;	/* Server name */
	char *version;

	int port;
	SOCKET listen_sock;	/* Socket to listen to */

	pthread_t main_thread;
	pthread_mutex_t thread_mutex;
} server_info_t;

typedef struct {
	contype_t type;
	time_t connect_time;

	char *host;
	char *hostname;

	struct sockaddr_in *sin;
	socklen_t sinlen;
	SOCKET sock;
} connection_t;

static server_info_t server_info;
static int running = SERVER_INITIALIZING;

static char *makeasciihost(const struct in_addr *in, char *host)
{
	if (!in || !host) {
		sys_debug(1, "ERROR: makeasciihost called with NULL arguments");
		return NULL;
	}

	/**
	 * inet_ntoa() works only with IPv4 addresses
	 * inet_ntop() support similar functionality 
	 * and work with both IPv4 and IPv6 addresses
	 */
	//strncpy(host, inet_ntoa(*in), 20);
	if (inet_ntop(AF_INET, (void *)in, host, INET_ADDRSTRLEN)) {
	} else {
		/**
		 * NULL is returned if there was an error, with errno set to indicate the error.
		 */
		sys_debug(1, "ERROR: inet_ntop() return NULL, error: %s", strerror(errno));
		return NULL;
	}

#if 0
	unsigned char *s = (unsigned char *)in;
	int a, b, c, d;
	a = (int)*s++;
	b = (int)*s++;
	c = (int)*s++;
	d = (int)*s;

	snprintf(buf, 20, "%d.%d.%d.%d", a, b, c, d);
#endif
	return host;
}

static char *create_malloced_ascii_host(struct in_addr *in)
{
	/**
	 * INET_ADDRSTRLEN is large enough to hold a text string
	 * representing an IPv4 address, and INET6_ADDRSTRLEN is
	 * large enough to hold a text string representing an IPv6 address.
	 */
	char *buf = NULL;
	if (!in) {
		sys_debug(1, "ERROR: Dammit, don't send NULL's to create_malloced_ascii_host()");
		return NULL;
	}

	buf = (char *) malloc(INET_ADDRSTRLEN + 1);
	if (!buf) {
		sys_debug(1, "ERROR: Opps, malloc() return NULL");
		return NULL;
	}

	buf[INET_ADDRSTRLEN] = '\0';
	return makeasciihost(in, buf);
}

static void setup_listeners()
{
	memset((void *)&server_info, 0, sizeof(server_info_t));
	server_info.port = 8800;
	server_info.listen_sock = sock_get_server_socket(server_info.port);
	if (INVALID_SOCKET == server_info.listen_sock) {
		sys_debug(1, "ERROR, sock_get_server_socket() return invalid socket");
		return;
	}

	/* Set the socket to nonblocking */
	sock_set_blocking(server_info.listen_sock, SOCKET_NONBLOCK);
	if (listen(server_info.listen_sock, LISTEN_QUEUE) == SOCKET_ERROR) {
		sys_debug(1, "ERROR, listen() return error");
		goto fail;
	}

	running = SERVER_RUNNING;
	return;
	/** add this socket to a manage list */
fail:
	if (server_info.listen_sock != INVALID_SOCKET)
		sock_close(server_info.listen_sock);
}

static connection_t *create_connection()
{
	connection_t *con = (connection_t *) malloc (sizeof (connection_t));
	if (!con) return NULL;
	con->type = CONNECT_TYPE_UNKNOWN;
	con->sin = NULL;
	con->host = NULL;
	con->hostname = NULL;
	con->sin = NULL;
	return con;
}

static void clean_connection(connection_t *con)
{
	if (!con) return;
	if (con->host) {
		free(con->host);
	} else if (con->hostname) {
		free(con->hostname);
	} else if (con->sin) {
		free(con->sin);
	}

	free(con);
}

static void handle_recv(const connection_t *con)
{
	char buf[BUFSIZE] = {0};
	int i, res;
	fd_set rfd;
	struct timeval tv;
	if (!con) return;

	FD_ZERO(&rfd);
	FD_SET(con->sock,&rfd);

	tv.tv_sec =0;
	tv.tv_usec = 30000;

	if (select(con->sock + 1,&rfd, NULL, NULL, &tv) != -1) {
		if (FD_ISSET(con->sock, &rfd)) {
			/**
			 * length of message in bytes that received,
			 * 0 if no messages are available and peer has done an orderly shutdown,
			 * or −1 on error
			 */
			res = recv(con->sock, buf, BUFSIZE, 0);
			if (res >= 0) {
				for (i = 0; i < res; i ++) {
					printf("%c ", buf[i]);
				}
				memset(buf, 0, BUFSIZE);
				printf("\n");
			} else {
				switch (errno){
				case EINTR:
					sys_debug(1, "Interrupt signal EINTR caught");
					break;
				case EAGAIN:
					sys_debug(1, "Interrupt signal EAGAIN caught");
					break;
				case EBADF:
					sys_debug(1, "Interrupt signal EBADF caught");
					break;
				case EINVAL:
					sys_debug(1, "Interrupt signal EINVAL caught");
					break;
				case EFAULT:
					sys_debug(1, "Interrupt signal EFAULT caught");
					break;
				case EIO:
					sys_debug(1, "Interrupt signal EIO caught");
					break;
				case EISDIR:
					sys_debug(1, "Interrupt signal EISDIR caught");
					break;
				default:
					sys_debug(1, "Unknown interrupt signal caught\n");
				}
			}
		}
	} else {
		switch (errno){
		case EINTR:
			sys_debug(1, "Interrupt signal EINTR caught");
			break;
		case EAGAIN:
			sys_debug(1, "Interrupt signal EAGAIN caught");
			break;
		case EBADF:
			sys_debug(1, "Interrupt signal EBADF caught");
			break;
		case EINVAL:
			sys_debug(1, "Interrupt signal EINVAL caught");
			break;
		default:
			sys_debug(1, "Unknown interrupt signal caught\n");
		}
	}
}

/*
 * This is called to handle a brand new connection, in it's own thread.
 * Nothing is know about the type of the connection.
 */
static void *handle_connection(void *arg)
{
	const connection_t *con = (connection_t *) arg;

	if (!con) {
		sys_debug(1, "ERROR, handle_connection: got NULL connection");
		return NULL;
	}

	sock_set_blocking(con->sock, SOCKET_NONBLOCK);
	while (1) {
		handle_recv(con);
	}

	return NULL;
}

static connection_t *get_connection(SOCKET sock)
{
	int sockfd;
	socklen_t sin_len;
	connection_t *con;
	fd_set rfds;
	struct timeval tv;
	int maxfd = 0;
	struct sockaddr_in *sin = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	if (!sin) {
		sys_debug(1, "WARNING: Weird stuff in get_connection. nmalloc returned NULL sin");
		return NULL;
	}

	/* setup sockaddr structure */
	sin_len = sizeof(struct sockaddr_in);
	memset(sin, 0, sin_len);
  
	/* try to accept a connection */
	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);
	if (sock > maxfd)
		maxfd = sock;
	maxfd += 1;

	tv.tv_sec = 0;
	tv.tv_usec = 30000;

	if (select(maxfd, &rfds, NULL, NULL, &tv) > 0) {
		if (!(sock_valid(sock) && FD_ISSET(sock, &rfds))) {
			free(sin);
			return NULL;
		}
	} else {
		free(sin);
		return NULL;
	}

	sockfd = sock_accept(sock, (struct sockaddr *)sin, &sin_len);
	if (sockfd >= 0) {
		con = create_connection();
		if (!sin) {
			sys_debug(1, "ERROR: NULL sockaddr struct");
			return NULL;
		}

		if (!con) {
			sys_debug(1, "ERROR: NULL create_connection");
			return NULL;
		}

		con->type = CONNECT_TYPE_TCP;
		con->sock = sockfd;
		con->sin = sin;
		con->sinlen = sin_len;
		con->host = create_malloced_ascii_host(&(sin->sin_addr));
		sys_debug(2, "DEBUG: Getting new connection on socket %d from host %s",
					sockfd, con->host == NULL ? "(null)" : con->host);
		return con;
	}

	if (!is_recoverable(errno))
		sys_debug(1, "WARNING: accept() failed with on socket %d, max: %d, [%d:%s]", sock, maxfd, 
			  errno, strerror(errno));
	free(sin);
	return NULL;
}

static void *socket_tcp_server_thread(void *arg)
{
	pthread_t ptid;
	connection_t *con;
	func_enter();
	init_network();
	/* Setup listeners */
	setup_listeners();

	while (running == SERVER_RUNNING) {
		con = get_connection(server_info.listen_sock);

		if (con) {
			/* handle the new connection it in a new thread */
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


