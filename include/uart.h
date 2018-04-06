#ifndef __UART_H
#define __UART_H

#define DEFAULT_UART_SPEED (9600)
#define DEFAULT_UART_NAME  "/dev/ttyUSB0"


enum ComDataBits {
    COM_DATA_5_BITS = 5,
    COM_DATA_6_BITS = 6,
    COM_DATA_7_BITS = 7,
    COM_DATA_8_BITS = 8,
};

enum ComParity {
    COM_PARITY_NONE = 0,
    COM_PARITY_ODD  = 1,
    COM_PARITY_EVEN = 2,
};

enum ComStopBits {
    COM_STOP_1_BITS   = 0,
    COM_STOP_1_5_BITS = 1,
    COM_STOP_2_BITS   = 2,
};


int uart_open(const char *uart_name);
void uart_close(int fd);
//int uart_write(int fd, const char *buff, size_t len);








#endif

