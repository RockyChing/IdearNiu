/**
 * Parameters check rules:
 * 1. Function with 'static' stated, we check none;
 * 2. otherwise, do the check
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sockets.h>
#include <log_util.h>
#include <utils.h>
#include <main.h>
#include <utils.h>
#include <busybox.h>

int cmd_cb(char *buff, size_t len)
{
	printf("%s", buff);
	return len;
}

int main(int argc, char *argv[])
{
	setup_signal_handler();
#if 0
	run_command("wpa_cli scan", cmd_cb);
	//sleep(5);
	run_command("wpa_cli scan_result", cmd_cb);
	common_test();
#endif
	if (argc == 2) {
		if (!strcmp(argv[1], "ps")) {
			ps_main();
		}
	}
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
#elif TEST_SOCKET == 1
	return socket_common_test(argc, argv);
#elif TEST_TCP_SERVER == 1
	init_network();
	const char *host_ip = NULL;

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
	init_network();
	socket_udp_server_test_entry();
#elif TEST_GET_EVENT == 1
	return getevent_test_entry(argc, argv);
#elif TEST_DRIVER_MODEL == 1
	driver_model_test_entry();
#elif TEST_ALSA == 1
	alsa_test_entry();
	return 0;
#elif TEST_THREAD_LIMITS == 1
	thread_limits_test_entry();
#elif TEST_TINYALSA == 1
	return tinyalsa_test_entry(argc, argv);
#elif TEST_SQLITE == 1
	sqlite_test_entry();
#elif TEST_NET_IFREQ == 1
	ifreq_test_entry();
#elif TEST_BASE64 == 1
	return base64_test_entry();
#elif TEST_HTTP_CLIENT == 1
	httpc_test_entry();
#endif
	/* only the superuser can create a raw socket */
	//ping("8.8.8.8");
	while (1) {
		sleep(5);
		ps_main();
	}

#if TEST_TCP_SERVER == 1
	deinit_network();
#endif
	return 0;
}
