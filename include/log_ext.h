#ifndef __LOG_EXT_H
#define __LOG_EXT_H
#include <stdio.h>
#include <stdlib.h>


#define COLOR_OFF	"\x1B[0m"
#define COLOR_RED	"\x1B[0;91m"
#define COLOR_GREEN	"\x1B[0;92m"
#define COLOR_YELLOW	"\x1B[0;93m"
#define COLOR_BLUE	"\x1B[0;94m"
#define COLOR_MAGENTA	"\x1B[0;95m"
#define COLOR_BOLDGRAY	"\x1B[1;30m"
#define COLOR_BOLDWHITE	"\x1B[1;37m"

#define log_debug(format, ...) \
    do{\
        if (1) {\
            printf("[DEBUG][%s][%s(%d)]: " format "\r\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);\
            fflush(stdout);\
        }\
    } while(0)

#define log_info(format, ...) \
	do{\
		if (1) {\
			printf(COLOR_MAGENTA "[INFO][%s][%s(%d)]: " format "\r\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);\
			fflush(stdout);\
			printf(COLOR_OFF); \
		}\
	} while(0)

#define log_warn(format, ...) \
	do{\
		if (1) {\
			printf(COLOR_YELLOW "[WARN][%s][%s(%d)]: " format "\r\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);\
			fflush(stdout);\
			printf(COLOR_OFF); \
		}\
	} while(0)

#define log_error(format, ...) \
	do{\
		if (1) {\
			printf(COLOR_RED "[ERROR][%s][%s(%d)]: " format "\r\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);\
			fflush(stdout);\
			printf(COLOR_OFF); \
		}\
	} while(0)

#define log_fatal(format, ...) \
		do{\
			if (1) {\
				printf(COLOR_RED "[FATAL][%s][%s(%d)]: " format "\r\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);\
				fflush(stdout);\
				printf(COLOR_OFF); \
				abort(); \
			}\
		} while(0)

#endif /* __LOG_EXT_H */

