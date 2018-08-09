#ifndef _LINUX_MAIN_H
#define _LINUX_MAIN_H

//#define TEST_COMMON 0
#define TEST_ASSERT 0
#define TEST_FIFO 0
#define TEST_LIST 0
#define TEST_JSON 0
#define TEST_UART 0
#define TEST_FB   0
#define TEST_SOCKET     0
#define TEST_TCP_SERVER 0
#define TEST_TCP_CLIENT 0
#define TEST_UDP_SERVER 0
#define TEST_UDP_CLIENT 0
#define TEST_TCP_SEPOLL 0
#define TEST_GET_EVENT  0
#define TEST_DRIVER_MODEL 0
#define TEST_ALSA 0
#define TEST_THREAD_LIMITS 0
#define TEST_TINYALSA 0
#define TEST_SQLITE 0
#define TEST_NET_IFREQ 0
#define TEST_BASE64 0
#define TEST_HTTP_CLIENT 0
#define TEST_DHCPC 1



/** externals */
extern void common_test();
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
extern void driver_model_test_entry();
extern void alsa_test_entry();
extern void thread_limits_test_entry();
extern void sqlite_test_entry();
extern void ifreq_test_entry();
extern void httpc_test_entry();

extern int socket_common_test(int argc, char *argv[]);
extern int getevent_test_entry(int argc, char *argv[]);
extern int tinyalsa_test_entry(int argc, char *argv[]);
extern int base64_test_entry();

extern int dhcp_main(int argc, char *argv[]);

#endif

