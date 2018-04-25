#ifndef _BASE_64_H
#define _BASE_64_H

/**
 * BASE64_ANDROID = 1, from base64.java in Android
 */
#define BASE64_ANDROID 0
#if !BASE64_ANDROID
/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

unsigned char * base64_encode(const unsigned char *src, size_t len,
			      size_t *out_len);
unsigned char * base64_decode(const unsigned char *src, size_t len,
			      size_t *out_len);

#else
/**
 * Default values for encoder/decoder flags.
 */
#define BASE64_DEFAULT 0

/**
 * Encoder flag bit to omit the padding '=' characters at the end
 * of the output (if any).
 */
#define BASE64_NO_PADDING 1

/**
 * Encoder flag bit to omit all line terminators (i.e., the output
 * will be on one long line).
 */
#define BASE64_NO_WRAP 2

/**
 * Encoder flag bit to indicate lines should be terminated with a
 * CRLF pair instead of just an LF.  Has no effect if {@code
 * NO_WRAP} is specified as well.
 */
#define BASE64_CRLF 4

/**
 * Encoder/decoder flag bit to indicate using the "URL and
 * filename safe" variant of Base64 (see RFC 3548 section 4) where
 * {@code -} and {@code _} are used in place of {@code +} and
 * {@code /}.
 */
#define BASE64_URL_SAFE 8

/**
 * Flag to pass to {@link Base64OutputStream} to indicate that it
 * should not close the output stream it is wrapping when it
 * itself is closed.
 */
#define BASE64_NO_CLOSE 16


typedef unsigned char byte;
typedef int bool;
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

extern int base64_encode(byte input[], int len, byte output[], int flags);
extern int base64_decode(byte input[], int len, byte output[], int flags);
#endif
#endif
