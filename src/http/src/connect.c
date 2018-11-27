#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <sys/socket.h>
#include <sys/select.h>

#include <netdb.h>
#include <netinet/in.h>
#ifndef __BEOS__
#include <arpa/inet.h>
#endif

#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include "utils.h"
#include "host.h"
#include "connect.h"

#include <stdint.h>

/* Define sockaddr_storage where unavailable (presumably on IPv4-only
   hosts).  */

#if 1
# ifndef HAVE_STRUCT_SOCKADDR_STORAGE
#  define sockaddr_storage sockaddr_in
# endif
#endif

/* Fill SA as per the data in IP and PORT.  SA shoult point to struct
   sockaddr_storage if ENABLE_IPV61 is defined, to struct sockaddr_in
   otherwise.  */
static void sockaddr_set_data(struct sockaddr *sa, const ip_address *ip, int port)
{
	switch (ip->family) {
	case AF_INET: {
		struct sockaddr_in *sin = (struct sockaddr_in *)sa;
		xzero (*sin);
		sin->sin_family = AF_INET;
		sin->sin_port = htons (port);
		sin->sin_addr = ip->data.d4;
		break;
	}
	default:
		abort();
	}
}

/* Get the data of SA, specifically the IP address and the port.  If
   you're not interested in one or the other information, pass NULL as
   the pointer.  */
static void sockaddr_get_data(const struct sockaddr *sa, ip_address *ip, int *port)
{
	switch (sa->sa_family) {
	case AF_INET: {
		struct sockaddr_in *sin = (struct sockaddr_in *)sa;
		if (ip) {
			ip->family = AF_INET;
			ip->data.d4 = sin->sin_addr;
		}
		if (port)
			*port = ntohs(sin->sin_port);
		break;
	}
	default:
		abort();
	}
}

struct cwt_context {
	int fd;
	const struct sockaddr *addr;
	socklen_t addrlen;
	int result;
};

static void connect_with_timeout_callback (void *arg)
{
	struct cwt_context *ctx = (struct cwt_context *)arg;
	ctx->result = connect (ctx->fd, ctx->addr, ctx->addrlen);
}

/* Like connect, but specifies a timeout.  If connecting takes longer
   than TIMEOUT seconds, -1 is returned and errno is set to
   ETIMEDOUT.  */
static int
connect_with_timeout (int fd, const struct sockaddr *addr, socklen_t addrlen, double timeout)
{
	struct cwt_context ctx;
	ctx.fd = fd;
	ctx.addr = addr;
	ctx.addrlen = addrlen;

	ctx.result = connect(ctx.fd, ctx.addr, ctx.addrlen);

	if (ctx.result == -1 && errno == EINTR)
	errno = ETIMEDOUT;
	return ctx.result;
}

/* Connect via TCP to the specified address and port.

   If PRINT is non-NULL, it is the host name to print that we're
   connecting to.  */

int connect_to_ip(const ip_address *ip, int port, const char *print)
{
	struct sockaddr_storage ss;
	struct sockaddr *sa = (struct sockaddr *)&ss;
	int sock;

	/* If PRINT is non-NULL, print the "Connecting to..." line, with
	 	PRINT being the host name we're connecting to.  */
	if (print) {
      	const char *txt_addr = print_address(ip);
      	if (0 != strcmp (print, txt_addr)) {
          	char *str = NULL;

          	logprintf (LOG_VERBOSE, "Connecting to %s|%s|:%d... ", str ? str : escnonprint_uri(print), txt_addr, port);
          	xfree (str);
		} else {
           if (ip->family == AF_INET)
               logprintf (LOG_VERBOSE, "Connecting to %s:%d... ", txt_addr, port);
        }
    }

	/* Store the sockaddr info to SA.  */
	sockaddr_set_data (sa, ip, port);

	/* Create the socket of the family appropriate for the address.  */
	sock = socket(sa->sa_family, SOCK_STREAM, 0);
	if (sock < 0)
		goto err;

	/* For very small rate limits, set the buffer size (and hence,
		hopefully, the kernel's TCP window size) to the per-second limit.
		That way we should never have to sleep for more than 1s between
		network reads.  */
	if (opt.limit_rate && opt.limit_rate < 8192) {
		int bufsize = opt.limit_rate;
		if (bufsize < 512)
			bufsize = 512;          /* avoid pathologically small values */
#ifdef SO_RCVBUF
		if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void *) &bufsize, (socklen_t) sizeof (bufsize)))
			logprintf (LOG_NOTQUIET, "setsockopt SO_RCVBUF failed: %s\n", strerror (errno));
#endif
		/* When we add limit_rate support for writing, which is useful
			for POST, we should also set SO_SNDBUF here.  */
		}

		/* Connect the socket to the remote endpoint.  */
		if (connect_with_timeout(sock, sa, sizeof(struct sockaddr_in), opt.connect_timeout) < 0)
    		goto err;

		/* Success. */
		assert (sock >= 0);
		if (print)
			logprintf (LOG_VERBOSE, "connected.\n");
		logprintf(LOG_VERBOSE, "Created socket %d.\n", sock);
		return sock;

err: {
	/* Protect errno from possible modifications by close and logprintf.  */
	int save_errno = errno;
	if (sock >= 0) {
		fd_close (sock);
	}
	if (print)
		logprintf (LOG_NOTQUIET, "failed: %s.\n", strerror (errno));
	errno = save_errno;
	} return -1;
}

/* Connect via TCP to a remote host on the specified port.

   HOST is resolved as an Internet host name.  If HOST resolves to
   more than one IP address, they are tried in the order returned by
   DNS until connecting to one of them succeeds.  */

int connect_to_host(const char *host, int port)
{
	int i, start, end;
	int sock;

	func_enter();
	struct address_list *al = lookup_host(host, 0);

retry:
	if (!al) {
		logprintf (LOG_NOTQUIET,"%s: unable to resolve host address %s\n", exec_name, host);
		return E_HOST;
	}

	address_list_get_bounds (al, &start, &end);
	for (i = start; i < end; i++) {
		const ip_address *ip = address_list_address_at(al, i);
		sock = connect_to_ip(ip, port, host);
		if (sock >= 0) {
			/* Success. */
			address_list_set_connected(al);
			address_list_release(al);
			func_exit();
			return sock;
		}

		/* The attempt to connect has failed.  Continue with the loop
			and try next address. */

		address_list_set_faulty (al, i);
	}

	/* Failed to connect to any of the addresses in AL. */
	if (address_list_connected_p(al)) {
		/* We connected to AL before, but cannot do so now.  That might
			indicate that our DNS cache entry for HOST has expired.  */
		address_list_release(al);
		al = lookup_host(host, LH_REFRESH);
		goto retry;
	}
	address_list_release (al);
	func_exit();
	return -1;
}

/* Get the IP address associated with the connection on FD and store
   it to IP.  Return true on success, false otherwise.

   If ENDPOINT is ENDPOINT_LOCAL, it returns the address of the local
   (client) side of the socket.  Else if ENDPOINT is ENDPOINT_PEER, it
   returns the address of the remote (peer's) side of the socket.  */
bool socket_ip_address(int sock, ip_address *ip, int endpoint)
{
	struct sockaddr_storage storage;
	struct sockaddr *sockaddr = (struct sockaddr *) &storage;
	socklen_t addrlen = sizeof (storage);
	int ret;

	memset(sockaddr, 0, addrlen);
	if (endpoint == ENDPOINT_LOCAL)
		ret = getsockname(sock, sockaddr, &addrlen);
	else if (endpoint == ENDPOINT_PEER)
		ret = getpeername(sock, sockaddr, &addrlen);
	else
		abort ();
	if (ret < 0)
		return false;

	memset(ip, 0, sizeof(ip_address));
	ip->family = sockaddr->sa_family;
	switch (sockaddr->sa_family) {
	case AF_INET: {
		struct sockaddr_in *sa = (struct sockaddr_in *)&storage;
		ip->data.d4 = sa->sin_addr;
		printf("conaddr is: %s\n", print_address(ip));
		return true;
	}
	default:
		abort();
	}
}

/* Get the socket family of connection on FD and store
   Return family type on success, -1 otherwise.

   If ENDPOINT is ENDPOINT_LOCAL, it returns the sock family of the local
   (client) side of the socket.  Else if ENDPOINT is ENDPOINT_PEER, it
   returns the sock family of the remote (peer's) side of the socket.  */
int socket_family (int sock, int endpoint)
{
	struct sockaddr_storage storage;
	struct sockaddr *sockaddr = (struct sockaddr *) &storage;
	socklen_t addrlen = sizeof (storage);
	int ret;

	memset (sockaddr, 0, addrlen);

	if (endpoint == ENDPOINT_LOCAL)
		ret = getsockname(sock, sockaddr, &addrlen);
	else if (endpoint == ENDPOINT_PEER)
		ret = getpeername(sock, sockaddr, &addrlen);
	else
		abort ();

	if (ret < 0)
		return -1;

	return sockaddr->sa_family;
}

/* Return true if the error from the connect code can be considered
   retryable.  Wget normally retries after errors, but the exception
   are the "unsupported protocol" type errors (possible on IPv4/IPv6
   dual family systems) and "connection refused".  */
bool retryable_socket_connect_error (int err)
{
	return false;
}

/* Wait for a single descriptor to become available, timing out after
   MAXTIME seconds.  Returns 1 if FD is available, 0 for timeout and
   -1 for error.  The argument WAIT_FOR can be a combination of
   WAIT_FOR_READ and WAIT_FOR_WRITE.

   This is a mere convenience wrapper around the select call, and
   should be taken as such (for example, it doesn't implement Wget's
   0-timeout-means-no-timeout semantics.)  */

int select_fd (int fd, double maxtime, int wait_for)
{
  fd_set fdset;
  fd_set *rd = NULL, *wr = NULL;
  struct timeval tmout;
  int result;

  if (fd >= FD_SETSIZE)
    {
      logprintf (LOG_NOTQUIET, ("Too many fds open.  Cannot use select on a fd >= %d\n"), FD_SETSIZE);
      exit (0);
    }
  FD_ZERO (&fdset);
  FD_SET (fd, &fdset);
  if (wait_for & WAIT_FOR_READ)
    rd = &fdset;
  if (wait_for & WAIT_FOR_WRITE)
    wr = &fdset;

  tmout.tv_sec = (long) maxtime;
  tmout.tv_usec = 1000000 * (maxtime - (long) maxtime);

  do
  {
    result = select (fd + 1, rd, wr, NULL, &tmout);
  }
  while (result < 0 && errno == EINTR);

  return result;
}

/* Basic socket operations, mostly EINTR wrappers.  */
static int sock_read (int fd, char *buf, int bufsize)
{
	int res;
	do {
		res = read (fd, buf, bufsize);
	} while (res == -1 && errno == EINTR);
	return res;
}

static int sock_write (int fd, char *buf, int bufsize)
{
	int res;
	do {
		res = write (fd, buf, bufsize);
	} while (res == -1 && errno == EINTR);

	return res;
}

static int sock_poll (int fd, double timeout, int wait_for)
{
	return select_fd (fd, timeout, wait_for);
}

static int sock_peek(int fd, char *buf, int bufsize)
{
	int res;
	do {
		res = recv(fd, buf, bufsize, MSG_PEEK);
	} while (res == -1 && errno == EINTR);
	return res;
}

static void sock_close (int fd)
{
	close(fd);
	DEBUGP (("Closed fd %d\n", fd));
}
#undef read
#undef write
#undef close

/* Reading and writing from the network.  We build around the socket
   (file descriptor) API, but support "extended" operations for things
   that are not mere file descriptors under the hood, such as SSL
   sockets.

   That way the user code can call fd_read(fd, ...) and we'll run read
   or SSL_read or whatever is necessary.  */
struct transport_info {
	struct transport_implementation *imp;
	void *ctx;
};

struct transport_info *ssl_transport = NULL;


/* Register the transport layer operations that will be used when
   reading, writing, and polling FD.

   This should be used for transport layers like SSL that piggyback on
   sockets.  FD should otherwise be a real socket, on which you can
   call getpeername, etc.  */

void fd_register_transport(int fd, struct transport_implementation *imp, void *ctx)
{
	struct transport_info *info;

	/* The file descriptor must be non-negative to be registered.
	   Negative values are ignored by fd_close(), and -1 cannot be used as
	   hash key.  */
	assert (fd >= 0);

	info = xnew(struct transport_info);
	info->imp = imp;
	info->ctx = ctx;
	if (!ssl_transport) {
		log_debug("ssl_transport init");
		ssl_transport = info;
	}
}

/* Return context of the transport registered with
   fd_register_transport.  This assumes fd_register_transport was
   previously called on FD.  */

void *fd_transport_context(int fd)
{
	struct transport_info *info = ssl_transport;
	return info ? info->ctx : NULL;
}

/* When fd_read/fd_write are called multiple times in a loop, they should
   remember the INFO pointer instead of fetching it every time.  It is
   not enough to compare FD to LAST_FD because FD might have been
   closed and reopened.  modified_tick ensures that changes to
   transport_map will not be unnoticed.

   This is a macro because we want the static storage variables to be
   per-function.  */

#define LAZY_RETRIEVE_INFO(info) do {	\
		info = ssl_transport;			\
	} while (0)

static bool poll_internal(int fd, struct transport_info *info, int wf, double timeout)
{
	if (timeout == -1)
		timeout = opt.read_timeout;
	if (timeout) {
		int test;
		if (info && info->imp->poller)
			test = info->imp->poller(fd, timeout, wf, info->ctx);
		else
			test = sock_poll(fd, timeout, wf);
		if (test == 0)
			errno = ETIMEDOUT;
		if (test <= 0)
			return false;
	}
	return true;
}

/* Read no more than BUFSIZE bytes of data from FD, storing them to
   BUF.  If TIMEOUT is non-zero, the operation aborts if no data is
   received after that many seconds.  If TIMEOUT is -1, the value of
   opt.timeout is used for TIMEOUT.  */
int fd_read(int fd, char *buf, int bufsize, double timeout)
{
	struct transport_info *info;
	LAZY_RETRIEVE_INFO(info);
	if (!poll_internal(fd, info, WAIT_FOR_READ, timeout))
		return -1;

	if (info && info->imp->reader)
		return info->imp->reader (fd, buf, bufsize, info->ctx);
	else
		return sock_read(fd, buf, bufsize);
}

/* Like fd_read, except it provides a "preview" of the data that will
   be read by subsequent calls to fd_read.  Specifically, it copies no
   more than BUFSIZE bytes of the currently available data to BUF and
   returns the number of bytes copied.  Return values and timeout
   semantics are the same as those of fd_read.

   CAVEAT: Do not assume that the first subsequent call to fd_read
   will retrieve the same amount of data.  Reading can return more or
   less data, depending on the TCP implementation and other
   circumstances.  However, barring an error, it can be expected that
   all the peeked data will eventually be read by fd_read.  */
int fd_peek(int fd, char *buf, int bufsize, double timeout)
{
	struct transport_info *info;
	LAZY_RETRIEVE_INFO(info);
	if (!poll_internal(fd, info, WAIT_FOR_READ, timeout))
		return -1;
	if (info && info->imp->peeker)
		return info->imp->peeker (fd, buf, bufsize, info->ctx);
	else
		return sock_peek(fd, buf, bufsize);
}

/* Write the entire contents of BUF to FD.  If TIMEOUT is non-zero,
   the operation aborts if no data is received after that many
   seconds.  If TIMEOUT is -1, the value of opt.timeout is used for
   TIMEOUT.  */
int fd_write(int fd, char *buf, int bufsize, double timeout)
{
	int res;
	struct transport_info *info;
	LAZY_RETRIEVE_INFO (info);

	/* `write' may write less than LEN bytes, thus the loop keeps trying
		it until all was written, or an error occurred.  */
	res = 0;
	while (bufsize > 0) {
		if (!poll_internal (fd, info, WAIT_FOR_WRITE, timeout))
			return -1;
		if (info && info->imp->writer)
			res = info->imp->writer (fd, buf, bufsize, info->ctx);
		else
			res = sock_write (fd, buf, bufsize);
		if (res <= 0)
			break;
		buf += res;
		bufsize -= res;
	}
	return res;
}

/* Report the most recent error(s) on FD.  This should only be called
   after fd_* functions, such as fd_read and fd_write, and only if
   they return a negative result.  For errors coming from other calls
   such as setsockopt or fopen, strerror should continue to be
   used.

   If the transport doesn't support error messages or doesn't supply
   one, strerror(errno) is returned.  The returned error message
   should not be used after fd_close has been called.  */

const char *fd_errstr(int fd)
{
	/* Don't bother with LAZY_RETRIEVE_INFO, as this will only be called
	 in case of error, never in a tight loop.  */
	struct transport_info *info = ssl_transport;

	if (info && info->imp->errstr) {
	  	const char *err = info->imp->errstr (fd, info->ctx);
	  	if (err)
	    	return err;
	  	/* else, fall through and print the system error. */
	}

	return strerror (errno);
}

/* Close the file descriptor FD.  */

void fd_close(int fd)
{
	struct transport_info *info;
	if (fd < 0)
		return;

	/* Don't use LAZY_RETRIEVE_INFO because fd_close() is only called once
	per socket, so that particular optimization wouldn't work.  */
	info = ssl_transport;

	if (info && info->imp->closer)
		info->imp->closer (fd, info->ctx);
	else
		sock_close (fd);

	if (info) {
		xfree(info);
	}
}
