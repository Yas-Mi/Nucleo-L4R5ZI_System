/*
 * lcd_app.c
 *
 *  Created on: 2022/11/03
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "lib.h"
#include "lcd_dev.h"
#include "btn_dev.h"
#include "lcd_app.h"

// マクロ定義
#define VESION   "ＶＥＲＳＩＯＮ１．０．０"

// 状態
enum State_Type {
	ST_UNINITIALIZED = 0U,
	ST_INTIALIZED,
	ST_START_UP,
	ST_SELECT_MODE_DISP,		// サイセイ/タ゛ウンロード選択画面
	ST_UNDEIFNED,
	ST_MAX,
};

// メッセージ
enum MESSSAGE_Type {
	MSG_INIT = 0U,
	MSG_START_UP,
	MSG_SELECT_MODE,
	MSG_BTN_UP_LONG,
	MSG_BTN_UP_SHORT,
	MSG_BTN_DOWN_LONG,
	MSG_BTN_DOWN_SHORT,
	MSG_BTN_BACK_LONG,
	MSG_BTN_BACK_SHORT,
	MSG_BTN_SELECT_LONG,
	MSG_BTN_SELECT_SHORT,
	MSG_CONNECT,
	MSG_DISCONNECT,
	MSG_MAX,
};

// 関数ポインタ
typedef int (*FUNC)(void);

// 状態遷移用定義
typedef struct {
	FUNC func;
	uint8_t nxt_state;
}FSM;

// 制御用ブロック
typedef struct {
	kz_thread_id_t tsk_id;		// メインタスクのID
	kz_msgbox_id_t msg_id;		// メッセージID
	uint8_t state;				// 状態
	// ST_SELECT_MODE_DISPで使用する情報
	uint8_t select_ypos;			//"＞"のy位置
	
} LCD_APP_CTL;
LCD_APP_CTL lcd_ap_ctl;

// プロトタイプ宣言
static void lcd_app_init(void);
static void lcd_app_start_up(void);
static void lcd_app_select_mode(void);
static void lcd_app_move_cursor(void);

// 状態遷移テーブル
static const FSM fsm[ST_MAX][MSG_MAX] = {
	// MSG_INIT					      MSG_START_UP						 MSG_SELECT_MODE   							 MSG_BTN_UP_LONG	   MSG_BTN_UP_SHORT	   				   MSG_BTN_DOWN_LONG	 MSG_BTN_DOWN_SHORT    				  MSG_BTN_BACK_LONG     MSG_BTN_BACK_SHORT    MSG_BTN_SELECT_LONG   MSG_BTN_SELECT_SHORT  MSG_CONNECT MSG_DISCONNECT
	{{lcd_app_init, ST_INTIALIZED},  {NULL, ST_UNDEIFNED}, 				{NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},				   {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, 				  {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},}, // ST_UNINITIALIZED
	{{NULL, ST_UNDEIFNED},			 {lcd_app_start_up, ST_START_UP},   {NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},				   {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, 				  {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},}, // ST_INTIALIZED
	{{NULL, ST_UNDEIFNED},			 {NULL, ST_UNDEIFNED}, 				{lcd_app_select_mode, ST_SELECT_MODE_DISP}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},				   {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, 				  {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},}, // ST_START_UP
	{{NULL, ST_UNDEIFNED},			 {NULL, ST_UNDEIFNED}, 				{NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {lcd_app_move_cursor, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {lcd_app_move_cursor, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},}, // ST_SELECT_MODE_DISP
};

// 内部関数
// 起動画面表示関数
static void lcd_app_start_up(void)
{
	// 起動画面表示
	LCD_dev_clear_disp();
	LCD_dev_write(1, 0, "オンカ゛クサイセイシステム");
	LCD_dev_write(2, 1, VESION);
	LCD_dev_disp();
}

// モードセレクト表示関数
static void lcd_app_select_mode(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	
	// サイセイ/ダウンロード
	LCD_dev_clear_disp();
	LCD_dev_write(1,0,"サイセイ");
	LCD_dev_write(1,1,"タ゛ウンロート゛");
	LCD_dev_write(0,this->select_ypos,"＞");
	LCD_dev_disp();
}

// 上ボタンコールバック
static void lcd_app_btn_up(BTN_STATUS sts)
{
	// 短押しの場合
	if (sts == BTN_SHORT_PUSHED) {
		// メッセージの送信
		LCD_APP_MSG_btn_up_short();
	// 長押しの場合
	} else {
		// メッセージの送信
		LCD_APP_MSG_btn_up_long();
	}
}

// 下ボタンコールバック
static void lcd_app_btn_down(BTN_STATUS sts)
{
	// 短押しの場合
	if (sts == BTN_SHORT_PUSHED) {
		// メッセージの送信
		LCD_APP_MSG_btn_down_short();
	// 長押しの場合
	} else {
		// メッセージの送信
		LCD_APP_MSG_btn_down_long();
	}
}

// 戻るボタンコールバック
static void lcd_app_btn_back(BTN_STATUS sts)
{
	// 短押しの場合
	if (sts == BTN_SHORT_PUSHED) {
		// メッセージの送信
		LCD_APP_MSG_btn_back_short();
	// 長押しの場合
	} else {
		// メッセージの送信
		LCD_APP_MSG_btn_back_long();
	}
}

// 決定ボタンコールバック
static void lcd_app_btn_select(BTN_STATUS sts)
{
	// 短押しの場合
	if (sts == BTN_SHORT_PUSHED) {
		// メッセージの送信
		LCD_APP_MSG_btn_select_short();
	// 長押しの場合
	} else {
		// メッセージの送信
		LCD_APP_MSG_btn_select_long();
	}
}

// 初期化関数
static void lcd_app_init(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	
	// LCDデバイスドライバ初期化
	LCD_dev_init();
	
	//ボタンのコールバックを設定
	BTN_dev_reg_callback_up(lcd_app_btn_up);
	BTN_dev_reg_callback_down(lcd_app_btn_down);
	BTN_dev_reg_callback_back(lcd_app_btn_back);
	BTN_dev_reg_callback_select(lcd_app_btn_select);
	
	// "＞"のy位置を0としておく
	this->select_ypos = 0;
}

// カーソル移動関数
static void lcd_app_move_cursor(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	
	// 現在表示されている"＞"を削除
	LCD_dev_clear(0, this->select_ypos, 1);
	
	// カーソル位置を反転させる
	this->select_ypos = ~this->select_ypos & 0x01;
	
	// カーソル("＞")を移動 (*)x位置が0の場所には文字がない前提
	//LCD_dev_clear_disp();
	LCD_dev_write(0, this->select_ypos, "＞");
	LCD_dev_disp();
}

// メインのタスク
// 状態遷移テーブルに従い、処理を実行し、状態を更新する
int LCD_app_main(int argc, char *argv[])
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	uint8_t cur_state = ST_UNINITIALIZED;
	uint8_t nxt_state = cur_state;
	LCD_APP_MSG *msg;
	int32_t size;
	FUNC func;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(LCD_APP_CTL));
	
	// 自タスクのIDを制御ブロックに設定
	this->tsk_id = kz_getid();
	// メッセージIDを制御ブロックに設定
	this->msg_id = MSGBOX_ID_LCD_APP_MAIN;
	
	while(1){
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);
		
		// 処理/次状態を取得
		func = fsm[cur_state][msg->msg_type].func;
		nxt_state = fsm[cur_state][msg->msg_type].nxt_state;
		
		// メッセージを解放
		kz_kmfree(msg);
		
		this->state = nxt_state;
		
		// 処理を実行
		if (func != NULL) {
			func();
		}
		
		// 状態遷移
		if (nxt_state != ST_UNDEIFNED) {
			cur_state = nxt_state;
		}
	}

	return 0;
}

// 外部公開関数
// 初期化要求メッセージを送信する関数
void LCD_APP_MSG_init(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_INIT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// 起動画面表示メッセージを送信する関数
void LCD_APP_MSG_start_up(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_START_UP;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// モードセレクト表示メッセージを送信する関数
void LCD_APP_MSG_select_mode(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_SELECT_MODE;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// 上ボタン長押しメッセージを送信する関数
void LCD_APP_MSG_btn_up_long(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_UP_LONG;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// 上ボタン短押しメッセージを送信する関数
void LCD_APP_MSG_btn_up_short(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_UP_SHORT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// 下ボタン長押しメッセージを送信する関数
void LCD_APP_MSG_btn_down_long(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_DOWN_LONG;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// 下ボタン短押しメッセージを送信する関数
void LCD_APP_MSG_btn_down_short(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_DOWN_SHORT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// 戻るボタン長押しメッセージを送信する関数
void LCD_APP_MSG_btn_back_long(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_BACK_LONG;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// 戻るボタン短押しメッセージを送信する関数
void LCD_APP_MSG_btn_back_short(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_BACK_SHORT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// 決定ボタン長押しメッセージを送信する関数
void LCD_APP_MSG_btn_select_long(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_SELECT_LONG;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// 決定ボタン短押しメッセージを送信する関数
void LCD_APP_MSG_btn_select_short(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_SELECT_SHORT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

