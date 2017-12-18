#ifndef _LOG_UTIL_H
#define _LOG_UTIL_H
#include <stdio.h>

#define BUFSIZE 1500

#define func_enter() printf("%s enter.\n", __FUNCTION__)
#define func_exit()  printf("%s exit.\n", __FUNCTION__)

#ifdef DEBUG_CHECK_PARAMETERS
/**
  * The assert_param macro is used for function's parameters check.
  * @expr: If expr is false, it calls assert_failed function which reports 
  * the name of the source file and the source line number of the call 
  * that failed. If expr is true, it returns no value.
  */
void assert_fail(const char *, const char*, unsigned int, const char *);
#define assert_param(expr) ((expr) ? (void)0 : assert_fail(#expr, __FILE__, __LINE__, __FUNCTION__))
#else
#define assert_param(expr) ((void)0)
#endif


#define assert_return(expr) assert_param(expr)

#define ASSERT(v) do {                                                 \
    if ((v) < 0) {                                                     \
        printf("system-error: '%s' (code: %d)", strerror(errno), errno);  \
        return -1; }                                                   \
    } while (0)

extern void sys_debug (int level, char *fmt, ...);


#endif

