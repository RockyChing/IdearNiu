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

extern connection_t *create_connection();
extern void clean_connection(connection_t *con);

static connection_t *create_tcp_client()
{
	SOCKET sockfd;
	connection_t *con = create_connection();
	char *strIP = get_netdev_ip("ens33");

	do {
		if (!con) {
			sys_debug(1, "ERROR: create_tcp_client");
			break;
		}
	
		if (!strIP) {
			sys_debug(1, "ERROR: get host ip");
			break;
		}

		con->hostip = strIP;
		if (!con->hostip) {
			sys_debug(1, "ERROR: get strdup ip");
			break;
		}

		con->port = 8000;
		if ((sockfd = sock_connect(con->hostip, con->port, 9000)) == INVALID_SOCKET) {
			sys_debug(1, "ERROR: create sockfd");
			break;
		}

		con->type = SOCK_TYPE_TCP;
		con->connect_time = time(NULL);
		con->sock = sockfd;
		con->running = CLIENT_RUNNING;
		return con;
	} while (0);

	return NULL;
}

static inline void destroy_tcp_client(connection_t *client)
{
	if (client) {
		sock_close(client->sock);
		clean_connection(client);
	}
}

static void *socket_tcp_client_thread(void *arg)
{
	fd_set rfds;
	connection_t *client = create_tcp_client();
	struct timeval tv;
	int ret, i, nr;
	char buf[BUFSIZE] = {0};
	if (!client) return NULL;

	while (client->running == CLIENT_RUNNING) {
		bzero(&tv, sizeof(tv));
		FD_ZERO(&rfds);				
		FD_SET(client->sock,&rfds);

		tv.tv_sec = 2;
		tv.tv_usec = 3000;
		ret = select(client->sock + 1, &rfds, NULL, NULL, &tv);
		if (ret > 0) {
			if (FD_ISSET(client->sock, &rfds)) {
				/**
				 * length of message in bytes that received,
				 * 0 if no messages are available and peer has done an orderly shutdown,
				 * or −1 on error
				 */
				nr = recv(client->sock, buf, BUFSIZE, 0);
				if (nr > 0) {
	                client->read_statistics += nr;
					for (i = 0; i < nr; i ++) {
						printf("%c", buf[i]);
					}

					memset(buf, 0, BUFSIZE);
					printf("\n");
	                printf("-------------------------\n"
	                       "data received: %ld\n"
	                       "-------------------------\n\n", client->read_statistics);
				} else if (0 == nr) {
					client->running = CLIENT_DYING;
					sys_debug(1, "peer has done an orderly shutdown");
				} else {
					/* recv() error */
					if (EAGAIN == errno) {
						sys_debug(1, "recv() got EAGAIN");
					} else {
						client->running = CLIENT_DYING;
						sys_debug(1, "recv() errno, %s", strerror(errno));
					}
				}
			}
		} else if (ret == 0) {
			/* time expires, send data */
			char wb[BUFSIZE] = {0};
			int nw = snprintf(wb, BUFSIZE, "data received: %ld\n", client->read_statistics);
            if (sock_write_bytes(client->sock, wb, nw) < 0) {
				client->running = CLIENT_DYING;
				sys_debug(1, "ERROR, sent data to TCP client occurs \"%s\"", strerror(errno));
			}
		} else {
			/* error occurs*/
			sys_debug(1, "ERROR: select() complains: %s", strerror(errno));
			client->running = CLIENT_DYING;
		}
	}

	destroy_tcp_client(client);
	return NULL;
}

void socket_tcp_client_test_entry()
{
	func_enter();
	pthread_t tcp_client_thread;
	printf("start TCP client thread...\n");

	if (thread_create(&tcp_client_thread, socket_tcp_client_thread, NULL)) {
		printf("start TCP server thread failed!\n");
	}

	func_exit();
}

