#include <stdio.h>
#include <unistd.h>

/** externals */
extern void list_test_entry();
extern void fifo_test_entry();
extern void json_test_entry();
extern void setup_signal_handler();


int main(int argc, char *argv[])
{
	setup_signal_handler();

    list_test_entry();
    fifo_test_entry();
    json_test_entry();

	while (1) {
		sleep(5);
	}

	return 0;
}
