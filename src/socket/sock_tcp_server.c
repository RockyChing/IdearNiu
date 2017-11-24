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
#include <log_util.h>

#ifdef SOMAXCONN
#define LISTEN_QUEUE SOMAXCONN
#else
#define LISTEN_QUEUE 50
#endif

typedef struct {
	char *myhostname;	/* NULL unless bind to specific ip */
	char *server_name;	/* Server name */
	char *version;

	int port;
	SOCKET listen_sock;	/* Socket to listen to */

	pthread_t main_thread;
	pthread_mutex_t thread_mutex;
} server_info_t;

static server_info_t server_info;
static int running = SERVER_INITIALIZING;

static void makeasciihost(const struct in_addr *in, char *host)
{
	if (!in || !host) {
		sys_debug(1, "ERROR: makeasciihost called with NULL arguments");
		return;
	}

	/**
	 * inet_ntoa() works only with IPv4 addresses
	 * inet_ntop() support similar functionality 
	 * and work with both IPv4 and IPv6 addresses
	 */
	//strncpy(host, inet_ntoa(*in), 20);
	char *ipdot = inet_ntop(AF_INET, (void *)in, host, INET_ADDRSTRLEN);
#if 0
	unsigned char *s = (unsigned char *)in;
	int a, b, c, d;
	a = (int)*s++;
	b = (int)*s++;
	c = (int)*s++;
	d = (int)*s;

	snprintf(buf, 20, "%d.%d.%d.%d", a, b, c, d);
#endif

}

static void create_malloced_ascii_host(struct in_addr *in, char *host)
{
	if (!in || !host) {
		sys_debug(1, "ERROR: Dammit, don't send NULL's to create_malloced_ascii_host()");
		return;
	}

	makeasciihost(in, host);
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

static int get_connection(SOCKET sock)
{
	int sockfd;
	socklen_t sin_len;
	//connection_t *con;
	fd_set rfds;
	struct timeval tv;
	int i, maxport = 0;
	struct sockaddr_in *sin = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	if (!sin) {
		sys_debug(1, "WARNING: Weird stuff in get_connection. nmalloc returned NULL sin");
		return -1;
	}

	/* setup sockaddr structure */
	sin_len = sizeof(struct sockaddr_in);
	memset(sin, 0, sin_len);
  
	/* try to accept a connection */
	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);
	if (sock > maxport)
		maxport = sock;
	maxport += 1;

	tv.tv_sec = 0;
	tv.tv_usec = 30000;

	if (select(maxport, &rfds, NULL, NULL, &tv) > 0) {
		if (!(sock_valid(sock) && FD_ISSET(sock, &rfds))) {
			free(sin);
			return -1;
		}
	} else {
		free(sin);
		return -1;
	}

	sockfd = sock_accept(sock, (struct sockaddr *)sin, &sin_len);
	if (sockfd >= 0) {
		//con = create_connection();
		if (!sin) {
			sys_debug(1, "ERROR: NULL sockaddr struct, wft???");
			return NULL;
		}

		/**
		 * host: i.e 10.213.36.178
		 * INET_ADDRSTRLEN is large enough to hold a text string
		 * representing an IPv4 address, and INET6_ADDRSTRLEN is
		 * large enough to hold a text string representing an IPv6 address.
		 */
		char host[INET_ADDRSTRLEN] = {'\0'};
		create_malloced_ascii_host(&(sin->sin_addr), host);
		host[INET_ADDRSTRLEN-1] = '\0';
		sys_debug(2, "DEBUG: Getting new connection on socket %d from host %s",
					sockfd, host);
		return 0;
	}

	if (!is_recoverable(errno))
		sys_debug(1, "WARNING: accept() failed with on socket %d, max: %d, [%d:%s]", sock, maxport, 
			  errno, strerror(errno));
	free(sin);
	return -1;
}

static void *socket_tcp_server_thread(void *arg)
{
	int ret;
	func_enter();
	init_network();
	/* Setup listeners */
	setup_listeners();

	while (running == SERVER_RUNNING) {
		get_connection(server_info.listen_sock);
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


