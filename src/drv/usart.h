
#ifndef USART_H_
#define USART_H_

#include "stm32l4xx.h"

// チャネル定義
typedef enum {
	USART_CH1,
	USART_CH2,
	USART_CH3,
	USART_CH_MAX,
} USART_CH;

// コールバック関数定義
typedef void (*USART_CALLBACK)(USART_CH ch, void *vp);

// 公開関数
extern void usart_init(void);
extern int32_t usart_open(USART_CH ch, uint32_t baudrate);
extern int32_t usart_send(USART_CH ch, uint8_t *data, uint32_t size);
extern int32_t usart_recv(USART_CH ch, uint8_t *data, uint32_t size);
extern int32_t usart_send_for_int(USART_CH ch, uint8_t *data, uint32_t size);
extern int32_t usart_reg_recv_callback(USART_CH ch, USART_CALLBACK cb, void *vp);
extern int32_t usart_reg_send_callback(USART_CH ch, USART_CALLBACK cb, void *vp);

#endif /* USART_H_ */
