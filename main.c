/**
 * Parameters check rules:
 * 1. Function with 'static' stated, we check none;
 * 2. otherwise, do the check
 */
#include <stdio.h>
#include <unistd.h>

#include <sockets.h>
#include <log_util.h>

#define TEST_ASSERT 0
#define TEST_FIFO 0
#define TEST_LIST 0
#define TEST_JSON 0
#define TEST_UART 0
#define TEST_FB   1
#define TEST_TCP_SERVER 0
#define TEST_TCP_CLIENT 0
#define TEST_UDP_SERVER 0
#define TEST_UDP_CLIENT 0
#define TEST_TCP_SEPOLL 1



/** externals */
extern void assert_test_entry();
extern void list_test_entry();
extern void fifo_test_entry();
extern void json_test_entry();
extern void uart_test_entry();
extern void fbtest_entry();
extern void setup_signal_handler();
extern void socket_tcp_server_test_entry();
extern void socket_udp_server_test_entry();
extern void socket_tcp_client_test_entry();
extern void socket_tcp_server_epoll_test_entry();


int main(int argc, char *argv[])
{
	const char *host_ip = NULL;
	setup_signal_handler();

#if TEST_ASSERT == 1
	/* trigger a fault */
	assert_param(argc == 0);
#elif TEST_FIFO == 1
    list_test_entry();
#elif TEST_LIST == 1
    fifo_test_entry();
#elif TEST_JSON == 1
    json_test_entry();
#elif TEST_UART == 1
	uart_test_entry();
#elif TEST_FB == 1
	fbtest_entry();
#elif TEST_TCP_SERVER == 1
	init_network();
	host_ip = get_netdev_ip("ens33");
	socket_tcp_server_test_entry();
#elif TEST_TCP_SEPOLL == 1
	init_network();
	host_ip = get_netdev_ip("ens33");
	assert_return(host_ip);
	sys_debug(3, "DEBUG: host ip: %s", host_ip);
	socket_tcp_server_epoll_test_entry();
#elif TEST_TCP_CLIENT == 1
	socket_tcp_client_test_entry();
#elif TEST_UDP_SERVER == 1
	socket_udp_server_test_entry();
#endif
	/* only the superuser can create a raw socket */
	//ping("8.8.8.8");
	while (1) {
		sleep(5);
	}

#if TEST_TCP_SERVER == 1
	deinit_network();
#endif
	return 0;
}
