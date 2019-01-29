#ifndef _LOG_UTIL_H
#define _LOG_UTIL_H
#include <stdio.h>

#define BUFSIZE 1500
#define LOG_ERROR    0
#define LOG_WARNING  1
#define LOG_INFO	  2
#define LOG_DEBUG    3
#define DEFAULT_LOG_LEVEL LOG_DEBUG

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

#define dump(buf, len)	\
	do { \
		int i; \
		char *p = (char*) buf; \
		for(i = 0; i < len; i++) { \
			if(0 == (i % 16) && 0 != i) \
				printf("\n"); \
			printf("%02x ", (p[i]&0xff)); \
		} \
		printf("\n"); \
	} while(0)

extern void sys_debug (int level, char *fmt, ...);
extern void sys_debug_ext(int level, const char *tag, int line_num, const char *fmt, ...);

#define debug(x...)   sys_debug(LOG_DEBUG, x)
#define info(x...)   sys_debug(LOG_INFO, x)
#define warning(x...) sys_debug(LOG_WARNING, x)
#define error(x...)   do {sys_debug(LOG_ERROR, x); abort(); } while (0)

#define ALOGD(x...)   sys_debug_ext(LOG_DEBUG, LOG_TAG, __LINE__, x)
#define ALOGV(x...)   sys_debug_ext(LOG_INFO, LOG_TAG, __LINE__, x)
#define ALOGI(x...)   sys_debug_ext(LOG_INFO, LOG_TAG, __LINE__, x)
#define ALOGW(x...)   sys_debug_ext(LOG_WARNING, LOG_TAG, __LINE__, x)
#define ALOGE(x...)   sys_debug_ext(LOG_ERROR, LOG_TAG, __LINE__, x)

#define SLOGD(x...)   sys_debug_ext(LOG_DEBUG, LOG_TAG, __LINE__, x)
#define SLOGI(x...)   sys_debug_ext(LOG_INFO, LOG_TAG, __LINE__, x)
#define SLOGV(x...)   sys_debug_ext(LOG_INFO, LOG_TAG, __LINE__, x)
#define SLOGW(x...)   sys_debug_ext(LOG_WARNING, LOG_TAG, __LINE__, x)
#define SLOGE(x...)   sys_debug_ext(LOG_ERROR, LOG_TAG, __LINE__, x)

#endif

