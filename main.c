#include <stdio.h>

/** externals */
extern void list_test_entry();
extern void fifo_test_entry();
extern void json_test_entry();


int main(int argc, char *argv[])
{
    list_test_entry();
    fifo_test_entry();
    json_test_entry();

	return 0;
}
