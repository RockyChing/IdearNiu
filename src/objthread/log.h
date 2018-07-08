#ifndef _NETD_LOG_H
#define _NETD_LOG_H

#define LOG_ERROR    0
#define LOG_WARNING  1
#define LOG_INFO	 2
#define LOG_DEBUG    3

#define DEFAULT_LOG_LEVEL LOG_DEBUG

#define hex_dump(buf, len)	\
	do { \
		int i; \
		for(i = 0; i < len; i++) { \
			if(0 == (i % 16) && 0 != i) \
				printf("\n"); \
			char *p = (char*) buf; \
			printf("%02x ", (p[i]&0xff)); \
		} \
		printf("\n"); \
	} while(0)

static inline void sys_debug(int level, const char *tag, int line_num, const char *fmt, ...)
{

}

#define debug(x...)   sys_debug(LOG_DEBUG, LOG_TAG, __LINE__, x)
#define info(x...)    sys_debug(LOG_INFO, LOG_TAG, __LINE__, x)
#define warning(x...) sys_debug(LOG_WARNING, LOG_TAG, __LINE__, x)
#define error(x...)   sys_debug(LOG_ERROR, LOG_TAG, __LINE__, x)

#define ALOGD(x...)   sys_debug(LOG_DEBUG, LOG_TAG, __LINE__, x)
#define ALOGV(x...)   sys_debug(LOG_INFO, LOG_TAG, __LINE__, x)
#define ALOGI(x...)   sys_debug(LOG_INFO, LOG_TAG, __LINE__, x)
#define ALOGW(x...)   sys_debug(LOG_WARNING, LOG_TAG, __LINE__, x)
#define ALOGE(x...)   sys_debug(LOG_ERROR, LOG_TAG, __LINE__, x)

#define SLOGD(x...)   sys_debug(LOG_DEBUG, LOG_TAG, __LINE__, x)
#define SLOGI(x...)   sys_debug(LOG_INFO, LOG_TAG, __LINE__, x)
#define SLOGV(x...)   sys_debug(LOG_INFO, LOG_TAG, __LINE__, x)
#define SLOGW(x...)   sys_debug(LOG_WARNING, LOG_TAG, __LINE__, x)
#define SLOGE(x...)   sys_debug(LOG_ERROR, LOG_TAG, __LINE__, x)

#define ALOG_ASSERT(b, x...) sys_debug(LOG_ERROR, LOG_TAG, __LINE__, x)


#endif

