#ifndef _LOG_UTIL_H
#define _LOG_UTIL_H
#include <stdio.h>

#define BUFSIZE 1000

#define func_enter() printf("%s enter.\n", __FUNCTION__)
#define func_exit()  printf("%s exit.\n", __FUNCTION__)


extern void sys_debug (int level, char *fmt, ...);




#endif

