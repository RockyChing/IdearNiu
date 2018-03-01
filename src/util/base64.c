/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <base64.h>
#include <utils.h>

#if !BASE64_ANDROID
static const unsigned char base64_table[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * base64_encode - Base64 encode
 * @src: Data to be encoded
 * @len: Length of the data to be encoded
 * @out_len: Pointer to output length variable, or %NULL if not used
 * Returns: Allocated buffer of out_len bytes of encoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer. Returned buffer is
 * nul terminated to make it easier to use as a C string. The nul terminator is
 * not included in out_len.
 */
unsigned char * base64_encode(const unsigned char *src, size_t len,
			      size_t *out_len)
{
	unsigned char *out, *pos;
	const unsigned char *end, *in;
	size_t olen;
	int line_len;

	olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
	olen += olen / 72; /* line feeds */
	olen++; /* nul termination */
	if (olen < len)
		return NULL; /* integer overflow */
	out = xmalloc(olen);
	if (out == NULL)
		return NULL;

	end = src + len;
	in = src;
	pos = out;
	line_len = 0;
	while (end - in >= 3) {
		*pos++ = base64_table[(in[0] >> 2) & 0x3f];
		*pos++ = base64_table[(((in[0] & 0x03) << 4) |
				       (in[1] >> 4)) & 0x3f];
		*pos++ = base64_table[(((in[1] & 0x0f) << 2) |
				       (in[2] >> 6)) & 0x3f];
		*pos++ = base64_table[in[2] & 0x3f];
		in += 3;
		line_len += 4;
		if (line_len >= 72) {
			*pos++ = '\n';
			line_len = 0;
		}
	}

	if (end - in) {
		*pos++ = base64_table[(in[0] >> 2) & 0x3f];
		if (end - in == 1) {
			*pos++ = base64_table[((in[0] & 0x03) << 4) & 0x3f];
			*pos++ = '=';
		} else {
			*pos++ = base64_table[(((in[0] & 0x03) << 4) |
					       (in[1] >> 4)) & 0x3f];
			*pos++ = base64_table[((in[1] & 0x0f) << 2) & 0x3f];
		}
		*pos++ = '=';
		line_len += 4;
	}

	if (line_len)
		*pos++ = '\n';

	*pos = '\0';
	if (out_len)
		*out_len = pos - out;
	return out;
}


/**
 * base64_decode - Base64 decode
 * @src: Data to be decoded
 * @len: Length of the data to be decoded
 * @out_len: Pointer to output length variable
 * Returns: Allocated buffer of out_len bytes of decoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
unsigned char * base64_decode(const unsigned char *src, size_t len,
			      size_t *out_len)
{
	unsigned char dtable[256], *out, *pos, block[4], tmp;
	size_t i, count, olen;
	int pad = 0;

	memset(dtable, 0x80, 256);
	for (i = 0; i < sizeof(base64_table) - 1; i++)
		dtable[base64_table[i]] = (unsigned char) i;
	dtable['='] = 0;

	count = 0;
	for (i = 0; i < len; i++) {
		if (dtable[src[i]] != 0x80)
			count++;
	}

	if (count == 0 || count % 4)
		return NULL;

	olen = count / 4 * 3;
	pos = out = xmalloc(olen);
	if (out == NULL)
		return NULL;

	count = 0;
	for (i = 0; i < len; i++) {
		tmp = dtable[src[i]];
		if (tmp == 0x80)
			continue;

		if (src[i] == '=')
			pad++;
		block[count] = tmp;
		count++;
		if (count == 4) {
			*pos++ = (block[0] << 2) | (block[1] >> 4);
			*pos++ = (block[1] << 4) | (block[2] >> 2);
			*pos++ = (block[2] << 6) | block[3];
			count = 0;
			if (pad) {
				if (pad == 1)
					pos--;
				else if (pad == 2)
					pos -= 2;
				else {
					/* Invalid padding */
					xfree(out);
					return NULL;
				}
				break;
			}
		}
	}

	*out_len = pos - out;
	return out;
}

#else
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

/**
 * Emit a new line every this many output tuples.  Corresponds to
 * a 76-character line length (the maximum allowable according to
 * <a href="http://www.ietf.org/rfc/rfc2045.txt">RFC 2045</a>).
 */
#define LINE_GROUPS 19

bool do_padding, do_newline, do_cr;

void do_encode(byte input[], int len, byte output[], int flags)
{
	int count = do_newline ? LINE_GROUPS : -1;
	int op = 0;
	int p = 0;
	int v = -1;
	byte tail[1024] = { 0 };
	int tail_len = 0;
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
 * base64-encode the given data and return the encode data len
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




/**
 * Lookup table for turning bytes into their position in the
 * Base64 alphabet.
 */
static const int DECODE[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -2, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

/**
 * Decode lookup table for the "web safe" variant (RFC 3548
 * sec. 4) where - and _ replace + and /.
 */
static const int DECODE_WEBSAFE[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -2, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static int do_decode(byte input[], int len, byte output[], int flags)
{
	int op = 0, p = 0;
	/**
	 * States 0-3 are reading through the next input tuple.
	 * State 4 is having read one '=' and expecting exactly
	 * one more.
	 * State 5 is expecting no more data or padding characters
	 * in the input.
	 * State 6 is the error state; an error has been detected
	 * in the input and no future input can "fix" it.
	 */
	int state = 0;   // state number (0 to 6)
	int value = 0;
	/** Non-data values in the DECODE arrays. */
	const int SKIP = -1;
	const int EQUALS = -2;
	const int *alphabet = ((flags & BASE64_URL_SAFE) == 0) ? &DECODE[0] : &DECODE_WEBSAFE[0];

	while (p < len) {
        // Try the fast path:  we're starting a new tuple and the
        // next four bytes of the input stream are all data
        // bytes.  This corresponds to going through states
        // 0-1-2-3-0.  We expect to use this method for most of
        // the data.
        //
        // If any of the next four bytes of input are non-data
        // (whitespace, etc.), value will end up negative.  (All
        // the non-data values in decode are small negative
        // numbers, so shifting any of them up and or'ing them
        // together will result in a value with its top bit set.)
        //
        // You can remove this whole block and the output should
        // be the same, just slower.
        if (state == 0) {
            while (p+4 <= len &&
                   (value = ((alphabet[input[p] & 0xff] << 18) |
                             (alphabet[input[p+1] & 0xff] << 12) |
                             (alphabet[input[p+2] & 0xff] << 6) |
                             (alphabet[input[p+3] & 0xff]))) >= 0) {
                output[op+2] = (byte) value;
                output[op+1] = (byte) (value >> 8);
                output[op] = (byte) (value >> 16);
                op += 3;
                p += 4;
            }
            if (p >= len) break;
        }

        // The fast path isn't available -- either we've read a
        // partial tuple, or the next four input bytes aren't all
        // data, or whatever.  Fall back to the slower state
        // machine implementation.
        int d = alphabet[input[p++] & 0xff];
        switch (state) {
        case 0:
            if (d >= 0) {
                value = d;
                ++state;
            } else if (d != SKIP) {
                state = 6;
                return false;
            }
            break;

        case 1:
            if (d >= 0) {
                value = (value << 6) | d;
                ++state;
            } else if (d != SKIP) {
                state = 6;
                return false;
            }
            break;

        case 2:
            if (d >= 0) {
                value = (value << 6) | d;
                ++state;
            } else if (d == EQUALS) {
                // Emit the last (partial) output tuple;
                // expect exactly one more padding character.
                output[op++] = (byte) (value >> 4);
                state = 4;
            } else if (d != SKIP) {
                state = 6;
                return false;
            }
            break;

        case 3:
            if (d >= 0) {
                // Emit the output triple and return to state 0.
                value = (value << 6) | d;
                output[op+2] = (byte) value;
                output[op+1] = (byte) (value >> 8);
                output[op] = (byte) (value >> 16);
                op += 3;
                state = 0;
            } else if (d == EQUALS) {
                // Emit the last (partial) output tuple;
                // expect no further data or padding characters.
                output[op+1] = (byte) (value >> 2);
                output[op] = (byte) (value >> 10);
                op += 2;
                state = 5;
            } else if (d != SKIP) {
                state = 6;
                return false;
            }
            break;

        case 4:
            if (d == EQUALS) {
                ++state;
            } else if (d != SKIP) {
                state = 6;
                return false;
            }
            break;

        case 5:
            if (d != SKIP) {
                state = 6;
                return false;
            }
            break;
        }
    }

	// Done reading input.  Now figure out where we are left in
    // the state machine and finish up.

    switch (state) {
    case 0:
        // Output length is a multiple of three.  Fine.
        break;
    case 1:
        // Read one extra input byte, which isn't enough to
        // make another output byte.  Illegal.
        state = 6;
        return false;
    case 2:
        // Read two extra input bytes, enough to emit 1 more
        // output byte.  Fine.
        output[op++] = (byte) (value >> 4);
        break;
    case 3:
        // Read three extra input bytes, enough to emit 2 more
        // output bytes.  Fine.
        output[op++] = (byte) (value >> 10);
        output[op++] = (byte) (value >> 2);
        break;
    case 4:
        // Read one padding '=' when we expected 2.  Illegal.
        state = 6;
        return false;
    case 5:
        // Read all the padding '='s we expected and no more.
        // Fine.
        break;
    }

	return len * 3 / 4;
}

/**
 * Decode the Base64-encoded data in input and return the data in
 * a new byte array.
 *
 * <p>The padding '=' characters at the end are considered optional, but
 * if any are present, there must be the correct number of them.
 *
 * @param input  the data to decode
 * @param len    the number of bytes of input to decode
 * @param flags  controls certain features of the decoded output.
 *               Pass {@code DEFAULT} to decode standard Base64.
 *
 * @throws IllegalArgumentException if the input contains
 * incorrect padding
 */
int base64_decode(byte input[], int len, byte output[], int flags)
{
	return do_decode(input, len, output, flags);
}
#endif
