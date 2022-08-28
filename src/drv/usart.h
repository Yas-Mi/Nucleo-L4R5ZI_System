
#ifndef USART_H_
#define USART_H_

#include "stm32l4xx.h"

#define USART1 (0)
#define USART2 (1)
#define USART3 (2)

typedef void (*USART_CALLBACK)(uint32_t ch);

extern uint32_t usart_init(uint32_t ch, USART_CALLBACK callback);
extern uint32_t usart_open(uint32_t ch);
extern uint32_t usart_send(uint32_t ch, uint8_t *data, uint32_t send_num);
extern uint32_t usart_is_send_enable(uint32_t ch);
extern uint32_t serial_send_byte(uint32_t ch, uint8_t c);
extern uint32_t usart_is_rcv_enable(uint32_t ch);
extern uint8_t usart_recv_byte(uint32_t ch);
extern uint32_t usart_intr_is_send_enable(uint32_t ch);
extern void usart_intr_send_enable(int32_t ch);
extern void usart_intr_send_disable(int32_t ch);
extern int usart_intr_is_recv_enable(int32_t ch);
extern void usart_intr_recv_enable(int32_t ch);
extern void usart_intr_recv_disable(int32_t ch);

extern void usart1_handler(void);
extern void usart2_handler(void);
extern void usart3_handler(void);

#endif /* USART_H_ */
