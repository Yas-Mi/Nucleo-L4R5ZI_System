
#ifndef I2C_WRAPPER_H_
#define I2C_WRAPPER_H_

#include "stm32l4xx.h"

// チャネル定義
typedef enum {
	I2C_CH1,		// LCDとの通信で使用
	I2C_CH2,		// PCM3060との通信で使用
	I2C_CH_MAX,
} I2C_CH;

// 公開関数
extern void i2c_wrapper_init(void);
extern int32_t i2c_wrapper_open(I2C_CH ch);
extern int32_t i2c_wrapper_send(I2C_CH ch, uint8_t slave_addr, uint8_t *data, uint32_t size);

#endif /* I2C_WRAPPER_H_ */
