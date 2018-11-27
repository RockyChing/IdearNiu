#ifndef CONNECT_H
#define CONNECT_H

#include "host.h"       /* for definition of ip_address */

/* Function declarations */

/* Returned by connect_to_host when host name cannot be resolved.  */
enum {
	E_HOST = -100
};
	
int connect_to_host(const char *, int);
int connect_to_ip(const ip_address *, int, const char *);

enum {
	ENDPOINT_LOCAL,
	ENDPOINT_PEER
};
	
bool socket_ip_address(int, ip_address *, int);
int  socket_family(int sock, int endpoint);

bool retryable_socket_connect_error(int);

/* Flags for select_fd's WAIT_FOR argument. */
enum {
	WAIT_FOR_READ = 1,
	WAIT_FOR_WRITE = 2
};
int select_fd (int, double, int);

struct transport_implementation {
	int (*reader)(int, char *, int, void *);
	int (*writer)(int, char *, int, void *);
	int (*poller)(int, double, int, void *);
	int (*peeker)(int, char *, int, void *);
	const char *(*errstr)(int, void *);
	void (*closer)(int, void *);
};

void fd_register_transport(int, struct transport_implementation *, void *);
void *fd_transport_context(int);
int fd_read(int, char *, int, double);
int fd_write(int, char *, int, double);
int fd_peek(int, char *, int, double);
const char *fd_errstr(int);
void fd_close(int);

#endif /* CONNECT_H */

