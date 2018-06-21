#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <linux/sctp.h>

/**
Generic Socket Options
  - this means they are handled by the protocol-independent code within the
    kernel, not by one particular protocol module such as IPv4
 <SO_BROADCAST>
  - This option enables or disables the ability of the process to send broadcast messages.
    Broadcasting is supported for only datagram sockets and only on networks that
    support the concept of a broadcast message (e.g., Ethernet, token ring, etc). You
    cannot broadcast on a point-to-point link or any connection-based transport protocol
    such as SCTP or TCP.
    Since an application must set this socket option before sending a broadcast datagram,
    it prevents a process from sending a broadcast when the application was never
    designed to broadcast. For example, a UDP application might take the destination IP
    address as a command-line argument, but the application never intended for a user
    to type in a broadcast address. Rather than forcing the application to try to determine
    if a given address is a broadcast address or not, the test is in the kernel: If the
    destination address is a broadcast address and this socket option is not set, EACCES is
    returned.

 <SO_DEBUG>
  - This option is supported only by TCP. When enabled for a TCP socket, the kernel
    keeps track of detailed information about all the packets sent or received by TCP for
    the socket. These are kept in a circular buffer within the kernel.

 <SO_DONTROUTE>
  - This option specifies that outgoing packets are to bypass the normal routing
    mechanisms of the underlying protocol. For example, with IPv4, the packet is directed
    to the appropriate local interface, as specified by the network and subnet portions of
    the destination address. If the local interface cannot be determined from the
    destination address (e.g., the destination is not on the other end of a point-to-point
    link, or is not on a shared network), ENETUNREACH is returned.
	This option is often used by routing daemons (e.g., routed and gated) to bypass the
	routing table and force a packet to be sent out a particular interface.

 <SO_ERROR>
  - When an error occurs on a socket, the protocol module in a Berkeley-derived kernel
	sets a variable named so_error for that socket to one of the standard Unix Exxx
	values. This is called the pending error for the socket. The process can be immediately
	notified of the error in one of two ways:
		1. If the process is blocked in a call to select on the socket, for
		   either readability or writability, select returns with either or both conditions set.
		2. If the process is using signal-driven I/O, the SIGIO signal is
		   generated for either the process or the process group.
	The process can then obtain the value of so_error by fetching the SO_ERROR socket
	option. The integer value returned by getsockopt is the pending error for the socket.
	The value of so_error is then reset to 0 by the kernel.
	If so_error is nonzero when the process calls read and there is no data to return,
	read returns –1 with errno set to the value of so_error. The value
	of so_error is then reset to 0. If there is data queued for the socket, that data is
	returned by read instead of the error condition. If so_error is nonzero when the
	process calls write, –1 is returned with errno set to the value of so_error and so_error is reset to 0.
	This is the first socket option that we have encountered that can be fetched but cannot be set.

 <SO_KEEPALIVE>
  - When the keep-alive option is set for a TCP socket and no data has been exchanged
	across the socket in either direction for two hours, TCP automatically sends a
	keep-alive probe to the peer. This probe is a TCP segment to which the peer must
	respond. One of three scenarios results:
	1. The peer responds with the expected ACK. The application is not notified
	   (since everything is okay). TCP will send another probe following another two
	   hours of inactivity.
	2. The peer responds with an RST, which tells the local TCP that the peer host has
       crashed and rebooted. The socket's pending error is set to ECONNRESET and
	   the socket is closed.
	3. There is no response from the peer to the keep-alive probe. Berkeley-derived
	   TCPs send 8 additional probes, 75 seconds apart, trying to elicit a response.
       TCP will give up if there is no response within 11 minutes and 15 seconds after
       sending the first probe.
	   If there is no response at all to TCP's keep-alive probes, the socket's pending
	   error is set to ETIMEDOUT and the socket is closed. But if the socket receives an
	   ICMP error in response to one of the keep-alive probes, the corresponding
	   error is returned instead (and the socket is still
	   closed). A common ICMP error in this scenario is "host unreachable,"
	   indicating that the peer host is unreachable, in which case, the pending error
	   is set to EHOSTUNREACH. This can occur either because of a network failure or
	   because the remote host has crashed and the last-hop router has detected the crash.
	The purpose of this option is to detect if the peer host crashes or becomes
	unreachable (e.g., dial-up modem connection drops, power fails, etc.). If the peer
	process crashes, its TCP will send a FIN across the connection, which we can easily
	detect with select. Also realize that
	if there is no response to any of the keep-alive probes (scenario 3), we are not
	guaranteed that the peer host has crashed, and TCP may well terminate a valid
	connection. It could be that some intermediate router has crashed for 15 minutes,
	and that period of time just happens to completely overlap our host's 11-minute and
	15-second keep-alive probe period. In fact, this function might more properly be
	called "make-dead" rather than "keep-alive" since it can terminate live connections.

	This option is normally used by servers, although clients can also use the option.
	Servers use the option because they spend most of their time blocked waiting for
	input across the TCP connection, that is, waiting for a client request. But if the client
	host's connection drops, is powered off, or crashes, the server process will never
	know about it, and the server will continually wait for input that can never arrive. This
	is called a half-open connection. The keep-alive option will detect these half-open
	connections and terminate them.

 <SO_LINGER>
  - This option specifies how the @close function operates for a connection-oriented
	protocol (e.g., for TCP and SCTP, but not for UDP). By default, close returns
	immediately, but if there is any data still remaining in the socket send buffer, the
	system will try to deliver the data to the peer.
	The SO_LINGER socket option lets us change this default. This option requires the
	following structure to be passed between the user process and the kernel. It is
	defined by including <sys/socket.h>.
 		struct linger {
		 	int l_onoff; // 0=off, nonzero=on
		 	int l_linger; // linger time, POSIX specifies units as seconds
		};
	Calling setsockopt leads to one of the following three scenarios, depending on the
	values of the two structure members:
		1. If l_onoff = 0, the option is turned off. The value of l_linger is ignored and
		   the previously discussed TCP default applies: close returns immediately.
		2. If l_onoff != 0 and l_linger = 0, TCP aborts the connection when
		   it is closed. That is, TCP discards any data still
		   remaining in the socket send buffer and sends an RST to the peer, not the
		3. If l_onoff != 0 and l_linger > 0, then the kernel will linger
		   when the socket is closed. That is, if there is any data still
		   remaining in the socket send buffer, the process is put to sleep until either: 
		   (i) all the data is sent and acknowledged by the peer TCP, or
		   (ii) the linger time	expires. If the socket has been set to nonblocking, it will not wait
		   for the close to complete, even if the linger time is nonzero. When using this
		   feature of the SO_LINGER option, it is important for the application to check the
		   return value from close, because if the linger time expires before the
		   remaining data is sent and acknowledged, close returns EWOULDBLOCK and
		   any remaining data in the send buffer is discarded.

 <SO_OOBINLINE>
  - When this option is set, out-of-band data will be placed in the normal input queue (i.e.,
	inline). When this occurs, the MSG_OOB flag to the receive functions cannot be used to
	read the out-of-band data.

 <SO_RCVBUF>
 <SO_SNDBUF>
  - Every socket has a send buffer and a receive buffer.
	The receive buffers are used by TCP, UDP, and SCTP to hold received data until it is
	read by the application. With TCP, the available room in the socket receive buffer
	limits the window that TCP can advertise to the other end. The TCP socket receive
	buffer cannot overflow because the peer is not allowed to send data beyond the
	advertised window. This is TCP's flow control, and if the peer ignores the advertised
	window and sends data beyond the window, the receiving TCP discards it. With UDP,
	however, when a datagram arrives that will not fit in the socket receive buffer, that
	datagram is discarded. Recall that UDP has no flow control: It is easy for a fast sender
	to overwhelm a slower receiver, causing datagrams to be discarded by the receiver's
	UDP.In fact, a fast sender can overwhelm its own
	network interface, causing datagrams to be discarded by the sender itself.

	These two socket options let us change the default sizes.
	For a client, this means the SO_RCVBUF socket option must be set before calling
	connect. For a server, this means the socket option must be set for the listening
	socket before calling listen. Setting this option for the connected socket will have no
	effect whatsoever on the possible window scale option because accept does not
	return with the connected socket until TCP's three-way handshake is complete. That
	is why this option must be set for the listening socket. (The sizes of the socket buffers
	are always inherited from the listening socket by the newly created connected socket.

 <SO_RCVLOWAT>
 <SO_SNDLOWAT>
  - Every socket also has a receive low-water mark and a send low-water mark. These
	are used by the select function. These two socket
	options, SO_RCVLOWAT and SO_SNDLOWAT, let us change these two low-water marks.

	The receive low-water mark is the amount of data that must be in the socket receive
	buffer for select to return "readable." It defaults to 1 for TCP, UDP, and SCTP sockets.
	The send low-water mark is the amount of available space that must exist in the
	socket send buffer for select to return "writable." This low-water mark normally
	defaults to 2,048 for TCP sockets. With UDP, the low-water mark is used, as we
	described in Section 6.3, but since the number of bytes of available space in the send
	buffer for a UDP socket never changes (since UDP does not keep a copy of the
	datagrams sent by the application), as long as the UDP socket send buffer size is
	greater than the socket's low-water mark, the UDP socket is always writable.

 <SO_RCVTIMEO>
 <SO_SNDTIMEO>
  - These two socket options allow us to place a timeout on socket receives and sends.
	Notice that the argument to the two sockopt functions is a pointer to a timeval
	structure, the same one used with select. This lets us specify the
	timeouts in seconds and microseconds. We disable a timeout by setting its value to 0
	seconds and 0 microseconds. Both timeouts are disabled by default.

 <SO_REUSEADDR>
 <SO_REUSEPORT>

 <SO_TYPE>
  - This option returns the socket type. The integer value returned is a value such as
	SOCK_STREAM or SOCK_DGRAM. This option is typically used by a process that inherits a
	socket when it is started.

 <SO_USELOOPBACK>
  - This option applies only to sockets in the routing domain (AF_ROUTE). This option
	defaults to ON for these sockets (the only one of the SO_xxx socket options that
	defaults to ON instead of OFF). When this option is enabled, the socket receives a
	copy of everything sent on the socket.
 

IPv4 Socket Options
 <IP_HDRINCL>
  - If this option is set for a raw IP socket (Chapter 28), we must build our own IP header
	for all the datagrams we send on the raw socket. Normally, the kernel builds the IP
	header for datagrams sent on a raw socket, but there are some applications (notably
	traceroute) that build their own IP header to override values that IP would place into
	certain header fields.

 <IP_OPTIONS>
  - Setting this option allows us to set IP options in the IPv4 header. This requires
	intimate knowledge of the format of the IP options in the IP header.

 <IP_RECVDSTADDR>
  - This socket option causes the destination IP address of a received UDP datagram to
	be returned as ancillary data by recvmsg.

 <IP_RECVIF>
  - This socket option causes the index of the interface on which a UDP datagram is
	received to be returned as ancillary data by recvmsg

 <IP_TOS>
  - This option lets us set the type-of-service (TOS) field (which contains the DSCP and
	ECN fields) in the IP header for a TCP, UDP, or SCTP socket. If we call
	getsockopt for this option, the current value that would be placed into the DSCP and
	ECN fields in the IP header (which defaults to 0) is returned. There is no way to fetch
	the value from a received IP datagram.

 <IP_TTL>
  - With this option, we can set and fetch the default TTL that the system will
	use for unicast packets sent on a given socket. (The multicast TTL is set using the
	IP_MULTICAST_TTL socket option) 



IPv6 Socket Options
 <ICMP6_FILTER>
  - 此选项使我们可以获取和设置一个icmp6_filter结构，它指明256个可能的ICMPv6消息类型中哪一个传递给原始套接口上的进程。

 <IPV6_ADDRFORM>
  - 这个选项允许套接口从IPv4转换到IPv6，反之亦可。

 <IPV6_CHECKSUM>
  - 此选项指定用户数据中校验和所处位置的字节偏移。

 <IPV6_DSTOPTS>
  - 任何接收到的IPv6目标选项都将由recvmsg作为辅助数据返回。

 <IPV6_HOPLIMIT>
  - 接收到的跳限字段将由recvmsg作为辅助数据返回。

 <IPV6_HOPOPTS>
  - 任何接收到的IPv6步跳选项都将由recvmsg作为辅助数据返回。

 <IPV6_NEXTHOP>
  - 这不是一个套接口选项，而是一个可指定给sendmsg的辅助数据对象的类型。

 <IPV6_PKTINFO>
  - 设置此选项表明，下面关于接收到的IPv6数据报的两条信息将由recvmsg作为辅助数据返回。

 <IPV6_PKTOPTIONS>
  - 大多数IPv6套接口选项假设UDP套接口使用recvmsg和sendmsg所用的辅助数据在内核和应用进程间传递信息。

 <IPV6_RTHDR>
  - 设置这个选项表明接收到的IPv6路由头部将由recvmsg作为辅助数据返回。

 <IPV6_UNICAST_HOPS>
  - 此IPv6选项类似于IPv4的IP_TTL套接口选项。

 

TCP Socket Options
 <TCP_KEEPALIVE>
  - 它指定TCP开始发送保持存活探测分节前以秒为单位的连接空闲时间。
 				缺省值至少为7200秒，即2小时。此选项尽在SO_KEEPALIVE套接口选项打开时才有效。

 <TCP_MAXRT>
  - 它指定一旦TCP开始重传数据，在连接断开之前需经历的以秒为单位的时间总量。值0意味着使用系统缺省值，值-1意味着永远重传数据。如果是一个正值，它可能向上舍入成实现的下一次重传时间。

 <TCP_MAXSEG>
  - 获取或设置TCP连接的最大分节大小。

 <TCP_NODELAY>
  - 如果设置，此选项禁止TCP的Nagle算法。

 <TCP_STDURG>
  - 它影响对TCP紧急指针的解释。
*/
union val {
	int i_val;
	long l_val;
	struct linger linger_val;
	struct timeval timeval_val;
} val;

static char strres[128];

static char *sock_str_flag(union val *ptr, int len)
{
	if (len != sizeof (int))
		snprintf(strres, sizeof(strres), "size (%d) not sizeof(int)", len);
	else
		snprintf(strres, sizeof(strres), "%s", (ptr->i_val == 0) ? "off" : "on");
	return strres;
}

static char *sock_str_int(union val *ptr, int len)
{
	if (len != sizeof(int))
		snprintf(strres, sizeof(strres), "size (%d) not sizeof(int)", len);
	else
		snprintf(strres, sizeof(strres), "%d", ptr->i_val);
	return strres;
}

static char *sock_str_linger(union val *ptr, int len)
{
	struct linger * lptr = &ptr->linger_val;
	
	if (len != sizeof(struct linger))
		snprintf(strres, sizeof(strres), "size (%d) not sizeof(struct linger)", len);
	else
		snprintf(strres, sizeof(strres), "l_onoff = %d, l_linger = %d", lptr->l_onoff, lptr->l_linger);
	return strres;
}

static char *sock_str_timeval(union val *ptr, int len)
{
	struct timeval * tvptr = &ptr->timeval_val;

    if (len != sizeof(struct timeval))
        snprintf(strres, sizeof(strres), "size (%d) not sizeof(struct timeval)", len);
    else
        snprintf(strres, sizeof(strres), "%d sec, %d usec", tvptr->tv_sec, tvptr->tv_usec);
    return(strres);
}

struct sock_opts {
	const char *opt_str;
	int opt_level;
	int opt_name;
	char *(*opt_val_str) (union val *, int);
} sock_opts[] = {
	{ "SO_BROADCAST", SOL_SOCKET, SO_BROADCAST,	sock_str_flag },
	{ "SO_DEBUG",     SOL_SOCKET, SO_DEBUG,     sock_str_flag },
	{ "SO_DONTROUTE", SOL_SOCKET, SO_DONTROUTE,	sock_str_flag },
	{ "SO_ERROR",     SOL_SOCKET, SO_ERROR,     sock_str_int },
	{ "SO_KEEPALIVE", SOL_SOCKET, SO_KEEPALIVE,	sock_str_flag },
	{ "SO_LINGER",    SOL_SOCKET, SO_LINGER,	sock_str_linger },
	{ "SO_OOBINLINE", SOL_SOCKET, SO_OOBINLINE,	sock_str_flag },
	{ "SO_RCVBUF",    SOL_SOCKET, SO_RCVBUF,    sock_str_int },
	{ "SO_SNDBUF",    SOL_SOCKET, SO_SNDBUF,    sock_str_int },
	{ "SO_RCVLOWAT",  SOL_SOCKET, SO_RCVLOWAT,  sock_str_int },
	{ "SO_SNDLOWAT",  SOL_SOCKET, SO_SNDLOWAT,  sock_str_int },
	{ "SO_RCVTIMEO",  SOL_SOCKET, SO_RCVTIMEO,	sock_str_timeval },
	{ "SO_SNDTIMEO",  SOL_SOCKET, SO_SNDTIMEO,	sock_str_timeval },
	{ "SO_REUSEADDR", SOL_SOCKET, SO_REUSEADDR,	sock_str_flag },
#ifdef SO_REUSEPORT
	{ "SO_REUSEPORT", SOL_SOCKET, SO_REUSEPORT,	sock_str_flag },
#else
	{ "SO_REUSEPORT", 0, 0, NULL },
#endif
	{ "SO_TYPE",      SOL_SOCKET, SO_TYPE,      sock_str_int },
	// { "SO_USELOOPBACK", SOL_SOCKET, SO_USELOOPBACK,	sock_str_flag },
	{ "IP_TOS",       IPPROTO_IP, IP_TOS,       sock_str_int },
	{ "IP_TTL",       IPPROTO_IP, IP_TTL,       sock_str_int },
#if 0
	{ "IPV6_DONTFRAG",IPPROTO_IPV6, IPV6_DONTFRAG, sock_str_flag },
	{ "IPV6_UNICAST_HOPS", IPPROTO_IPV6, IPV6_UNICAST_HOPS, sock_str_int },
	{ "IPV6_V6ONLY",  IPPROTO_IPV6, IPV6_V6ONLY, sock_str_flag },
#endif
	{ "TCP_MAXSEG",   IPPROTO_TCP, TCP_MAXSEG, sock_str_int },
	{ "TCP_NODELAY",  IPPROTO_TCP, TCP_NODELAY,	sock_str_flag },
	{ "SCTP_AUTOCLOSE",	IPPROTO_SCTP, SCTP_AUTOCLOSE, sock_str_int },
	// { "SCTP_MAXBURST", IPPROTO_SCTP, SCTP_MAXBURST, sock_str_int },
	{ "SCTP_MAXSEG",  IPPROTO_SCTP, SCTP_MAXSEG, sock_str_int },
	{ "SCTP_NODELAY", IPPROTO_SCTP, SCTP_NODELAY,sock_str_flag },
	{ NULL, 0, 0, NULL }
};

void check_socket_options()
{
	int fd;
	socklen_t len;
	struct sock_opts *ptr;

	for (ptr = sock_opts; ptr->opt_str != NULL; ptr++) {
		printf("%s: ", ptr->opt_str);
		if (ptr->opt_val_str == NULL)
			printf("(undefined)\n");
		else {
			switch (ptr->opt_level) {
				case SOL_SOCKET:
				case IPPROTO_IP:
				case IPPROTO_TCP:
				fd = socket(AF_INET, SOCK_STREAM, 0);
				break;
#ifdef IPV6
				case IPPROTO_IPV6:
				fd = socket(AF_INET6, SOCK_STREAM, 0);
				break;
#endif
#ifdef IPPROTO_SCTP
				case IPPROTO_SCTP:
				fd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
				break;
#endif
				default: printf("Can't create fd for level %d\n", ptr->opt_level);
			}
			len = sizeof(val);
			if (getsockopt(fd, ptr->opt_level, ptr->opt_name, &val, &len) == -1) {
				printf("getsockopt error: %s\n", strerror(errno));
			} else {
				printf("default = %s\n", (*ptr->opt_val_str) (&val,	len));
			}
			close(fd);
		}
	}

}

