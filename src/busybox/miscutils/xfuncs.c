/*
 * Port from Busybox version: 1.22.1
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/if.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>

#include "log_util.h"
#include "libbb.h"


void xdup2(int from, int to)
{
	if (dup2(from, to) != to)
		error("can't duplicate file descriptor");
}

// Functions from other files
// "Renumber" opened fd
void xmove_fd(int from, int to)
{
	if (from == to)
		return;
	xdup2(from, to);
	close(from);
}

// Die if we can't copy a string to freshly allocated memory.
char *xstrdup(const char *s)
{
	char *t;
	if (s == NULL)
		return NULL;

	t = strdup(s);
	if (t == NULL)
		error("strdup");

	return t;
}


// Die if we can't allocate size bytes of memory.
void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL && size != 0)
		error("error of malloc");
	return ptr;
}

// Die if we can't allocate and zero size bytes of memory.
void *xzalloc(size_t size)
{
	void *ptr = xmalloc(size);
	if (!ptr) {
		error("error of xmalloc");
	}

	memset(ptr, 0, size);
	return ptr;
}


/* Die with an error message if sendto failed.
 * Return bytes sent otherwise  */
ssize_t xsendto(int s, const void *buf, size_t len, const struct sockaddr *to,
				socklen_t tolen)
{
	ssize_t ret = sendto(s, buf, len, 0, to, tolen);
	if (ret < 0) {
		if (ENABLE_FEATURE_CLEAN_UP)
			close(s);
		error("sendto");
	}
	return ret;
}

// Die with an error message if we can't bind a socket to an address.
void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen)
{
	if (bind(sockfd, my_addr, addrlen)) {
		error("bind");
		abort();
	}
}

/* We hijack this constant to mean something else */
/* It doesn't hurt because we will add this bit anyway */
char *sockaddr2str(const struct sockaddr *sa, int flags)
{
	char host[128];
	char serv[16];
	int rc;
	socklen_t salen;

	salen = LSA_SIZEOF_SA;
	rc = getnameinfo(sa, salen,
			host, sizeof(host),
	/* can do ((flags & IGNORE_PORT) ? NULL : serv) but why bother? */
			serv, sizeof(serv),
			/* do not resolve port# into service _name_ */
			flags | NI_NUMERICSERV
	);

	if (rc) {
		return NULL;
	}
	return xstrdup(host);
}

char *xmalloc_sockaddr2dotted_noport(const struct sockaddr *sa)
{
	return sockaddr2str(sa, NI_NUMERICHOST);
}

