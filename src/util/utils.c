#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>

#include <utils.h>
#include <log_util.h>
#include <type_def.h>

#define error_null() sys_debug(1, "ERROR: %s() called with NULL!", __FUNCTION__)

/*
 * Print an error message and exit
 */
static void panic(char *str, ...)
{
	char buf[BUFSIZE] = {'\0'};
	va_list ap;

	va_start(ap, str);
	vsnprintf(buf, BUFSIZE, str, ap);
	fprintf(stderr, "panic: %s\n", buf);
	va_end (ap);
	exit(4);
}

/*
 * Print an error message
 */
static void warn(char *str, ...)
{
	char buf[BUFSIZE] = {'\0'};
	va_list ap;

	va_start(ap, str);
	vsnprintf(buf, BUFSIZE, str, ap);
	fprintf(stderr, "warn: %s\n", buf);
	va_end (ap);
}

void swap(long *pa, long *pb)
{
	*pa = *pa ^ *pb;
	*pb = *pa ^ *pb;
	*pa = *pa ^ *pb;
}

/**
 * if a process caught a signal while the
 * process was blocked in a ‘‘slow’’ system call, the system call was interrupted. The
 * system call returned an error and errno was set to EINTR.
 */
int is_recoverable (int error)
{
	if ((error == EAGAIN) || (error == EINTR) || (error == EINPROGRESS))
		return 1;

	return 0;
}

size_t xstrlen(const char *str)
{
	if (!str) {
		error_null();
		return 0;
	}
	return strlen(str);
}

int xstrcmp(const char *s1, const char *s2)
{
	if (!s1 || !s2) {
		error_null();
		return 0;
	}
	return strcmp(s1, s2);
}

int xstrncmp (const char *s1, const char *s2, size_t n)
{
	if (!s1 || !s2) {
		error_null();
		return 0;
	}
	return strncmp(s1, s2, n);
}

int xstrcasecmp (const char *s1, const char *s2)
{
	if (!s1 || !s2)	{
		error_null();
		return 0;
	}
	return strcasecmp (s1, s2);
}

time_t get_time()
{
    return time(NULL);
}

char *get_ctime(const time_t *t)
{
    char *str_time = ctime(t);
    return str_time ? str_time : "(null)";
}

/*
 * Split a string @sentence with the specified separator @sep, such as:
 * "$GPRMC,32.8,118.5" --> $GPRMC 32.8 118.5
 */
void xsplit(struct token *tok, const char *sentence, int sep)
{
	const char *str = sentence;
	char *substr = tok->str[0];
	assert_param(tok);
	assert_param(sentence);
	assert_param(sep > 0);

	while (*str) {
		if (*str == sep) {
			tok->str_cnt += 1;
			substr = tok->str[tok->str_cnt];
		} else {
			*substr++ = *str;
		}
		str++;
	}
}

/*
 * Panic on failing malloc
 */
void *xmalloc(int size)
{
	void *mem;
	if (!size)
		size ++;
	mem = malloc(size);
	if (mem == (void *)0)
		panic("Couldn't allocate memory!");
	return mem;
}

/*
 * malloc and zero mem, panic on failing
 */
void *zmalloc(int size)
{
	void *mem;
	if (!size)
		size ++;
	mem = malloc(size);
	if (mem == (void *)0)
		panic("Couldn't allocate memory!");
	else
		memset(mem, 0, size);
	return mem;
}

/*
 * Panic on failing realloc
 */
void *xrealloc(void *ptr, int size)
{
	void *mem;

	mem = realloc(ptr, size);
	if(mem == (void *)0)
		panic("Couldn't re-allocate memory!");
	return mem;
}

/*
 * realloc and zero mem, panic on failing
 */
void *zrealloc(void *ptr, int size)
{
	void *mem;

	mem = realloc(ptr, size);
	if(mem == (void *)0)
		panic("Couldn't re-allocate memory!");
	else
		memset(mem, 0, size);
	return mem;
}


/**
 * Print a hexdump from given address and length with indicated number of columns
 * @mem - data to be dumped
 * @len - data length
 * @columns - how many columns specified
 */
void hexdump(const void *mem, uint32_t len, uint8_t columns)
{
	uint32_t i, j;
	uint32_t len_roundedup = columns * ((len + columns - 1) / columns);
	// len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS);

	for (i = 0; i < len_roundedup; i++) {
		// print offset
		if (i % columns == 0)
			printf("%04x ", i);

		// print hex data
		if (i < len)
			printf("%02x ", ((char*) mem)[i] & 0xFF);
		else
			printf("   "); // filler spaces

		// print ASCII dump when we have printed the last byte on this line
		if (i % columns == (columns - 1)) {
			for (j = i - (columns - 1); j <= i; j++) {
				if (j >= len) // end of block, not really printing
					putchar(' ');
				else if (isprint(((char*) mem)[j])) // printable char
					putchar(0xFF & ((char*) mem)[j]);
				else // other char
					putchar('.');
			}
			putchar('\n');
		}
	}
}

/**
 * Convert byte buffer to hex string.
 *
 * @addr - address of byte buffer
 * @len - length of byte buffer
 * @buf - destination buffer to store hex string
 * @size - size of destination buffer
 * @flags - bit-0: space should be inserted between each hex value
 *          bit-1: ASCII representation should be appended
 * Number of bytes converted (value will be less than len if the buffer was too small)
 */
int to_hex_string(const void *addr, size_t len, char *buf, size_t size, char flags)
{
    static const char *hexdigits_upper = "0123456789ABCDEF";
    static const char *hexdigits_lower = "0123456789abcdef";
    const char *hexdigits = (flags & (1 << 2) ? hexdigits_upper : hexdigits_lower);
	int i;

    if (buf == NULL)
        return -1;

    if (size < 3)
        return 0;

    // Compute maximum number of bytes that can be printed to buf
    int max_len = 0;
    if (flags & 2)
        // We need 3 or 4 bytes when also appending ASCII
        // In the case of no-spaces we have one byte less for the space between hex and ASCII and one byte less for NUL
        max_len = (flags & 1 ? (size - 1) / 4 : (size - 2) / 3);
    else
        // We need 2 or 3 bytes when NOT appending ASCII
        max_len = (flags & 1 ? size / 3 : (size - 1) / 2);

    if (max_len == 0)
        return 0;

    // Flag that we need to terminate hex string with ellipsis
    // This decrease in number of bytes is slightly conservative
    int makeEllipsis = 0;
    if (len > max_len) {
        makeEllipsis = 1;
        len = max_len - 1;
    }

    char *pbuf = buf;
    for (i = 0; i < len; i++)
    {
        char value = ((char *) addr)[i];
        *pbuf++ = hexdigits[(value >> 4) & 0xF];
        *pbuf++ = hexdigits[value & 0xF];
        if (flags & 1)
            *pbuf++ = ' ';
    }
    if (makeEllipsis) {
        *pbuf++ = '.';
        *pbuf++ = '.';
    }

    // Optionally append ASCII representation
    if (flags & 2) {
        if (pbuf[-1] != ' ')
            *pbuf++ = ' ';
        for (i = 0; i < len; i++)
        {
            char value = ((char *) addr)[i];
            if (value < ' ' || value > 126)
                value = '.';
            *pbuf++ = value;
        }
        if (flags & 1)
            *pbuf++ = ' ';
    }
    if (flags & 1) {
        // Prepare to replace last space with the NUL terminator
        pbuf--;
    }
    *pbuf++ = 0;

    return len;
}

/**
 * Convert hexstr to byte buffer
 * hexstr is a hexdump string of hexadecimal digit pairs: "3E 0F BA" or "3E0FBA"
 *
 * @hexstr - hex dump string, possibly with spaces interlaving the hex digit pairs
 * @buf - destination byte buffer
 * @size - size of destination byte buffer
 * Number of bytes inserted into destination byte buffer. On syntax error (bad hex digit),
 * a negative number is returned, whose absolute value indicates what byte was being convereted
 */
int from_hex_string(const char *hexstr, void *out, size_t size)
{
    char *buf = (char *) out;
    char *buf_start = buf;

    while (*hexstr != 0) {
        uint8_t msb, lsb;

        while (*hexstr == ' ')
            hexstr++;

        if ( (msb = *hexstr++) == 0 )
            break;
        msb = toupper(msb);
        if ('A' <= msb)
            msb -= 'A' - 10;
        if (msb >= '0')
            msb -= '0';
        if (msb > 15)
            return -(buf - buf_start);

        if ( (lsb = *hexstr++) == 0 )
            break;
        lsb = toupper(lsb);
        if ('A' <= lsb)
            lsb -= 'A' - 10;
        if (lsb >= '0')
            lsb -= '0';
        if (lsb > 15)
            return -(buf - buf_start);

        unsigned char value = msb << 4 | lsb;
        *buf++ = value;
    };
    return buf - buf_start;
}

int run_command(const char *cmd, cmd_callback cmd_cb)
{
	if (!cmd) {
		warn("run_command() null parameter");
		return -1;
	}

	/**
	 * FILE *popen(const char *command, const char *type);
	 * int pclose(FILE *stream);
	 */
	FILE *fp = popen(cmd, "r");
	if (fp == NULL) {
		warn("popen() null return");
		return -1;
	}

	char buf[BUFSIZE] = { 0 };
	while (cmd_cb != NULL &&
		(fgets(buf, sizeof(buf) - 1, fp) != NULL)) {
		(*cmd_cb) (buf, strlen(buf));
	}

	return (pclose(fp) != -1) ? 0 : -1;
}

