#ifndef __LOG_EXT_H
#define __LOG_EXT_H
#include <stdio.h>
#include <log_util.h>

#define C_Black   0;30
#define C_Red     0;31
#define C_Green   0;32
#define C_Brown   0;33
#define C_Blue    0;34
#define C_Purple  0;35
#define C_Cyan    0;36

#define log_debug(format, ...) \
    do{\
        if (1) {\
            printf("\033[1;32m[DEBUG][%s][%s(%d)]: " format "\r\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);\
            fflush(stdout);\
            printf("\033[0m"); \
        }\
    } while(0)

#define log_info(format, ...) \
	do{\
		if (1) {\
			printf("\033[1;36m[INFO][%s][%s(%d)]: " format "\r\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);\
			fflush(stdout);\
			printf("\033[0m"); \
		}\
	} while(0)

#define log_warn(format, ...) \
	do{\
		if (1) {\
			printf("\033[1;33m[WARN][%s][%s(%d)]: " format "\r\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);\
			fflush(stdout);\
			printf("\033[0m"); \
		}\
	} while(0)

#define log_error(format, ...) \
	do{\
		if (1) {\
			printf("\033[0;31m[ERROR][%s][%s(%d)]: " format "\r\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);\
			fflush(stdout);\
			printf("\033[0m"); \
			abort(); \
		}\
	} while(0)

#endif /* __LOG_EXT_H */

