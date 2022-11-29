/*
 * BTN_dev.h
 *
 *  Created on: 2022/11/13
 *      Author: User
 */

#ifndef DEV_BTN_DEV_H_
#define DEV_BTN_DEV_H_

// ボタンの状態
typedef enum {
	BTN_SHORT_PUSHED = 0,
	BTN_LONG_PUSHED,
	BTN_STATUS_MAX,
} BTN_STATUS;

// ボタンの種類
typedef enum {
	BTN_UP = 0U,	//!< 上ボタン
	BTN_DOWN,		//!< 下ボタン
	BTN_BACK,		//!< 戻るボタン
	BTN_SELECT,		//!< 決定ボタン
	BTN_MAX,
} BTN_TYPE;

// コールバック関数定義
typedef void (*BTN_CALLBACK)(BTN_STATUS sts);

// 公開関数
extern void btn_up_interrupt(void);
extern void btn_down_interrupt(void);
extern void btn_back_interrupt(void);
extern void btn_select_interrupt(void);
extern int BTN_dev_main(int argc, char *argv[]);
extern int BTN_dev_reg_callback_up(BTN_CALLBACK cb);
extern int BTN_dev_reg_callback_down(BTN_CALLBACK cb);
extern int BTN_dev_reg_callback_back(BTN_CALLBACK cb);
extern int BTN_dev_reg_callback_select(BTN_CALLBACK cb);

#endif /* DEV_BTV_DEV_H_ */
