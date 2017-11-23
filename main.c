#include <stdio.h>

/** externals */
extern void test_list_entry();

extern void test_fifo_entry();


int main(int argc, char *argv[])
{
    test_list_entry();
    test_fifo_entry();

	return 0;
}
