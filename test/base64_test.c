#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <base64.h>
#include <type_def.h>
#include <utils.h>
#include <log_util.h>


/**
 * Test refor:
 * http://www.atool.org/base64.php
 */
static void base64_test()
{
	const uint8_t *data = "Emit this is don't instantiate, 20171123_idearniu, Bai Nian Gu Du 0"
						  "Emit this is don't instantiate, 20171123_idearniu, Bai Nian Gu Du 1"
						  "Emit this is don't instantiate, 20171123_idearniu, Bai Nian Gu Du 2"
						  "Emit this is don't instantiate, 20171123_idearniu, Bai Nian Gu Du 3";
	size_t out_len;
	int i;
#if !BASE64_ANDROID
	uint8_t *encode = base64_encode(data, strlen(data), &out_len);
	uint8_t *decode = NULL;

	assert_return(encode != NULL);

	for (i = 0; i < out_len; i ++) {
		printf("%c", encode[i]);
	}
	printf("\n\n");

	decode = base64_decode(encode, out_len, &out_len);
	assert_return(encode != NULL);
#else
	byte encode[2048] = { 0 };
	byte decode[2048] = { 0 };
	out_len = base64_encode((byte *) data, strlen(data), encode, BASE64_DEFAULT);

	for (i = 0; i < out_len; i ++) {
		printf("%c", encode[i]);
	}
	printf("\n\n");


	out_len = base64_decode(encode, out_len, decode, BASE64_DEFAULT);
#endif
	for (i = 0; i < out_len; i ++) {
		printf("%c", decode[i]);
	}
	printf("\n\n");

	xfree(encode);
	xfree(decode);
}

int base64_test_entry()
{
	base64_test();
	return 0;
}

