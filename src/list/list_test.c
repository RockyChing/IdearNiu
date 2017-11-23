#include <stdio.h>
#include <list.h>

struct list_head test_list_head;

void test_list_entry()
{
    printf("start testing list\n");

    INIT_LIST_HEAD(&test_list_head);
}

