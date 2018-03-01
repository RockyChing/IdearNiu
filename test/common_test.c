#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include <type_def.h>
#include <log_util.h>
#include <aes_cbc.h>
#include <base64.h>

typedef struct _framectrl_80211 {
    //buf[0]
    uint8_t Protocol:2;
    uint8_t Type:2;
    uint8_t Subtype:4;
    //buf[1]
    uint8_t ToDS:1;
    uint8_t FromDS:1;
    uint8_t MoreFlag:1;
    uint8_t Retry:1;
    uint8_t PwrMgmt:1;
    uint8_t MoreData:1;
    uint8_t Protectedframe:1;
    uint8_t Order:1;
} framectrl_80211, *pframectrl_80211;

static void aes_test()
{
	const uint8_t *plaintext = "*** This is AES CBC mode test***";
	const uint32_t PLAIN_TEXT_LEN = (uint32_t) strlen((const char *)plaintext);
	const uint8_t key[] = { 0x10, 0xa5, 0x88, 0x69, 0xd7, 0x4b, 0xe5, 0xa3,
							0x74, 0xcf, 0x86, 0x7c, 0xfb, 0x47, 0x38, 0x59 };
	const uint8_t aes_iv[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	uint8_t codetext[1024] = { 0 };
	uint8_t decodetext[1024] = { 0 };
	int ret = -1;
	int i;

	printf("plaintext:\t%s\n", plaintext);
	ret = aes_encrypt(key, sizeof(key), aes_iv, plaintext, strlen(plaintext), codetext, PLAIN_TEXT_LEN + 16);
	assert_return(ret != -1);
	printf("codetext:\t");
	for (i = 0; i < PLAIN_TEXT_LEN + 16; i ++) {
		printf("%c", codetext[i]);
	}
	printf("\n");

	ret = aes_decrypt(key, sizeof(key), aes_iv, codetext, ret, decodetext, PLAIN_TEXT_LEN + 16);
	assert_return(ret != -1);
	//printf("decodetext:\t%s\n", decodetext);

	printf("decodetext:\t");
	for (i = 0; i < PLAIN_TEXT_LEN; i ++) {
		printf("%c", decodetext[i]);
	}
	printf("\n");
}

void common_test()
{
	func_enter();
	int i;
	char buf[5] = { '0', '1', '2', '3', '4'};
	for (i = 0; i < 5; ++i) {
		printf("%c", buf[i]);
	}

	// result: 'sizeof: 2 bytes'
	//sys_debug(0, "sizeof: %d bytes\n", sizeof(struct _framectrl_80211));
	//aes_test();

	func_exit();
}

