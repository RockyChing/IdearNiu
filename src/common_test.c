#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include <type_def.h>
#include <log_util.h>

typedef struct _framectrl_80211 {
    //buf[0]
    uint8_t Protocol:2;
    uint8_t Type:2;
    uint8_t Subtype:4;
    //buf[1]
    uint8_t ToDS:1;
    uint8_t FromDS:1;
    uint8_t MoreFlag:1;
    uint8_t Retry:1;
    uint8_t PwrMgmt:1;
    uint8_t MoreData:1;
    uint8_t Protectedframe:1;
    uint8_t Order:1;
} framectrl_80211, *pframectrl_80211;


int common_test()
{
	func_enter();
	// result: 'sizeof: 2 bytes'
	sys_debug(0, "sizeof: %d bytes\n", sizeof(struct _framectrl_80211));

	func_exit();
	return 0;
}

