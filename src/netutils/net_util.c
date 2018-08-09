#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

#include <utils.h>
#include <net_util.h>
#include <log_util.h>
#include <sockets.h>


int network_time_sync(const char *time_server)
{
	char *ts = time_server;
	char ipstr[4][16] = { { 0 }, };
	char tv_buf[32] = { 0 };
	time_t tv_sec = 0;
	int sockfd = INVALID_SOCKET;
	int ret = -1;

	if (NULL == ts) {
		ts = DEFAULT_TIME_SERVER;
	}

	getip_byhostname(ts, ipstr);
	if ((sockfd = sock_connect(ipstr[0], DEFAULT_TIME_SERVER_PORT, 5000)) == INVALID_SOCKET) {
		error("ERROR: create network_time_sync socket");
		return -1;
	}

	/**
	 * The time server will send a big-endian 32-bit integer representing
	 * the number of seconds since since 00:00 (midnight) 1 January 1900 GMT.
	 * http://www.faqs.org/rfcs/rfc868.html
	 For example:
		the time  2,208,988,800 corresponds to 00:00  1 Jan 1970 GMT,
				  2,398,291,200 corresponds to 00:00  1 Jan 1976 GMT,
				  2,524,521,600 corresponds to 00:00  1 Jan 1980 GMT,
				  2,629,584,000 corresponds to 00:00  1 May 1983 GMT,
			 and -1,297,728,000 corresponds to 00:00 17 Nov 1858 GMT.
	 */
	ret = recv(sockfd, tv_buf, sizeof(tv_buf), 0);
	debug("%s response:", DEFAULT_TIME_SERVER);
	dump(tv_buf, ret);
	char buf[4];
	buf[0] = tv_buf[3];
	buf[1] = tv_buf[2];
	buf[2] = tv_buf[1];
	buf[3] = tv_buf[0];
	memcpy(&tv_sec, buf, 4);

	printf("tv_sec:\t\t %x\n", tv_sec);
	tv_sec -= 2208988800;
	printf("tv_sec:\t\t %x\n", tv_sec);
	printf("now in string:\t %s\n\n", ctime(&tv_sec));

	sock_close(sockfd);
	exit(0);
	return 0;
}

