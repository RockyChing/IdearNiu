#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <type_def.h>
#include <uart_protocol_common.h>

uart_frame_t g_uart_frame;


int byte2hexstr(const uint8_t *pBytes, int srcLen, uint8_t *pDstStr, int dstLen)
{
	const char tab[] = "0123456789abcdef";
	int i = 0;

	memset(pDstStr, 0, dstLen);

	if (dstLen < srcLen * 2)
		srcLen = (dstLen - 1) / 2;

	for (i = 0; i < srcLen; i++)
	{
		*pDstStr++ = tab[*pBytes >> 4];
		*pDstStr++ = tab[*pBytes & 0x0f];
		pBytes++;
	}
	*pDstStr++ = 0;
	return srcLen * 2;
}

void uart_data_send_out()
{

	uart_frame_t  *uart_rx_buff = (uart_frame_t*)malloc(g_uart_frame.len + 2);
	memcpy(uart_rx_buff, &g_uart_frame, g_uart_frame.len + 1);
	uint8_t msg_type = uart_rx_buff->msg_type;
	uart_rx_buff->msg_buff[uart_rx_buff->len-10] = g_uart_frame.crc;
	//uint32_t len = uart_rx_buff->len + 1;
	uint8_t data[200] = {0};
	printf("uart_data_send_out \r\n");
	
	byte2hexstr((uint8_t*)uart_rx_buff, uart_rx_buff->len+1, (uint8_t*)data, 100);
	printf("read uart1->:%s\r\n", data);
	
	switch(msg_type) {
		case UART_MSG_CTRL:
		case UART_MSG_ST_REQ:
		case UART_MSG_ST_UPLOAD:
		case UART_MSG_INFO_UPLOAD:
		case UART_MSG_EXCEPTION:
		case UART_MSG_DEV_INFO:		
		case UART_MSG_SN_REQ:
		case UART_MSG_WORK_SWITCH:
		case UART_MSG_CLR_CONFIG:	
				//md_uart_send((uint8_t*)uart_rx_buff,&len);
			//	qcom_sys_reset(); 
				break;		

	}
	
	free(uart_rx_buff);
}

void uart_recv_data_handle(uint8_t *UartData , uint32_t UartLen)
{
	static int   uart_count = 0;
	static uart_state_t uart_state = UART_FRAME_HEADER;
	uint32_t i= 0;
	for(i = 0; i< UartLen ; i++) {
		switch( uart_state) {
		case UART_FRAME_HEADER:
			g_uart_frame.crc = 0;
			uart_count = 0;
			if (UartData[i] == UART_HEADER_CHAR) {	 
				g_uart_frame.head = UartData[i] ;
				uart_state = UART_FRAME_LEN;
			}
			break;

		case UART_FRAME_LEN:
			g_uart_frame.len = UartData[i];
			g_uart_frame.crc += UartData[i];
			if (UartData[i] < 10) 
				uart_state = UART_FRAME_HEADER;
			else
				uart_state = UART_MACHINE_TYPE;
			break;

		case UART_MACHINE_TYPE:
			g_uart_frame.device_type = UartData[i];
			g_uart_frame.crc += UartData[i];
			uart_state = UART_FRAME_CRC;
			break;
		 
		case UART_FRAME_CRC:
			g_uart_frame.frame_crc = UartData[i];
			g_uart_frame.crc += UartData[i];
			uart_state = UART_FRAME_RESERVE1;
			break;

		case UART_FRAME_RESERVE1:
			g_uart_frame.Rev[0]= UartData[i];
			g_uart_frame.crc += UartData[i];
			uart_state = UART_FRAME_RESERVE2;
			break;

		case UART_FRAME_RESERVE2:
			g_uart_frame.Rev[1]= UartData[i];
			g_uart_frame.crc += UartData[i];
			uart_state = UART_MSG_ID;
			break;

		case UART_MSG_ID:
			g_uart_frame.msg_id = UartData[i];
			g_uart_frame.crc += UartData[i];
			uart_state = UART_FRAME_VERSION;
			break;

		case UART_FRAME_VERSION:
			g_uart_frame.frame_ver = UartData[i];
			g_uart_frame.crc += UartData[i];
			uart_state = UART_MACHINE_VERSION;
			break;

		case UART_MACHINE_VERSION:
			g_uart_frame.device_ver = UartData[i];
			g_uart_frame.crc += UartData[i];
			uart_state = UART_MSG_TYPE;
			break;

		case UART_MSG_TYPE:
			g_uart_frame.msg_type= UartData[i];
			g_uart_frame.crc += UartData[i];
			if((g_uart_frame.len -10) > 0)
				uart_state = UART_MSG_TEXT;
			else
				uart_state = UART_MSG_CRC;
			break;

		case UART_MSG_TEXT:
			g_uart_frame.msg_buff[uart_count++]= UartData[i];
			g_uart_frame.crc += UartData[i];
			if(uart_count ==( g_uart_frame.len -10))
				uart_state = UART_MSG_CRC;
			break;

		case UART_MSG_CRC:
			g_uart_frame.crc = ~g_uart_frame.crc;
			g_uart_frame.crc += 1;
			if(g_uart_frame.crc == ((uint8_t)UartData[i])) {
				uart_data_send_out();
			}
			uart_state = UART_FRAME_HEADER;
			break;
		default:
			uart_state = UART_FRAME_HEADER;
			break;
	 	}
	}
}

