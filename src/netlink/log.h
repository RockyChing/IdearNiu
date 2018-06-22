#ifndef _NETD_LOG_H
#define _NETD_LOG_H

#define LOG_ERROR    0
#define LOG_WARNING  1
#define LOG_INFO	 2
#define LOG_DEBUG    3

#define DEFAULT_LOG_LEVEL LOG_INFO

void sys_debug(int level, const char *tag, const char *fmt, ...);

#define debug(x...)   sys_debug(LOG_DEBUG, LOG_TAG, x)
#define info(x...)    sys_debug(LOG_INFO, LOG_TAG, x)
#define warning(x...) sys_debug(LOG_WARNING, LOG_TAG, x)
#define error(x...)   sys_debug(LOG_ERROR, LOG_TAG, x)

#define ALOGD(x...)   sys_debug(LOG_DEBUG, LOG_TAG, x)
#define ALOGV(x...)   sys_debug(LOG_INFO, LOG_TAG, x)
#define ALOGW(x...)   sys_debug(LOG_WARNING, LOG_TAG, x)
#define ALOGE(x...)   sys_debug(LOG_ERROR, LOG_TAG, x)

#define SLOGW(x...)   sys_debug(LOG_WARNING, LOG_TAG, x)
#define SLOGE(x...)   sys_debug(LOG_ERROR, LOG_TAG, x)


#endif

