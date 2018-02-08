#include <stdio.h>
#include <list.h>

struct list_head test_list_head;

void list_test_entry()
{
    printf("start testing list, !!!but not really start!!!\n");

    INIT_LIST_HEAD(&test_list_head);
}

