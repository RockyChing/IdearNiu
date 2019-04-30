#ifndef _UTILITY_ES_H
#define _UTILITY_ES_H
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <type_def.h>

typedef enum {
	PROCESS_R_RUNNING  = 0,
	PROCESS_S_SLEEPING = 1,
	PROCESS_D_SLEEP    = 2,
	PROCESS_T_STOPED   = 3,
	PROCESS_t_STOPED   = 4,
	PROCESS_Z_ZOMBIE   = 5,
	PROCESS_X_DEAD     = 6,
	PROCESS_U_UNKNOWN  = 0xFF
} process_stat_t;

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) :(b))
#endif

#define NUM_ELEMENTS(arr) (sizeof(arr) / sizeof(arr[0]))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define SET_NONBLOCKING(fd) {           \
        int flags = fcntl(fd, F_GETFL); \
        flags |= O_NONBLOCK;            \
        fcntl(fd, F_SETFL, flags);      \
	}

#define SET_BLOCKING(fd) {              \
        int flags = fcntl(fd, F_GETFL); \
        flags &= ~O_NONBLOCK;           \
        fcntl(fd, F_SETFL, flags);      \
	}

#define dump_hex(buf, len)	\
	do { \
		int i; \
		char *p = (char*) buf; \
		for(i = 0; i < len; i++) { \
			if(0 == (i % 32) && 0 != i) \
				printf("\n"); \
			printf("%02x ", (p[i]&0xff)); \
		} \
		printf("\n"); \
	} while(0)

#define TOK_MAX_SZ	(32)
#define TOK_MAX_CNT (32)

typedef int (*cmd_callback) (char *buff, size_t len);

/**
 * If @ptr is NULL, no operation is performed.
 */
#define xfree(ptr) do { free(ptr); \
		ptr = NULL; \
	} while (0)

struct token {
	char str[TOK_MAX_CNT][TOK_MAX_SZ];
	int  str_cnt;
};

void swap(long *pa, long *pb);
int is_recoverable (int error);
size_t xstrlen(const char *str);
int xstrcmp(const char *s1, const char *s2);
int xstrncmp (const char *s1, const char *s2, size_t n);
int xstrcasecmp (const char *s1, const char *s2);
int run_command(const char *cmd, cmd_callback cmd_cb);

time_t get_time();
char *get_ctime(const time_t *t);
void xsplit(struct token *tok, const char *sentence, int sep);
void *xmalloc(int size);
void *xrealloc(void *ptr, int size);
void *zmalloc(int size);
void *zrealloc(void *ptr, int size);

void sleep_us(uint32_t us);
void sleep_ms(uint32_t us);
void sleep_s(uint32_t us);
void process_stat_all(void);
process_stat_t process_stat(pid_t pid);

#define new0(type, count)			\
	(type *) (__extension__ ({		\
		size_t __n = (size_t) (count);	\
		size_t __s = sizeof(type);	\
		void *__p;			\
		__p = malloc(__n * __s);	\
		memset(__p, 0, __n * __s);	\
		__p;				\
	}))

#endif

