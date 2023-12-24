/*
 * touch_screeb.h
 *
 *  Created on: 2023/12/6
 *      Author: User
 */

#ifndef _TS_MNG_H_
#define _TS_MNG_H_

typedef enum {
	TS_CALLBACK_TYPE_TOUCH_START = 0,	// タッチ開始
	TS_CALLBACK_TYPE_TOUCHING,			// タッチ中
	TS_CALLBACK_TYPE_RELEASE,			// タッチ中からの離し
	TS_CALLBACK_TYPE_SINGLE_CLICK,		// シングルクリック
	TS_CALLBACK_TYPE_DOUBLE_CLICK,		// ダブルクリック (*) 未サポート
	TS_CALLBACK_TYPE_MAX,
} TS_CALLBACK_TYPE;

typedef void (*TS_MNG_CALLBACK)(TS_CALLBACK_TYPE type, uint16_t x, uint16_t y, void *vp);

extern void ts_mng_init(void);
extern void ts_mng_proc_cyc(void);
extern int32_t ts_mng_reg_callback(TS_CALLBACK_TYPE type, TS_MNG_CALLBACK callback_fp, void *callback_vp);
extern int32_t ts_mng_write(uint16_t *disp_data);

extern void ts_mng_set_cmd(void);

#endif /* _TS_MNG_H_ */