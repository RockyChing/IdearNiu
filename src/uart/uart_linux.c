#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/types.h>

#include <uart.h>
#include <log_util.h>
#include <utils.h>
#include <type_def.h>


#define loge(x...) sys_debug(1, x)

struct BaudAlias {
    int baud;
    int baudalias;
};

static const struct BaudAlias BaudArray[] = {
    { 0,      B0 },
    { 50,     B50 },
    { 75,     B75 },
    { 110,    B110 },
    { 134,    B134 },
    { 150,    B150 },
    { 200,    B200 },
    { 300,    B300 },
    { 600,    B600 },
    { 1200,   B1200 },
    { 1800,   B1800 },
    { 2400,   B2400 },
    { 4800,   B4800 },
    { 9600,   B9600 },
    { 19200,  B19200 },
    { 38400,  B38400 },
    { 57600,  B57600 },
    { 115200, B115200 },
    { 230400, B230400 },
};


static int get_baud_alias(int speed)
{
	short i;
	for (i = NUM_ELEMENTS(BaudArray) - 1; i >= 0; i --) {
		if (speed == BaudArray[i].baud)
			return BaudArray[i].baudalias;
	}

	return 0;
}

static int uart_init(int fd, int baudrate,
					int data_bits, int data_parity, int stop_bits)
{
	int fd_flags;
	assert_param(fd > 0);
	assert_param(data_bits == COM_DATA_5_BITS || data_bits == COM_DATA_6_BITS ||
				 data_bits == COM_DATA_7_BITS || data_bits == COM_DATA_8_BITS);
	assert_param(data_parity == COM_PARITY_NONE ||
		         data_parity == COM_PARITY_ODD || data_parity == COM_PARITY_EVEN);
	assert_param(stop_bits == COM_STOP_1_BITS ||
		         stop_bits == COM_STOP_1_5_BITS || stop_bits == COM_STOP_2_BITS);

	/**
	 * F_GETFL: Return (as  the  function  result) the file access mode and the file status flags
	 * F_SETFL: Set the file status flags to the value specified by  arg.
	 *          On  Linux,  this  command can change only the O_APPEND, O_ASYNC,
                O_DIRECT, O_NOATIME, and O_NONBLOCK flags.  It is  not  possible
                to change the O_DSYNC and O_SYNC flags!
     * On error, -1 is returned, and errno is set appropriately.
	 */
	fd_flags = fcntl(fd, F_GETFL);
	assert_return(fd_flags != -1);
	assert_return(fcntl(fd, F_SETFL, fd_flags & ~O_NONBLOCK) != -1);
	struct termios options;

	/**
     * Return 0 on success, -1 on failure and set errno to indicate the error
     */
    if (tcgetattr(fd, &options) != 0) {
        sys_debug(1, "Error: terminal tcgetattr");
        return -1;
    }

	int baud_alias = get_baud_alias(baudrate);
	assert_return(baud_alias);
	cfsetispeed(&options,  baud_alias);
    cfsetospeed(&options,  baud_alias);

	switch (data_bits) {
	case COM_DATA_8_BITS:
		options.c_cflag |= CS8;
		break;
	case COM_DATA_5_BITS:
		options.c_cflag |= CS5;
		break;
	case COM_DATA_6_BITS:
		options.c_cflag |= CS6;
		break;
	case COM_DATA_7_BITS:
		options.c_cflag |= CS7;
		break;
	default:
        loge("Error: Unsupported terminal data bits!\n");
        return -1;
	}

	switch (data_parity) {
	case COM_PARITY_NONE:
		options.c_cflag &= ~PARENB;
        options.c_iflag &= ~INPCK;
		break;
	case COM_PARITY_ODD:
		options.c_cflag |= (PARODD | PARENB);
        options.c_iflag |= INPCK;
		break;
	case COM_PARITY_EVEN:
		options.c_cflag |= PARENB;
        options.c_cflag &= ~PARODD;
        options.c_iflag |= INPCK;
		break;
	default:
        loge("Error: Unsupported terminal parity!\n");
        return -1;
	}

	switch (stop_bits) {
	case COM_STOP_1_BITS:
	case COM_STOP_1_5_BITS:
		options.c_cflag &= ~CSTOPB;
		break;
	case COM_STOP_2_BITS:
		options.c_cflag |= CSTOPB;
		break;
	default:
        loge("Error: Unsupported terminal parity!\n");
        return -1;
	}

	options.c_lflag  &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG);
    options.c_oflag  &= ~(OPOST | OCRNL);
    options.c_iflag &= ~(IGNPAR | PARMRK | ISTRIP | IXANY | ICRNL);
    options.c_iflag &= ~(IXON | IXOFF | BRKINT);
    options.c_cc[VTIME] = 0;
    options.c_cc[VMIN] = 1;

    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        loge("Error: terminal tcsetattr");
        return -1;
    }

    return 0;
}

/**
 * An open interface with default configuration, 9600, 8N1
 * On sucess, return uart fd
 */
int uart_open(const char *uart_name)
{
	if (!uart_name)
		uart_name = DEFAULT_UART_SPEED;

	int fd = open(uart_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	assert_return(fd > 0);

	/**
     * isatty() returns 1 if fd is an open file descriptor referring to a terminal;
     * otherwise 0 is returned, and errno is set to indicate the error
     */
    if (!isatty(fd)) {
        loge("Error: %s is not a terminal", uart_name);
		return -1;
    }

	assert_return(uart_init(fd, DEFAULT_UART_SPEED,
			COM_DATA_8_BITS, COM_PARITY_NONE, COM_STOP_1_BITS) == 0);

	sys_debug(3, "DEBUG: %s open done, fd = %d", uart_name, fd);
	return fd;
}

void uart_close(int fd)
{
	close(fd);
}

/*
 * Write len bytes from buf to the socket.
 * Returns the return value from send()
 */
int uart_write(int fd, const char *buff, int len)
{
	int t;
	assert_param(fd > 0);
	assert_param(buff != NULL);
	assert_param(len > 0);

	for(t = 0 ; len > 0 ; ) {
		int n = write(fd, buff + t, len);
		if (n < 0) {
			if (is_recoverable(errno))
				continue;
			sys_debug(1, "ERROR: uart write() error: %s", strerror(errno));
		    return (t == 0) ? n : t;
		}
		t += n;
		len -= n;
	}

	return t;
}

static int uart_dump(const char *prefix, const uint8_t *ptr, uint32_t length)
{
    char buffer[100] = {'\0'};
    u32  offset = 01;
    int  i;

	assert_param(prefix != NULL);
	assert_param(ptr != NULL);
	assert_param(length > 0);

	// lock; // must put a lock here
    while (offset < length) {
        int off;
        strcpy(buffer, prefix);
        off = strlen(buffer);
        ASSERT(snprintf(buffer + off, sizeof(buffer) - off, "%08x: ", offset));
        off = strlen(buffer);

        for (i = 0; i < 16; i ++) {
            if (offset + i < length) {
                ASSERT(snprintf(buffer + off, sizeof(buffer) - off, "%02x%c", ptr[offset + i], i == 7 ? '-' : ' '));
            } else {
                ASSERT(snprintf(buffer + off, sizeof(buffer) - off, " .%c", i == 7 ? '-' : ' '));
            }
            off = strlen(buffer);
        }

        ASSERT(snprintf(buffer + off, sizeof(buffer) - off, " "));
		off = strlen(buffer);
		for (i = 0; i < 16; i++)
			if (offset + i < length) {
				ASSERT(snprintf(buffer + off, sizeof(buffer) - off, "%c", (ptr[offset + i] < ' ') ? '.' : ptr[offset + i]));
				off = strlen(buffer);
			}

        offset += 16;
		printf("== %s", buffer);
    }

	// unlock; 
    return 0;
}
