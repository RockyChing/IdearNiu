#include <stdio.h>
#include <fifo.h>

struct kfifo *test_fifo;

void test_fifo_entry()
{
    printf("start testing fifo, !!!but not really start!!!\n");

    test_fifo = kfifo_alloc(2048, NULL);
}


