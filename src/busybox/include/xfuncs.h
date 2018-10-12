#ifndef _IDEANIU_XFUNCS_H
#define _IDEANIU_XFUNCS_H

void xdup2(int from, int to);
void xmove_fd(int from, int to);
void *xzalloc(size_t size);

/* sockets */
ssize_t xsendto(int s, const void *buf, size_t len, const struct sockaddr *to,
				socklen_t tolen);
void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen);
char *xmalloc_sockaddr2dotted_noport(const struct sockaddr *sa);


#endif

