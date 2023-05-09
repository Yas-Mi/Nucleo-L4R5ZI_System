/*
 * lcd_fw.h
 *
 *  Created on: 2022/12/12
 *      Author: User
 */

#ifndef MNG_LCD_FW_H_
#define MNG_LCD_FW_H_

#include "lcd_dev.h"

// マクロ
#define LCD_FW_DISPLAY_WIDTH		LCD_DISPLAY_WIDTH		// LCDの幅
#define LCD_FW_DISPLAY_HEIGHT		LCD_DISPLAY_HEIGHT		// LCDの高さ
#define LCD_FW_LINE					LCD_DISPLAY_HEIGHT		// LCDに表示する文字の行数 (*)今回は行数 = 高さ
#define LCD_FW_CURSOR_XPOS			(0U)					// カーソルのx位置
#define LCD_FW_SAVE_DISPLAY_NUM		(4U)					// 保存しておく表示の数 (*)画面を戻す場合どれだけ覚えておくか

typedef enum {
	CURSOR_POS_NO_CHANGE = 0,	//		カーソルの位置はそのまま
	CURSOR_POS_GO_NEXT,			//		カーソルの位置を進める
	CURSOR_POS_GO_BACK,			//		カーソルの位置を戻す
	CURSOR_POS_MAX,				//		最大値
} CURSOR_POS;

typedef struct {
	uint8_t x_pos;								// 表示するx位置
	uint8_t y_pos;								// 表示するy位置
	uint8_t *disp_char;	// 表示する文字列
} CHAR_INFO;

typedef struct {
	CHAR_INFO  char_info[LCD_FW_LINE];		// 表示文字列
} DISPLAY_INFO;

typedef struct {
	DISPLAY_INFO  disp_info;		// 表示文字列
	uint8_t       cursor_flag;		// カーソルを表示するかしないか 0:表示しない 1:表示する
	CURSOR_POS    cursor_pos;		// カーソル位置
	uint8_t       priority;			// 優先度
} DISPLAY_ALL_INFO;

extern void LCD_fw_init(void);
extern int8_t LCD_fw_disp(DISPLAY_ALL_INFO *disp_all_info);
extern int8_t LCD_fw_disp_back(uint32_t priority);
extern int8_t LCD_fw_get_cursor_pos(void);

#endif /* DEV_LCD_DEV_H_ */
