#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <threads.h>
#include <sockets.h>
#include <utils.h>
#include <delay.h>
#include <log_util.h>


static void http_header(char *str, const char *host, const char *path, const char *content)
{
	const int content_length = strlen(content);

	/**
	 * 1. request: 
	 * method [space] URL [space] HTTP-Version \r\n
	 */
	sprintf(str,
		"POST %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"User-Agent: Mozilla/5.0 (Windows NT 6.1; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/59.0.3071.109 Safari/537.36\r\n"
		"Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n"
		"Content-Length: %d\r\n"
		"Keep-Alive: 300\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"%s", path, host, content_length, content);
}

static void *http_send_thread(void *arg)
{
	char buf[BUFSIZE];
	int sockfd = *((int *)arg);
	const char *host = "api.avatardata.cn";
	const char *path = "/Weather/Query";
	const char *content = "key=4c70c85f8405453d96d162b47bc29a97&cityname=无锡&dtype=JSON&format=true";
	while (1) {
		if (sockfd == INVALID_SOCKET) {
			sleep(1);
			continue;
		}

		sleep(5);
		memset(buf, 0, sizeof(buf));
		http_header(buf, host, path, content);
		printf("Request:\n%s\n", buf);
		sock_write_bytes(sockfd, buf, strlen(buf));
	}

	return NULL;
}

static void htt_reader(void *buf, size_t len)
{
	if (len > 0)
		printf("\n%s\n\n", (char *)buf);
}

static void http()
{
	int i, ret;
	int sockfd;

	char ipstr[4][16] = { { 0 }, };
	ret = getip_byhostname("api.avatardata.cn", ipstr);
	for (i = 0; i < ret; i ++)
		sys_debug(1, "got ip '%s'", ipstr[i]);

	if ((sockfd = sock_connect(ipstr[0], 80, 9000)) == INVALID_SOCKET) {
		sys_debug(1, "ERROR: create sockfd");
		assert_return(0);
	}

	pthread_t s_thread;
	printf("start http client thread...\n");

	ret = thread_create(&s_thread, http_send_thread, &sockfd);
	assert_return(ret == 0);

	sock_read_loop(sockfd, htt_reader, 10000);
}

void httpc_test_entry()
{
	func_enter();
	http();

	func_exit();
}

