#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include <uart.h>
#include <threads.h>
#include <log_util.h>
#include <utils.h>
#include <type_def.h>
#include <delay.h>


int uart_fd;



static void *uart_write_thread(void *arg)
{
	uint8_t buff[BUFSIZE] = {'\0'};
	int ret, send_cnt = 1, len;
	
	for (; ;) {
		sprintf((char *) buff, "%s send count %d\n", DEFAULT_UART_NAME, send_cnt);
		len = min(BUFSIZE, strlen(buff));
		ret = uart_write(uart_fd, buff, len);
		assert_return(len == ret);
		buff[len - 1] = '\0';
		sys_debug(1, "++++++++ %s ++++++++\n", buff);
		memset(buff, 0, BUFSIZE);
		send_cnt ++;
		msleep(500);
	}

	uart_close(uart_fd);
	return NULL;
}

static void *uart_read_thread(void *arg)
{
	struct timeval tv;
	fd_set rfds;
	int ret, nr, i;
	char buf[BUFSIZE] = {'\0'};;
	for (; ;) {
		bzero(&tv, sizeof(tv));
		FD_ZERO(&rfds);				
		FD_SET(uart_fd, &rfds);

		tv.tv_sec = 0;
		tv.tv_usec = 3000;
		ret = select(uart_fd + 1, &rfds, NULL, NULL, &tv);
		if (ret > 0) {
			if (FD_ISSET(uart_fd, &rfds)) {
				nr = read(uart_fd, buf, BUFSIZE);
				if (nr > 0) {
					for (i = 0; i < nr; i ++) {
						printf("%c", buf[i]);
					}
					uart_recv_data_handle((uint8_t *)buf, (uint32_t)nr);
					memset(buf, 0, BUFSIZE);
				} else if (0 == nr) {
					sys_debug(1, "uart read return 0!");
				} else {
					/* recv() error */
					if (EAGAIN == errno) {
						sys_debug(1, "recv() got EAGAIN");
					} else {
						sys_debug(1, "recv() errno, %s", strerror(errno));
					}
				}
			}
		} else if (ret == 0) {
			/* time expires, send data */
		} else {
			/* error occurs*/
			sys_debug(1, "ERROR: select() in uart_read_thread() complains: %s", strerror(errno));
			break;
		}
	}

	uart_close(uart_fd);
	return NULL;
}

void uart_test_entry()
{
	func_enter();
	uart_fd = uart_open(DEFAULT_UART_NAME);
	pthread_t uart_write_handler, uart_read_handler;
	assert_return(uart_fd > 0);

	assert_return(0 == thread_create(&uart_read_handler, uart_read_thread, NULL));
	assert_return(0 == thread_create(&uart_write_handler, uart_write_thread, NULL));

	func_exit();
}

