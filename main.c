#include <stdio.h>
#include <unistd.h>

#include <sockets.h>


/** externals */
extern void list_test_entry();
extern void fifo_test_entry();
extern void json_test_entry();
extern void setup_signal_handler();
extern void socket_tcp_server_test_entry();
extern void socket_udp_server_test_entry();


int main(int argc, char *argv[])
{
	setup_signal_handler();
	init_network();
	is_netdev_has_ip("ens33");

    list_test_entry();
    fifo_test_entry();
    json_test_entry();
	socket_tcp_server_test_entry();
	socket_udp_server_test_entry();

	/* only the superuser can create a raw socket */
	//ping("8.8.8.8");
	while (1) {
		sleep(5);
	}

	deinit_network();
	return 0;
}
