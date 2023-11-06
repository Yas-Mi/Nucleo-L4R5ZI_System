/*
 * lcd_fw.c
 *
 *  Created on: 2022/12/12
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "lcd_fw.h"
#include "lcd_dev.h"
//#include "lib.h"

// 状態
#define LCD_FW_ST_NOT_INTIALIZED	(0U) // 未初期化
#define LCD_FW_ST_INITIALIZED		(1U) // 初期化済み

// 優先度
#define PRIORITY_NUM	(5U)

//  画面保存用リスト
typedef struct _DISPLAY_INFO_LIST{
	DISPLAY_ALL_INFO disp_all_info;
	struct _DISPLAY_INFO_LIST *next;
} DISPLAY_INFO_LIST;
typedef struct {
	DISPLAY_INFO_LIST *head;
	DISPLAY_INFO_LIST *tail;
}DISPLAY_INFO_QUEUE;

// 制御用ブロック
typedef struct {
	uint8_t status;					// 状態
	uint8_t cursor_pos;				// 現在表示されているカーソルの位置
}LCD_FW_CTL;
static LCD_FW_CTL lcd_fw_ctl;

// 画面保存用
static DISPLAY_INFO_QUEUE readyque[PRIORITY_NUM];			// レディーキュー

// 内部関数
// 画面をリスト構造に設定する処理
static int32_t push_disp(DISPLAY_ALL_INFO *disp_all_info)
{
	DISPLAY_INFO_LIST **head;
	DISPLAY_INFO_LIST **tail;
	
	// disp_infoがNULLの場合、エラーを返して終了
	if (disp_all_info == NULL) {
		return -1;
	}
	
	// リストの情報を取得
	head = &(readyque[disp_all_info->priority].head);
	tail = &(readyque[disp_all_info->priority].tail);
	
	if (*head == NULL) {
		// メモリ確保
		*head = kz_kmalloc(sizeof(DISPLAY_INFO_LIST));
		if (head == NULL) {
			return -1;
		}
		*tail = *head;
	} else {
		// リストに追加
		(*tail)->next = kz_kmalloc(sizeof(DISPLAY_INFO_LIST));
		if ((*tail)->next == NULL) {
			return -1;
		}
		*tail = (*tail)->next;
	}
	// データをコピー
	memcpy(&((*tail)->disp_all_info), disp_all_info, sizeof(DISPLAY_ALL_INFO));
	(*tail)->next = NULL;

	return 0;
}

// リストの要素の数を取得
static uint8_t get_disp_num(uint32_t priority)
{
	DISPLAY_INFO_LIST *cur;
	uint8_t cnt = 0U;
	
	// 優先度が範囲外の場合、エラーを返して終了
	if (priority > PRIORITY_NUM) {
		return -1;
	}
	
	// リストの先頭を取得
	cur = readyque[priority].head;
	
	// リストの数を取得
	while (cur->next != NULL) {
		cur = cur->next;
		cnt++;
	}
	
	return cnt;
}

// 一番新しい画面をリストから除去する処理
static int32_t del_top_disp(uint32_t priority)
{
	DISPLAY_INFO_LIST *cur;
	DISPLAY_INFO_LIST *prev;
	uint8_t num = get_disp_num(priority);
	
	// 要素の数が0の場合、エラーを返して終了
	if (num == 0U) {
		return -1;
	}
	
	// リストの先頭を取得
	cur = readyque[priority].head;
	
	while (cur->next != NULL) {
		prev = cur;
		cur = cur->next;
	}
	// 末尾をリストから削除
	prev->next = NULL;
	kz_kmfree(cur);
	readyque[priority].tail = prev;
	
	return 0;
}

// 外部公開関数
// 初期化処理
void LCD_fw_init(void)
{
	LCD_FW_CTL *this = &lcd_fw_ctl;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(LCD_FW_CTL));
	
	// LCDデバイスドライバの初期化
	LCD_dev_init();
	
	// リストを初期化
	memset(readyque, 0, sizeof(DISPLAY_INFO_QUEUE));
	
	// 状態を初期化済みに更新
	this->status = LCD_FW_ST_INITIALIZED;
	
	return;
}

// 書き込みを行う関数
int8_t LCD_fw_disp(DISPLAY_ALL_INFO *disp_all_info)
{
	LCD_FW_CTL *this = &lcd_fw_ctl;
	int8_t ret;
	int8_t y;
	DISPLAY_INFO *disp_info;
	uint8_t *disp_char;
	uint8_t x_pos;
	uint8_t y_pos;
	
	// disp_all_infoがNULLの場合エラーを返して終了
	if (disp_all_info == NULL) {
		return -1;
	}
	
	// 初期化未実施の場合、エラーを返して終了
	if (this->status != LCD_FW_ST_INITIALIZED) {
		return -1;
	}
	
	// カーソルの位置を設定
	if (disp_all_info->cursor_flag == 1U) {
		switch (disp_all_info->cursor_pos){
			case CURSOR_POS_GO_NEXT:
				// カーソルの位置を進める
				if (this->cursor_pos < (LCD_FW_LINE - 1U)) {
					this->cursor_pos++;
				} else {
					this->cursor_pos = 0U;
				}
				break;
			case CURSOR_POS_GO_BACK:
				// カーソルの位置をもどす
				if (this->cursor_pos > 0) {
					this->cursor_pos--;
				} else {
					this->cursor_pos = LCD_FW_LINE - 1U;
				}
				break;
			default:
				break;
		}
	}
	
	// 画面を覚えておく
	push_disp(disp_all_info);
	
	// いったんlcdイメージをクリア
	ret = LCD_dev_clear_disp();
	if (ret != 0) {
		return ret;
	}
	
	// 表示情報を取得
	disp_info = &(disp_all_info->disp_info);
	
	// 文字を書き込み
	for (y = 0; y < LCD_FW_LINE; y++) {
		// 情報を取得
		disp_char = disp_info->char_info[y].disp_char;
		x_pos = disp_info->char_info[y].x_pos;
		y_pos = disp_info->char_info[y].y_pos;
		// 文字列を書く
		ret = LCD_dev_write(x_pos, y_pos, disp_char);
		if (ret != 0) {
			return ret;
		}
		// カーソルを書く
		if (disp_all_info->cursor_flag == 1U) {
			if (y == this->cursor_pos) {
				ret = LCD_dev_write(LCD_FW_CURSOR_XPOS, this->cursor_pos, "＞");
				if (ret != 0) {
					return ret;
				}
			}
		}
	}
	
	// 表示
	LCD_dev_disp();
	
	return 0;
}

int8_t LCD_fw_disp_back(uint32_t priority)
{
	DISPLAY_INFO_LIST *cur;
	DISPLAY_ALL_INFO *disp_all_info;
	DISPLAY_INFO *disp_info;
	uint8_t *disp_char;
	uint8_t x_pos;
	uint8_t y_pos;
	int8_t ret;
	int8_t y;
	
	// 一番新しい画面を除去
	del_top_disp(priority);
	
	// リストの先頭を取得
	cur = readyque[priority].head;
	
	//前の画面を取得
	while (cur->next != NULL) {
		cur = cur->next;
	}
	
	// いったんlcdイメージをクリア
	ret = LCD_dev_clear_disp();
	if (ret != 0) {
		return ret;
	}
	
	disp_all_info = &(cur->disp_all_info);
	disp_info = &(disp_all_info->disp_info);
	
	// 文字を書き込み
	for (y = 0; y < LCD_FW_LINE; y++) {
		// 情報を取得
		disp_char = disp_info->char_info[y].disp_char;
		x_pos = disp_info->char_info[y].x_pos;
		y_pos = disp_info->char_info[y].y_pos;
		// 文字列を書く
		ret = LCD_dev_write(x_pos, y_pos, disp_char);
		if (ret != 0) {
			return ret;
		}
		// カーソルを書く
		if (disp_all_info->cursor_flag == 1U) {
			if (y == disp_all_info->cursor_pos) {
				ret = LCD_dev_write(LCD_FW_CURSOR_XPOS, disp_all_info->cursor_pos, "＞");
				if (ret != 0) {
					return ret;
				}
			}
		}
	}
	
	// 表示
	LCD_dev_disp();
	
	return 0;
}

int8_t LCD_fw_get_cursor_pos(void)
{
	LCD_FW_CTL *this = &lcd_fw_ctl;
	
	return this->cursor_pos;
}

