#ifndef LOG_H
#define LOG_H

/* The log file to which Wget writes to after HUP.  */
#define DEFAULT_LOGFILE "wget-log"

#include <stdio.h>

enum log_options { LOG_VERBOSE, LOG_NOTQUIET, LOG_NONVERBOSE, LOG_ALWAYS, LOG_PROGRESS };

void log_set_warc_log_fp (FILE *);

void logprintf (enum log_options, const char *, ...)
     GCC_FORMAT_ATTR (2, 3);
void logputs (enum log_options, const char *);
void logflush (void);
void log_set_flush (bool);
bool log_set_save_context (bool);

void log_init (const char *, bool);
void log_close (void);
void log_cleanup (void);
void log_request_redirect_output (const char *);
void redirect_output (bool, const char *);

const char *escnonprint_uri (const char *);

#endif /* LOG_H */



#ifndef _LOG_UTIL_H
#define _LOG_UTIL_H
#include <stdio.h>

#define BUFSIZE 1500
#define LOG_ERROR    0
#define LOG_WARNING  1
#define LOG_INFO	  2
#define LOG_DEBUG    3
#define DEFAULT_LOG_LEVEL LOG_DEBUG

#define func_enter() printf("+++++ %s(%d) +++++\n", __FUNCTION__, __LINE__)
#define func_exit()  printf("----- %s(%d) -----\n", __FUNCTION__, __LINE__)

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

#define debug(x...)   	sys_debug(LOG_DEBUG, x)


#define log_debug(x...)   	sys_debug(LOG_DEBUG, x)
#define log_info(x...)   	sys_debug(LOG_INFO, x)
#define log_warn(x...) 		sys_debug(LOG_WARNING, x)
#define log_error(x...)   	do {sys_debug(LOG_ERROR, x); abort(); } while (0)

#if 0
#define ALOGD(x...)   sys_debug_ext(LOG_DEBUG, LOG_TAG, __LINE__, x)
#define ALOGV(x...)   sys_debug_ext(LOG_INFO, LOG_TAG, __LINE__, x)
#define ALOGI(x...)   sys_debug_ext(LOG_INFO, LOG_TAG, __LINE__, x)
#define ALOGW(x...)   sys_debug_ext(LOG_WARNING, LOG_TAG, __LINE__, x)
#define ALOGE(x...)   do {sys_debug_ext(LOG_ERROR, LOG_TAG, __LINE__, x); abort(); } while (0)

#define SLOGD(x...)   sys_debug_ext(LOG_DEBUG, LOG_TAG, __LINE__, x)
#define SLOGI(x...)   sys_debug_ext(LOG_INFO, LOG_TAG, __LINE__, x)
#define SLOGV(x...)   sys_debug_ext(LOG_INFO, LOG_TAG, __LINE__, x)
#define SLOGW(x...)   sys_debug_ext(LOG_WARNING, LOG_TAG, __LINE__, x)
#define SLOGE(x...)   sys_debug_ext(LOG_ERROR, LOG_TAG, __LINE__, x)
#endif

#endif

