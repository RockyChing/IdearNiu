#include <stdio.h>

#include <log_ext.h>
#include <utils.h>
#include <list.h>
#include <hlist.h>

#define MAX_ITEM 256

struct hlist_head htable[MAX_ITEM];

struct hdata_node {
    unsigned int data;
    struct hlist_node list;
};

static void helper(void)
{
	fprintf(stderr, "%s", "\nEnter:\n" \
		"1: insert\n" \
        "2: serach\n" \
        "3: delete\n" \
        "0: quit\n");
}

int main(int argc, char *argv[])
{
	struct hdata_node *hnode = NULL;
    struct hlist_node *hlist = NULL, *n = NULL;
    int i = 0, quit = 0, opt = 0, key;
    unsigned int data;

	// htable init
	for (i = 0; i < ARRAY_SIZE(htable); i ++)
		INIT_HLIST_HEAD(&htable[i]);

	while (!quit) {
		helper();
		scanf("%d", &opt);
		switch (opt) {
		case 1:
			hnode = zmalloc(sizeof(struct hdata_node));
			if (!hnode) {
				log_error("out of memeory!");
				exit(-1);
			}

			INIT_HLIST_NODE(&hnode->list);
			fprintf(stderr, "%s", "enter data:");
			scanf("%d", &hnode->data);
			key = hnode->data % MAX_ITEM;
			hlist_add_head(&hnode->list, &htable[key]);
			log_debug("add %d at pos %d", hnode->data, key);
			break;
		case 2:
			fprintf(stderr, "%s", "enter data to search:");
			scanf("%d", &data);
			key = data % MAX_ITEM;
			if (hlist_empty(&htable[key])) {
				log_info("list at pos %d empty.", key);
			} else {
				hlist_for_each_entry(hnode, hlist, &htable[key], list) {
                    if(hnode->data == data)
                        log_info("find data %u at pos %d", hnode->data, key);
					//else 
					//	log_info("no data %u at pos %d", hnode->data, key);
                }
			} break;
		case 3:
			fprintf(stderr, "%s", "enter data to delete:");
			scanf("%d", &data);
			key = data % MAX_ITEM;
			if (hlist_empty(&htable[key])) {
				log_info("list at pos %d empty.", key);
			} else {
				hlist_for_each_entry_safe(hnode, hlist, n, &htable[key], list){
                    if(hnode->data == data){
                        hlist_del(&hnode->list);
						log_info("delete data %u at pos %d\n", hnode->data, key);
                        break;
                    }
                }
			} break;
		case 0:
			quit = 1;
			break;
		default:
			log_info("Unknown input!");
			break;
		}
	}

	// memory release
	for (i = 0; i < ARRAY_SIZE(htable); i ++) {
		hlist_for_each_entry_safe(hnode, hlist, n, &htable[i], list) {
			hlist_del(&hnode->list);
			log_debug("%u removed at pos %d", hnode->data, i);
			xfree(hnode);
		}
	}

	log_info("Exit.");
	return 0;
}

