#ifndef _UART_PROTOCOL_COMM_H
#define _UART_PROTOCOL_COMM_H
#include <type_def.h>

#define UART_BUFFER_SIZE  45
#define UART_HEADER_CHAR 0xAA

#define UART_MSG_CTRL 0x02
#define UART_MSG_ST_REQ 0x03
#define UART_MSG_ST_UPLOAD 0x04
#define UART_MSG_INFO_UPLOAD 0x06
#define UART_MSG_EXCEPTION 0x0A
#define UART_MSG_SN_REQ 0x07
#define UART_MSG_SN_WRITE 0x11
#define UART_MSG_CLR_CONFIG 0x83
#define UART_MSG_RSSI_REQ 0x63
#define UART_MSG_WORK_SWITCH 0x64
#define UART_MSG_RSSI_SEND 0x0D
#define UART_MSG_DEV_INFO 0xA0

typedef enum{

  UART_FRAME_HEADER,
  UART_FRAME_LEN,
  UART_MACHINE_TYPE,
  UART_FRAME_CRC,
  UART_FRAME_RESERVE1,
  UART_FRAME_RESERVE2,
  UART_MSG_ID,
  UART_FRAME_VERSION,
  UART_MACHINE_VERSION,
  UART_MSG_TYPE,
  UART_MSG_TEXT,
  UART_MSG_CRC,
  UART_FRAME_END,
  
} uart_state_t;

typedef struct
{
    uint8_t				    head;
	uint8_t					len;
	uint8_t					device_type;
	uint8_t	 				frame_crc;
	uint8_t                 Rev[2];
	uint8_t					msg_id;
	uint8_t					frame_ver;
	uint8_t					device_ver;
	uint8_t					msg_type;
	uint8_t 				msg_buff[UART_BUFFER_SIZE];
	uint8_t					crc;
} uart_frame_t;

//void uart1_write(A_CHAR* buff, A_UINT32* buff_len);
//void uart_thread(void);

#endif

