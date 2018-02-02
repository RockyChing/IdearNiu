#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <base64.h>

/**
 * Lookup table for turning Base64 alphabet positions (6 bits)
 * into output bytes.
 */
static const byte ENCODE[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/',
};

/**
 * Lookup table for turning Base64 alphabet positions (6 bits)
 * into output bytes.
 */
static const byte ENCODE_WEBSAFE[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_',
};

#define ENCODE_SIZE (sizeof (ENCODE))
/**
 * Emit a new line every this many output tuples.  Corresponds to
 * a 76-character line length (the maximum allowable according to
 * <a href="http://www.ietf.org/rfc/rfc2045.txt">RFC 2045</a>).
 */
#define LINE_GROUPS 19

byte tail[1024];
int tail_len = 0;
bool do_padding, do_newline, do_cr;

void do_encode(byte input[], int len, byte output[], int flags)
{
	int count = do_newline ? LINE_GROUPS : -1;
	int op = 0;
	int p = 0;
	int v = -1;
	const byte *alphabet = ((flags & BASE64_URL_SAFE) == 0) ? &ENCODE[0] : &ENCODE_WEBSAFE[0];

	// The main loop, turning 3 input bytes into 4 output bytes on
	// each iteration.
	while ((p + 3) <= len) {
		v = ((input[p] & 0xff) << 16) |
			((input[p+1] & 0xff) << 8) |
			(input[p+2] & 0xff);
		output[op] = alphabet[(v >> 18) & 0x3f];
		output[op+1] = alphabet[(v >> 12) & 0x3f];
		output[op+2] = alphabet[(v >> 6) & 0x3f];
		output[op+3] = alphabet[v & 0x3f];
		p += 3;
		op += 4;
		if (--count == 0) {
			if (do_cr) output[op++] = '\r';
			output[op++] = '\n';
			count = LINE_GROUPS;
		}
	}

	/**
	 * Finish up the tail of the input.  Note that we need to
	 * consume any bytes in tail before any bytes
	 * remaining in input; there should be at most two bytes
	 * total.
	 */
	if (p-tail_len == len-1) {
		int t = 0;
		v = ((tail_len > 0 ? tail[t++] : input[p++]) & 0xff) << 4;
		tail_len -= t;
		output[op++] = alphabet[(v >> 6) & 0x3f];
		output[op++] = alphabet[v & 0x3f];
		if (do_padding) {
			output[op++] = '=';
			output[op++] = '=';
		}
		if (do_newline) {
			if (do_cr) output[op++] = '\r';
			output[op++] = '\n';
		}
	} else if (p-tail_len == len-2) {
		int t = 0;
		v = (((tail_len > 1 ? tail[t++] : input[p++]) & 0xff) << 10) |
			(((tail_len > 0 ? tail[t++] : input[p++]) & 0xff) << 2);
		tail_len -= t;
		output[op++] = alphabet[(v >> 12) & 0x3f];
		output[op++] = alphabet[(v >> 6) & 0x3f];
		output[op++] = alphabet[v & 0x3f];
		if (do_padding) {
			output[op++] = '=';
		}
		if (do_newline) {
			if (do_cr) output[op++] = '\r';
			output[op++] = '\n';
		}
	} else if (do_newline && op > 0 && count != LINE_GROUPS) {
		if (do_cr) output[op++] = '\r';
		output[op++] = '\n';
	}
}

/**
 * Base64-encode the given data and return the encode data len
 * @param input  the data to encode
 * @param flags  controls certain features of the encoded output.
 *               Passing {@code DEFAULT} results in output that
 *               adheres to RFC 2045.
 */
int base64_encode(byte input[], int len, byte output[], int flags)
{
	int output_len = 0;
	do_padding = (flags & BASE64_NO_PADDING) == 0;
	do_newline = (flags & BASE64_NO_WRAP) == 0;
	do_cr = (flags & BASE64_CRLF) != 0;

	/** Compute the exact length of the array we will produce */
	output_len = len / 3 * 4;
	if (do_padding) {
		if (len % 3 > 0) {
			output_len += 4;
		}
	} else {
		switch (len % 3) {
			case 0: break;
			case 1: output_len += 2; break;
			case 2: output_len += 3; break;
		}
	}

	/** Account for the newlines, if any */
	if (do_newline && len > 0) {
		output_len += (((len-1) / (3 * LINE_GROUPS)) + 1) *
			(do_cr ? 2 : 1);
	}

	do_encode(input, len, output, flags);
	return output_len;
}




// http://www.atool.org/base64.php

















