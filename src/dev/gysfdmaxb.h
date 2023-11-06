/*
 * gysfdmaxb.h
 *
 *  Created on: 2023/11/1
 *      Author: ronald
 */

#ifndef DEV_GYSFDMAXB_H_
#define DEV_GYSFDMAXB_H_

#include "defines.h"
#include "kozos.h"

typedef enum {
	GYSFDMAXB_BAUDRATE_9600 = 0,	// 9600
	GYSFDMAXB_BAUDRATE_115200,		// 115200
	GYSFDMAXB_BAUDRATE_MAX,
} GYSFDMAXB_BAUDRATE;

typedef struct {
	uint8_t  status;	// 測位状態
	uint16_t year;		// 年
	uint8_t month;		// 月
	uint8_t day;		// 日
	uint8_t week;		// 曜日
	uint8_t hour;		// 時
	uint8_t minute;		// 分
	int16_t longitude;	// 経度
	int16_t latitude;	// 緯度
} GYSFDMAXB_DATA;

typedef void (*GYSFDMAXB_CALLBACK)(void *vp, GYSFDMAXB_DATA *data);	// コールバック

extern int32_t gysfdmaxb_init(void);
extern int32_t gysfdmaxb_reg_callback(GYSFDMAXB_CALLBACK callback_fp, void *callback_vp);
extern int32_t gysfdmaxb_change_baudrate(GYSFDMAXB_BAUDRATE baudrate);
extern int32_t gysfdmaxb_change_interval(uint8_t interval);

extern void gysfdmaxb_set_cmd(void);

#endif /* DEV_GYSFDMAXB_H_ */
