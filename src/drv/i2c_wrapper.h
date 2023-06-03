
#ifndef I2C_WRAPPER_H_
#define I2C_WRAPPER_H_

#include "stm32l4xx.h"

// チャネル定義
typedef enum {
	I2C_CH1,		// LCDとの通信で使用
	I2C_CH2,		// PCM3060との通信で使用
	I2C_CH_MAX,
} I2C_CH;

// 通信速度
typedef enum {
	I2C_BPS_100K,		// 100k
	I2C_BPS_400K,		// 400k
	I2C_BPS_MAX,
} I2C_BPS;

// オープンパラメータ
typedef struct {
	I2C_BPS bps;
} I2C_OPEN;

// コールバック関数定義
typedef void (*I2C_WRAPPER_SND_CALLBACK)(I2C_CH ch, int32_t ercd, void *vp);									// 送信コールバック
typedef void (*I2C_WRAPPER_RCV_CALLBACK)(I2C_CH ch, int32_t ercd, uint8_t *data, uint32_t size, void *vp);		// 受信コールバック

// 公開関数
extern void i2c_wrapper_init(void);
extern int32_t i2c_wrapper_open(I2C_CH ch, I2C_OPEN *open_par, I2C_WRAPPER_SND_CALLBACK snd_callback, I2C_WRAPPER_RCV_CALLBACK rcv_callback, void* callback_vp);
extern int32_t i2c_wrapper_send(I2C_CH ch, uint8_t slave_addr, uint8_t *data, uint32_t size);

#endif /* I2C_WRAPPER_H_ */
