/*
 * lcd_apl.h
 *
 *  Created on: 2022/09/18
 *      Author: User
 */

#ifndef APL_LCD_APP_H_
#define APL_LCD_APP_H_
#include "defines.h"
#include "kozos.h"

// メッセージ定義
typedef struct {
	uint32_t msg_type;
	void     *msg_data;
}LCD_APP_MSG;

// タスクにメッセージを送信するための関数
void LCD_APP_MSG_init(void);
void LCD_APP_MSG_start_up(void);
void LCD_APP_MSG_select_mode(void);
void LCD_APP_MSG_btn_up_long(void);
void LCD_APP_MSG_btn_up_short(void);
void LCD_APP_MSG_btn_down_long(void);
void LCD_APP_MSG_btn_down_short(void);
void LCD_APP_MSG_btn_back_long(void);
void LCD_APP_MSG_btn_back_short(void);
void LCD_APP_MSG_btn_select_long(void);
void LCD_APP_MSG_btn_select_short(void);

#endif /* APL_US_SENSOR_H_ */
