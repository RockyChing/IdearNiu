#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

#include <sockets.h>
#include <utils.h>
#include <log_util.h>


int sock_valid(const SOCKET sockfd)
{
	return ((int) (sockfd >= 0));
}

/*
 * Set the socket's KEEPALIVE flag
 * this will detech idle connections from blocking
 * forever if the host crashes
 */
static int sock_set_keepalive(SOCKET sockfd, const int keepalive)
{
	int optval = keepalive;
	int res;

	sys_debug(4, "DEBUG: Setting socket %d keepalive to %d", sockfd, keepalive);

	res = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *) &optval, sizeof (int));
	if (res == -1) {
		sys_debug(1, "WARNING: sock_set_keepalive() failed");
		return -1;
	}

	return res;
}


static int sock_set_no_linger(SOCKET sockfd)
{
	int res = 0;
#ifdef SO_LINGER
	struct linger lin = { 0, 0 };
	

	sys_debug(4, "DEBUG: Setting socket %d to no linger", sockfd);

	res = setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (void *) &lin, sizeof (struct linger));
	if (res == -1) {
		sys_debug(1, "WARNING: sock_set_no_linger() failed");
		return -1;
	}
#endif

	return res;
}

int sock_set_blocking(SOCKET sockfd, const int block)
{
	int res;
	
	sys_debug(3, "Setting fd %d to %s", sockfd,
		 (block == SOCKET_BLOCK) ? "blocking" : "nonblocking");
	
	if (!sock_valid(sockfd)) {
		sys_debug(1,
			 "ERROR: sock_set_blocking() called with invalid socket");
		return SOCKET_ERROR;
	} else if ((block < 0) || (block > 1)) {
		sys_debug(1,
			 "ERROR: sock_set_blocking() called with invalid block value");
		return SOCKET_ERROR;
	}

	res = fcntl(sockfd, F_SETFL, (block == SOCKET_BLOCK) ? 0 : O_NONBLOCK);
	if (res == -1) {
		sys_debug(1, "WARNING: sock_set_blocking() on socket %d failed", sockfd);
		return -1;
	}

	return res;
}

static socket_t *sock_create (SOCKET s, int domain, int type, int protocol)
{
	socket_t *is = (socket_t *) malloc (sizeof (socket_t));

	sys_debug(3, "DEBUG: sock_create(): Creating socket %d at %p", s, is);

	is->sock = s;
	is->domain = domain;
	is->type = type;
	is->protocol = protocol;
	is->blocking = UNKNOWN;
	is->keepalive = UNKNOWN;
	is->linger = UNKNOWN;
	is->busy = 0;

	return is;
}

/**
 * socket API's wrapper
 */
SOCKET sock_socket(int domain, int type, int protocol)
{
	SOCKET s = socket(domain, type, protocol);

	sys_debug(4, "DEBUG: sock_socket() creating socket %d", s);

	if (sock_valid(s)) {
		/*
		 * Turn on KEEPALIVE to detect crashed hosts 
		 */
		sock_set_keepalive(s, 1);
#ifdef SO_LINGER
		sock_set_no_linger(s);
#endif
	}

	return s;
}

int sock_close(SOCKET sockfd)
{
	sys_debug(4, "DEBUG: sock_close: Closing socket %d", sockfd);
	return close(sockfd);
}

SOCKET sock_accept(SOCKET s, struct sockaddr *addr, socketlen_t *addrlen)
{
	SOCKET rs = accept(s, addr, addrlen);

	sys_debug(4, "DEBUG: sock_accept() created socket %d", s);

	if (sock_valid(rs)) {
		/*
		 * Turn on KEEPALIVE to detect crashed hosts 
		 */
		sock_set_keepalive(rs, 1);
#ifdef SO_LINGER
		sock_set_no_linger(rs);
#endif
	}

	return rs;
}

#if 0
/* 
 * Write len bytes from buff to the client. Kick him on network errors.
 * Return the number of bytes written and -1 on error.
 * Assert Class: 2
 * Potential problems: Any usage of errno is bad in a threaded application.
 */
int
sock_write_bytes_or_kick(SOCKET sockfd, connection_t * clicon,
			 const char *buff, const int len)
{
	int res, err;

	if (!sock_valid(sockfd)) {
		xa_debug(1,
			 "ERROR: sock_write_bytes_or_kick() called with invalid socket");
		return -1;
	} else if (!clicon) {
		xa_debug(1,
			 "ERROR: sock_write_bytes_or_kick() called with NULL client");
		return -1;
	} else if (!buff) {
		xa_debug(1,
			 "ERROR: sock_write_bytes_or_kick() called with NULL data");
		return -1;
	} else if (len <= 0) {
		xa_debug(1,
			 "ERROR: sock_write_bytes_or_kick() called with invalid length");
		return -1;
	}

	errno = 666;
	
	res = sock_write_bytes(sockfd, buff, len);

	err = errno;

	if (res < 0) {
		xa_debug(4, "DEBUG: sock_write_bytes_or_kick: %d err [%d]",
			 res, err);

		if (!is_recoverable(errno)) {
			kick_connection(clicon, "Client signed off");
			return -1;
		}
	}
	return res;
}
#endif

/*
 * Write len bytes from buf to the socket.
 * Returns the return value from send()
 */
int sock_write_bytes(SOCKET sockfd, const char *buff, int len)
{
	int t;

	if (!buff) {
		sys_debug(1,
			 "ERROR: sock_write_bytes() called with NULL data");
		return -1;
	} else if (len <= 0) {
		sys_debug(1,
			 "ERROR: sock_write_bytes() called with zero or negative len");
		return -1;
	} else if (!sock_valid(sockfd)) {
		sys_debug(1,
			 "ERROR: sock_write_bytes() called with invalid socket");
		return -1;
	}

	for(t = 0 ; len > 0 ; ) {
		int n = send(sockfd, buff + t, len, 0);
		if (n < 0) {
			if (is_recoverable(errno))
				continue;
			sys_debug(1, "ERROR: socket send() error: %s", strerror(errno));
		    return (t == 0) ? n : t;
		}
		t += n;
		len -= n;
	}

	return t;
}

/*
 * Write len bytes from buf to the socket.
 * Returns the return value from sendto()
 * This is used by UDP socket
 */
int sock_sendto(int sockfd, const void *buff, size_t len, int flags,
                   const struct sockaddr *dest_addr, socklen_t addrlen)
{
	int t;

	if (!buff) {
		sys_debug(1,
			 "ERROR: sock_write_bytes() called with NULL data");
		return -1;
	} else if (len <= 0) {
		sys_debug(1,
			 "ERROR: sock_write_bytes() called with zero or negative len");
		return -1;
	} else if (!sock_valid(sockfd)) {
		sys_debug(1,
			 "ERROR: sock_write_bytes() called with invalid socket");
		return -1;
	} else if (!dest_addr) {
		sys_debug(1,
			 "ERROR: sock_write_bytes() called with invalid dest_addr");
		return -1;
	} else if (addrlen != sizeof(struct sockaddr)) {
		sys_debug(1,
			 "ERROR: sock_write_bytes() called with invalid socklen_t");
		return -1;
	}

	for(t = 0 ; len > 0 ; ) {
		int n = sendto(sockfd, buff + t, len, 0, dest_addr, addrlen);
		if (n < 0) {
			if (is_recoverable(errno))
				continue;
			sys_debug(1, "ERROR: socket send() error: %s", strerror(errno));
		    return (t == 0) ? n : t;
		}
		t += n;
		len -= n;
	}

	return t;
}

/*
 * Create a socket for all incoming requests on specified port.
 * Bind it to INADDR_ANY (all available interfaces).
 * Return the socket for bound socket, or INVALID_SOCKET if failed.
 */
SOCKET sock_get_server_socket(int type, const int port)
{
	struct sockaddr_in sin;
	int sin_len, error;
	SOCKET sockfd;

	if (port < 0) {
		sys_debug(1, "ERROR: Invalid port number %d. Cannot listen for requests, this is bad!",
			  port);
		return INVALID_SOCKET;
	}

	sys_debug (2, "DEBUG: Getting socket for port %d", port);
	/*
	 * get socket descriptor 
	 */
	sockfd = sock_socket(AF_INET, type == SOCK_TYPE_TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (sockfd == INVALID_SOCKET)
		return INVALID_SOCKET;

	/*
	 * SO_REUSEADDR - Reuse addresses in bind if *val is nonzero
	 *
	 * Normally, the implementation of TCP will prevent us from binding
	 * the same address until a timeout expires, which is usually on the
	 * order of several minutes. Luckily, the SO_REUSEADDR socket option
	 * allows us to bypass thisrestriction
	 */
	if (type == SOCK_STREAM) {
		int val = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
				(const void *) &val, sizeof (val)) != 0) {
			sys_debug(1, "ERROR: setsockopt() failed to set SO_REUSEADDR flag. (mostly harmless)");
		}
	}

	/*
	 * setup sockaddr structure 
	 */
	sin_len = sizeof (sin);
	memset(&sin, 0, sin_len);
	sin.sin_family = AF_INET;
	/* all available interfaces */
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);

	/*
	 * bind socket to port 
	 */
	error = bind(sockfd, (struct sockaddr *) &sin, sin_len);
	if (error == SOCKET_ERROR) {
		sys_debug(1,
			  "Bind to socket on port %d failed. Shutting down now.", port);
		sock_close(sockfd);
		return INVALID_SOCKET;
	}

	return sockfd;
}

/*
 * Connect to hostname on specified port and return the created socket.
 * Assert Class: 3
 */
SOCKET sock_connect_wto(const char *hostname, const int port,
			const int timeout)
{
	SOCKET sockfd = -1;
#if 0
	struct sockaddr_in sin, server;
	struct hostent *host;
	struct hostent hostinfo;
	char buf[BUFSIZE];
	int error;

	if (!hostname || !hostname[0]) {
		sys_debug(1, "ERROR: sock_connect() called with NULL or empty hostname");
		return INVALID_SOCKET;
	} else if (port <= 0) {
		sys_debug(1, "ERROR: sock_connect() called with invalid port number");
		return INVALID_SOCKET;
	}

	sockfd = sock_socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == INVALID_SOCKET) {
		sock_close(sockfd);
		return INVALID_SOCKET;
	}

	if (info.myhostname != NULL) {
		struct sockaddr_in localsin;
		memset(&localsin, 0, sizeof (struct sockaddr_in));

		xa_debug(2, "DEBUG: Trying to bind to %s", info.myhostname);

		localsin.sin_addr = localaddr;
		localsin.sin_family = AF_INET;
		localsin.sin_port = 0;

		if (bind
		    (sockfd, (struct sockaddr *) &localsin,
		     sizeof (localsin)) == SOCKET_ERROR) {
			xa_debug(2, "DEBUG: Unable to bind", info.myhostname);
			write_log(LOG_DEFAULT,
				  "ERROR: Bind to local address %s failed",
				  info.myhostname);
			sock_close(sockfd);
			return INVALID_SOCKET;
		}
	}

	memset(&sin, 0, sizeof (sin));
	memset(&server, 0, sizeof (struct sockaddr_in));

	if (isdigit((int) hostname[0])
	    && isdigit((int) hostname[ice_strlen(hostname) - 1])) {
		if (inet_aton(hostname, (struct in_addr *) &sin.sin_addr) ==
		    0) {
			write_log(LOG_DEFAULT, "ERROR: Invalid ip number %s",
				  hostname);
			sock_close(sockfd);
			return INVALID_SOCKET;
		}
		memcpy(&server.sin_addr, &sin.sin_addr, sizeof (sin));
	} else {
		host =
			ice_gethostbyname(hostname, &hostinfo, buf, BUFSIZE,
					  &error);
		if (host == NULL) {
			xa_debug(1, "DEBUG: gethostbyname %s failed",
				 hostname);
			sock_close(sockfd);
			ice_clean_hostent();
			return INVALID_SOCKET;
		}
		memcpy(&server.sin_addr, host->h_addr, host->h_length);
		ice_clean_hostent();
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	{
		char buf[50];

		makeasciihost(&server.sin_addr, buf);
		xa_debug(1, "Trying to connect to %s:%d", buf, port);
	}

	if (timeout > 0) {
		fd_set wfds;
		struct timeval tv;
		int retval;
		int val;
		mysocklen_t valsize = sizeof (int);

		xa_debug(3,
			 "DEBUG: sock_connect(): doing a connection w/ timeout");

		FD_ZERO(&wfds);
		FD_SET(sockfd, &wfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		sock_set_blocking(sockfd, SOCK_NONBLOCK);
		retval = connect(sockfd, (struct sockaddr *) &server, sizeof (server));
		if (retval == 0) {
			xa_debug(3, "DEBUG: sock_connect(): non blocking connect returned 0!");
			sock_set_blocking(sockfd, SOCK_BLOCK);
			return sockfd;
		} else {
#ifdef _WIN32
			if (WSAGetLastError() == WSAEINPROGRESS) {
#else
			if (!is_recoverable(errno)) {
#endif
				xa_debug(3, "DEBUG: sock_connect(): connect didn't return EINPROGRESS!, was: %d", errno);
				sock_close(sockfd);
				return SOCKET_ERROR;
			}
		}

		if (select(sockfd + 1, NULL, &wfds, NULL, &tv)) {
			retval = getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
					    (void *) &val,
					    (mysocklen_t *) & valsize);
			if ((retval == 0) && (val == 0)) {
				sock_set_blocking(sockfd, SOCK_BLOCK);
				return sockfd;
			} else {
				xa_debug(3,
					 "DEBUG: sock_connect(): getsockopt returned %i, val = %i, valsize = %i, errno = %i!",
					 retval, val, valsize, errno);
				sock_close(sockfd);
				return SOCKET_ERROR;
			}
		} else {
			xa_debug(3,
				 "DEBUG: sock_connect(): select returned 0");
			sock_close(sockfd);
			return SOCKET_ERROR;
		}
	} else {
		if (connect
		    (sockfd, (struct sockaddr *) &server,
		     sizeof (server)) == 0) {
			return sockfd;
		} else {
			sock_close(sockfd);
			return SOCKET_ERROR;
		}
	}
#endif
	return sockfd;
}

/**
 * get host ip address from file descriptor
 * @fd: host socket file descriptor
 * @buf: buffer to save address string
 * 0: on success; -1: on failure
 */
int socket_tcp_get_hostip(int fd,char *buf,int buf_len,const char *prefix)
{
	if(fd <= 2 || buf == NULL)
		return -1;

	int i=0,ret=0;
    struct ifreq ifr_info;

	for(i = 0; i < 10; i ++) {
		memset(&ifr_info,0,sizeof(struct ifreq));     
		snprintf(ifr_info.ifr_name,IFNAMSIZ,"%s%d",prefix,i);
		ret = ioctl(fd,SIOCGIFADDR,&ifr_info);
		if(ret == 0)
			break;
	}

	if(ret < 0){
		perror("SOCKET_tcpGetHostIp failed.\n");
		return -1;
	}

	snprintf(buf,buf_len,"%s",inet_ntoa(((struct sockaddr_in *)&ifr_info.ifr_addr)->sin_addr));
	//printf("host_ip: %s\n",buf);
	
	return 0;
}


/**
 * get host MAC address from file descriptor
 * @fd: host socket file descriptor
 * @buf: buffer to save address string
 * 0: on success; -1: on failure
 */
int sock_tcp_get_hostmac(int fd,char *buf,int buf_len,const char *prefix)
{
	if(fd <= 2 || buf == NULL)
		return -1;

	int i = 0, ret = 0;
    struct ifreq ifr_info;

	for(i = 0; i < 10; i ++) {
		memset(&ifr_info,0,sizeof(struct ifreq));     
		snprintf(ifr_info.ifr_name,IFNAMSIZ,"%s%d",prefix,i);
		ret = ioctl(fd,SIOCGIFHWADDR,&ifr_info);
		if(ret == 0)
			break;
	}

	if(ret < 0){
		printf("SOCKET_tcpGetHostMacAddress failed.\n");
		return -1;
	}

	snprintf(buf,buf_len,"%02x:%02x:%02x:%02x:%02x:%02x",
			(unsigned char)ifr_info.ifr_hwaddr.sa_data[0],
			(unsigned char)ifr_info.ifr_hwaddr.sa_data[1],
			(unsigned char)ifr_info.ifr_hwaddr.sa_data[2],
			(unsigned char)ifr_info.ifr_hwaddr.sa_data[3],
			(unsigned char)ifr_info.ifr_hwaddr.sa_data[4],
			(unsigned char)ifr_info.ifr_hwaddr.sa_data[5]);
	//printf("host_mac: %s\n",buf);

	return 0;
}

char *sock_get_local_ipaddress()
{
#if 0
	SOCKET sockfd;
	mysocklen_t sinlen = sizeof (struct sockaddr_in);
	struct sockaddr_in sin, cliaddr;

	sockfd = sock_socket(AF_INET, SOCK_DGRAM, 0);

	memset(&sin, 0, sizeof (sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(info.port[0]);

	if (!inet_aton("130.240.1.1", (struct in_addr *) &sin.sin_addr)) {
		xa_debug(1, "DEBUG: inet_aton() failed with [%d]", errno);
		return nstrdup("dynamic");
	}

	if (connect(sockfd, (struct sockaddr *) &sin, sizeof (sin)) == -1) {
		xa_debug(1, "DEBUG: connect() failed with [%s]", errno);
		return nstrdup("dynamic");
	}


	if (getsockname(sockfd, (struct sockaddr *) &cliaddr, &sinlen) == 0) {
		close(sockfd);
		if (inet_ntoa(cliaddr.sin_addr))
			return nstrdup(inet_ntoa(cliaddr.sin_addr));
		else
			return nstrdup("dynamic");
	} else {
		xa_debug(1, "DEBUG: getsockname() failed with [%d]", errno);
		close(sockfd);
		return nstrdup("dynamic");
	}
#endif
	return NULL;
}

static char *make_host_from_ip(const struct in_addr *in, char *host)
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

/**
 * make a host name like "192.168.1.88"
 * caller need to release memory
 */
char *make_host(struct in_addr *in)
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
	return make_host_from_ip(in, buf);
}

void init_network()
{
	/**
	 * Create a tcp socket for DNS queries that stays open
	 *
	 * The sethostent function will open the file
	 * or rewind it if it is already open. When the stayopen argument is set to a nonzero value,
	 * the file remains open after calling gethostent. The endhostent function can be
	 * used to close the file.
	 */
	//sethostent(1);
	server_info.myhostname = NULL;
	server_info.server_name = NULL;
	server_info.version = NULL;

	server_info.tcp_port = 8000;
	server_info.tcp_listen_sock = -1;
	server_info.tcp_running = SERVER_INITIALIZING;

	server_info.udp_port = 8800;
	server_info.udp_listen_sock = -1;
	server_info.udp_running = SERVER_INITIALIZING;
}

void deinit_network()
{
	if (server_info.myhostname) {
		free(server_info.myhostname);
	} else if (server_info.server_name) {
		free(server_info.server_name);
	} else if (server_info.version) {
		free(server_info.version);
	} else {
	}

	init_network();
}

