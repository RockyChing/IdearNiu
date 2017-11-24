#ifndef _LINUX_SOCKETS_H
#define _LINUX_SOCKETS_H
#include <sys/socket.h>

#define SERVER_RUNNING 1
#define SERVER_INITIALIZING 2
#define SERVER_DYING 0
#define OK 1
#define UNKNOWN -1


#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#define SOCKET_BLOCK 0
#define SOCKET_NONBLOCK 1

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


int sock_valid(const SOCKET sockfd);
int sock_set_blocking(SOCKET sockfd, const int block);
SOCKET sock_socket(int domain, int type, int protocol);
int sock_close(SOCKET sockfd);
SOCKET sock_accept(SOCKET s, struct sockaddr *addr, socketlen_t *addrlen);
int sock_write_bytes(SOCKET sockfd, const char *buff, int len);
int sock_write_string(SOCKET sockfd, const char *buff);
int sock_write(SOCKET sockfd, const char *fmt, ...);
int sock_read_lines_np(SOCKET sockfd, char *buff, const int len);
int sock_read_lines(SOCKET sockfd, char *buff, const int len);
SOCKET sock_get_server_socket(const int port);
SOCKET sock_connect_wto(const char *hostname, const int port, const int timeout);
int socket_tcp_get_hostip(int fd,char *buf,int buf_len,const char *prefix);
int sock_tcp_get_hostmac(int fd,char *buf,int buf_len,const char *prefix);
char *sock_get_local_ipaddress();
void init_network();


#endif

