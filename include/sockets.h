#ifndef _LINUX_SOCKETS_H
#define _LINUX_SOCKETS_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#define SERVER_RUNNING 1
#define SERVER_INITIALIZING 2
#define SERVER_DYING 0
#define OK 1
#define UNKNOWN -1


#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#define SOCKET_BLOCK 0
#define SOCKET_NONBLOCK 1

typedef enum {
	CLIENT_RUNNING = 0,
	CLIENT_DYING = 1
} clistate_t;

typedef enum {
	SOCK_TYPE_TCP = 0,
	SOCK_TYPE_UDP = 1,
	SOCK_TYPE_UNKNOWN = 3
} socktype_t;

typedef int SOCKET;
typedef socklen_t socketlen_t;

typedef struct {
	SOCKET sock;
	int domain;
	int type;
	int protocol;
	int blocking;
	int keepalive;
	int linger;
	int busy;
} socket_t;

typedef struct {
	char *myhostname;	/* NULL unless bind to specific ip */
	char *server_name;	/* Server name */
	char *version;

	int tcp_port;
	int tcp_running;
	SOCKET tcp_listen_sock;	/* Socket to listen to */

	int udp_port;
	int udp_running;
	SOCKET udp_listen_sock;	/* Socket to listen to */

	//pthread_t main_thread;
	//pthread_mutex_t thread_mutex;
} server_info_t;

typedef struct {
	socktype_t type;
	time_t connect_time;
    long read_statistics;

	char *host;
	char *hostname;

	struct sockaddr_in *sin;
	socklen_t sinlen;
	SOCKET sock;
	int running;
} connection_t;

int sock_valid(const SOCKET sockfd);
int sock_set_blocking(SOCKET sockfd, const int block);
SOCKET sock_socket(int domain, int type, int protocol);
int sock_close(SOCKET sockfd);
SOCKET sock_accept(SOCKET s, struct sockaddr *addr, socketlen_t *addrlen);
int sock_write_bytes(SOCKET sockfd, const char *buff, int len);
SOCKET sock_get_server_socket(const int type, const int port);
SOCKET sock_connect_wto(const char *hostname, const int port, const int timeout);
int socket_tcp_get_hostip(int fd,char *buf,int buf_len,const char *prefix);
int sock_tcp_get_hostmac(int fd,char *buf,int buf_len,const char *prefix);
char *sock_get_local_ipaddress();
char *make_host(struct in_addr *in);
void init_network();
void deinit_network();

server_info_t server_info;

#endif

