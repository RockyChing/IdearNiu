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
#include <log_ext.h>
#include <utils.h>
#include <type_def.h>
#include <epoll_loop.h>
#include <assert.h>
#include "timeout.h"

struct uart_data {
	int fd;
};

static struct uart_data g_uart_data;

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

static int uart_setup(int fd, int baudrate,
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
	assert(fd_flags != -1);
	assert(fcntl(fd, F_SETFL, fd_flags & ~O_NONBLOCK) != -1);
	struct termios options;

	/**
     * Return 0 on success, -1 on failure and set errno to indicate the error
     */
    if (tcgetattr(fd, &options) != 0) {
        log_error("Error: terminal tcgetattr");
        return -1;
    }

	int baud_alias = get_baud_alias(baudrate);
	assert(baud_alias);
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
        log_error("Error: Unsupported terminal data bits!\n");
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
        log_error("Error: Unsupported terminal parity!\n");
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
        log_error("Error: Unsupported terminal parity!\n");
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
        log_error("Error: terminal tcsetattr");
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
		uart_name = DEFAULT_UART_NAME;

	int fd = open(uart_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	assert(fd > 0);

	/**
     * isatty() returns 1 if fd is an open file descriptor referring to a terminal;
     * otherwise 0 is returned, and errno is set to indicate the error
     */
    if (!isatty(fd)) {
        log_error("Error: %s is not a terminal", uart_name);
		return -1;
    }

	assert(uart_setup(fd, DEFAULT_UART_SPEED,
			COM_DATA_8_BITS, COM_PARITY_NONE, COM_STOP_1_BITS) == 0);

	log_debug("DEBUG: %s open done, fd = %d", uart_name, fd);
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
int uart_write(int fd, const char *buff, size_t len)
{
	int t;
	assert(fd > 0);
	assert(buff != NULL);
	assert(len > 0);

	//lock(xxx);
	for(t = 0 ; len > 0 ; ) {
		int n = write(fd, buff + t, len);
		if (n < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			log_error("ERROR: uart write() error: %s", strerror(errno));
			//unlock(xxx);
		    return (t == 0) ? n : t;
		}
		t += n;
		len -= n;
	}

	//unlock(xxx);
	return t;
}

static int uart_dump(const char *prefix, const uint8_t *ptr, uint32_t length)
{
    char buffer[100] = {'\0'};
    uint32_t offset = 01;
    int  i;

	assert(prefix != NULL);
	assert(ptr != NULL);
	assert(length > 0);

	// lock; // must put a lock here
    while (offset < length) {
        int off;
        strcpy(buffer, prefix);
        off = strlen(buffer);
        assert(snprintf(buffer + off, sizeof(buffer) - off, "%08x: ", offset));
        off = strlen(buffer);

        for (i = 0; i < 16; i ++) {
            if (offset + i < length) {
                assert(snprintf(buffer + off, sizeof(buffer) - off, "%02x%c", ptr[offset + i], i == 7 ? '-' : ' '));
            } else {
                assert(snprintf(buffer + off, sizeof(buffer) - off, " .%c", i == 7 ? '-' : ' '));
            }
            off = strlen(buffer);
        }

        assert(snprintf(buffer + off, sizeof(buffer) - off, " "));
		off = strlen(buffer);
		for (i = 0; i < 16; i++)
			if (offset + i < length) {
				assert(snprintf(buffer + off, sizeof(buffer) - off, "%c", (ptr[offset + i] < ' ') ? '.' : ptr[offset + i]));
				off = strlen(buffer);
			}

        offset += 16;
		printf("== %s", buffer);
    }

	// unlock; 
    return 0;
}

static void uart_read_cb(int fd, uint32_t events, void *user_data)
{
	log_debug("event: %d", events);
	char buf[128];
	int nread;

	if (events & EPOLLIN) {
		nread = read(fd, buf, sizeof(buf)-1);
		if (nread > 0) {
			dump_hex(buf, nread);
		} else if (nread == 0) {
			log_info("uart read failed: device detached");
			epoll_remove_fd(fd);
			close(fd);
		} else {
			log_warn("uart read failed: %s", strerror(errno));
		}
	} else if (events & (EPOLLERR | EPOLLHUP)) {
		/* TODO */
	} else {
		epoll_remove_fd(fd);
		close(fd);
	}
}

static void uart_destory(void *user_data)
{
	log_debug("event: %p", user_data);
	//epoll_remove_fd(fd);
	//close(fd);
}

static bool uart_timer(void *user_data)
{
	int ret;
	int fd = ((struct uart_data *) user_data)->fd;

	const uint8_t buff[] = { 0xAA, 0x0B, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0xEF };
	time_t timep = time(NULL);
	struct tm *t = localtime(&timep);
	printf("%d:%d:%02d\n", t->tm_hour, t->tm_min, t->tm_sec);
	
	ret = uart_write(fd, buff, sizeof(buff));
	if (ret != sizeof(buff)) {
		log_warn("Uart send failed");
	} else {
		log_debug("Uart send.");
	}

	return true;
}

void uart_init(void)
{
	int fd = uart_open("/dev/ttyUSB0");
	int ret;
	uint32_t events;

	memset(&g_uart_data, 0, sizeof(g_uart_data));
	if (fd > 0) {
		g_uart_data.fd = fd;
		events = EPOLLIN | EPOLLERR | EPOLLHUP;
		ret = epoll_add_fd(fd, events, uart_read_cb, NULL, uart_destory);
		if (!ret) {
			log_info("Uart add to epoll.");
			ret = timeout_add(3000, uart_timer, &g_uart_data, NULL);
			if (ret) {
				log_info("Uart timer added.");
			}
		}
	}
}

