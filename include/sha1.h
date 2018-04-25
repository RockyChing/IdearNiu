#ifndef _SHA1_H_
#define	_SHA1_H_

#include <sys/types.h>
#include <stdint.h>

#ifdef USE_MINGW
typedef unsigned char u_char;
typedef unsigned int uint32_t;
typedef unsigned int u_int32_t;
typedef unsigned int u_int;

#else
#include <sys/cdefs.h>
#endif

#define SHA1_DIGEST_LENGTH		20
#define SHA1_DIGEST_STRING_LENGTH	41

typedef struct {
	uint32_t state[5];
	uint32_t count[2];
	u_char buffer[64];
} SHA1_CTX;

void SHA1_digest(const u_char *buf, u_char digest[20], u_int len);

#endif /* _SYS_SHA1_H_ */


