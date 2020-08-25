#ifndef __SSL_UTIL_H
#define __SSL_UTIL_H


#ifndef xmalloc
#define xmalloc malloc
#endif

#ifndef xfree
#define xfree free
#endif

struct ssl_network {
	const char *remote;
	uint16_t port;

	const char *ca_crt;
	uint16_t ca_crt_len;

	uintptr_t ssl_fd;
};


int ssl_connect(struct ssl_network *network);
ssize_t ssl_recv(uintptr_t fd, void *buf, size_t len, uint32_t timeout);
size_t ssl_send(uintptr_t fd, const void *buf, size_t len, uint32_t timeout);
int ssl_disconnect(uintptr_t fd);



#endif /* __SSL_UTIL_H */

