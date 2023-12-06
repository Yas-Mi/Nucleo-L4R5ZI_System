/*
 * touch_screeb.h
 *
 *  Created on: 2023/12/6
 *      Author: User
 */

#ifndef _TS_MNG_H_
#define _TS_MNG_H_

typedef enum {
	TS_CALLBACK_TYPE_SINGLE = 0,	// シングルクリック
	TS_CALLBACK_TYPE_DOUBLE,		// ダブるクリック
	TS_CALLBACK_TYPE_MAX,
} TS_CALLBACK_TYPE;

typedef void (*TS_MNG_CALLBACK)(TS_CALLBACK_TYPE type, uint16_t x, uint16_t y, void *vp);

extern void ts_mng_init(void);

#endif /* _TS_MNG_H_ */