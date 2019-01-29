#ifndef _LINUX_TYPEDEF_H_
#define _LINUX_TYPEDEF_H_
#include <sys/types.h>

#ifndef NULL
#define NULL (void *)0
#endif

typedef char  s8;
typedef short s16;
typedef long  s32;
typedef long long s64;

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned long  u32;
typedef unsigned long long u64;

typedef short int16_t;

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;


typedef s32 status_t;
typedef s64 nsecs_t;       // nano-seconds

#define STR_SPLIT_CNT      32
#define STR_SPLIT_SUB_LEN  32
#define STR_ERR_NO         0 // parase ok
#define STR_ERR_NULL       1 // parameter is NULL
#define STR_ERR_LEN        2 // parameter len too long
#define STR_ERR_OUT_INDEX  3
#define STR_ERR_IGNOR      4
typedef struct _str_split_result {
    char sub[STR_SPLIT_CNT][STR_SPLIT_SUB_LEN];
    int  count;
    int  err;
} str_split_result_t;


//#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *) ( (char *)__mptr - offsetof(type, member) );})

#endif /* _LINUX_TYPES_H_ */


