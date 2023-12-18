/*
 * touch_screen.c
 *
 *  Created on: 2022/10/29
 *      Author: User
 */
/*
	設計思想
	・タッチパネルのデバイスが変わっても、アプリ側に修正は入らないようにする
	
*/
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "intr.h"
#include "MSP2807.h"
#include <string.h>

#include "touch_screen.h"

// 状態
#define ST_UNINITIALIZED	(0)
#define ST_INITIALIZED		(1)
#define ST_UNDEIFNED		(2)
#define ST_MAX				(3)

// イベント
#define EVENT_CYC		(0)
#define EVENT_WRITE		(1)
#define EVENT_MAX		(2)

// マクロ
#define CHECK_TOUCH_STATE_PERIOD	(10)	// タッチ状態のチェック周期
#define CHECK_TOUCH_STATE_CNT		(3)		// タッチ状態のチェック回数
#define TS_TOUCHED					((1 << CHECK_TOUCH_STATE_CNT) - 1)
											// タッチ確定
#define AVERAGE_CNT					(3)		// 何回分の平均値をとるか
#define CALLBACK_MAX				(2)		// コールバック最大値

// コールバック情報
typedef struct {
	TS_CALLBACK_TYPE type;
	TS_MNG_CALLBACK  callback_fp;
	void             *callback_vp;
} CALLBACK_INFO;

// キャリブレーション情報
/* 
end_x end_y	
	+----------------------+
	|                      | ○
	|                      | ○
	|                      | ○
	|                      | ○
	|                      | ○
	+----------------------+
					start_x start_y
*/
typedef struct {
	uint16_t start_x;		// 開始のx座標
	uint16_t end_x;			// 終了のx座標
	uint16_t start_y;		// 開始のy座標
	uint16_t end_y;			// 終了のy座標
	uint16_t width;			// 幅
	uint16_t height;		// 高さ
	uint16_t lcd_width;		// LCDの幅
	uint16_t lcd_height;	// LCDの高さ
} CALIB_INFO;

// 制御用ブロック
typedef struct {
	uint32_t			state;							// 状態
	kz_thread_id_t		tsk_id;							// タスクID
	kz_msgbox_id_t		msg_id;							// メッセージID
	kz_msgbox_id_t		slp_msg_id;						// スリープメッセージID
	uint8_t				touch_state_bmp;				// タッチ状態のビットマップ
	uint8_t				check_touch_state_cnt;			// タッチ状態のチェック回数
	uint16_t			touch_x_val[AVERAGE_CNT];		// x座標
	uint16_t			touch_y_val[AVERAGE_CNT];		// y座標
	uint16_t			get_touch_val_cnt;				// タッチ値取得回数
	CALLBACK_INFO		callback_info[CALLBACK_MAX];	// コールバック
	CALIB_INFO			calib_info;						// キャリブレーション情報
} TOUCH_SCREEN_CTL;
static TOUCH_SCREEN_CTL ts_ctl;

// メッセージ
typedef struct {
	uint32_t	msg_type;
	uint32_t	msg_data;
}TS_MNG_MSG;

// コールバック呼び出し
static void do_callback(TS_CALLBACK_TYPE type, uint16_t x, uint16_t y)
{
	TOUCH_SCREEN_CTL *this = &ts_ctl;
	uint8_t i;
	CALLBACK_INFO *cb_info;
	
	// 呼び出し
	for (i = 0; i < CALLBACK_MAX; i++) {
		cb_info = &(this->callback_info[i]);
		if ((type == cb_info->type) && (cb_info->callback_fp != NULL)) {
			cb_info->callback_fp(cb_info->type, x, y, cb_info->callback_vp);
		}
	}
}

// タッチスクリーンの座標からLCDの座標に変換
static void conv_touch2lcd(uint16_t touch_x, uint16_t touch_y, uint16_t *lcd_x, uint16_t *lcd_y)
{
	TOUCH_SCREEN_CTL *this = &ts_ctl;
	CALIB_INFO *calib_info = &(this->calib_info);
	uint16_t adj_touch_x, adj_touch_y;
	float ratio;
	
	// 値を整える
	if (touch_x < calib_info->start_x) {
		adj_touch_x = 0;
	} else if (touch_x > calib_info->end_x){
		adj_touch_x = calib_info->end_x - calib_info->start_x;
	} else {
		adj_touch_x = touch_x - calib_info->start_x;
	}
	if (touch_y < calib_info->start_y) {
		adj_touch_y = 0;
	} else if (touch_y > calib_info->end_y){
		adj_touch_y = calib_info->end_y - calib_info->start_y;
	} else {
		adj_touch_y = touch_y - calib_info->start_y;
	}
	
	// xの比率を計算
	ratio = (float)((float)adj_touch_x/(float)calib_info->width);
	*lcd_x = (uint16_t)((float)(calib_info->lcd_width) * ratio);
	// yの比率を計算
	ratio = (float)((float)adj_touch_y/(float)calib_info->height);
	*lcd_y = (uint16_t)((float)(calib_info->lcd_height) * ratio);
}

// タッチの状態を取得
static void ts_msg_check_touch_state(void)
{
	TOUCH_SCREEN_CTL *this = &ts_ctl;
	MSP2807_PEN_STATE state;
	int32_t ret;
	int16_t x, y;
	int16_t lcd_x, lcd_y;
	uint8_t i;
	uint16_t sum_x, sum_y, ave_x, ave_y;
	
	// タッチ状態を取得
	this->touch_state_bmp |= (msp2807_get_touch_state() << this->check_touch_state_cnt++);
	
	// チェック回数に達していない場合は終了
	if (this->check_touch_state_cnt < CHECK_TOUCH_STATE_CNT) {
		return;
	}
	
	// チェック回数をクリア
	this->check_touch_state_cnt = 0;
	
	// タッチされた？
	if (this->touch_state_bmp == TS_TOUCHED) {
		// 座標を取得
		ret = msp2807_get_touch_pos(&x, &y);
		if (ret != E_OK) {
			console_str_send("msp2807_get_touch_pos error\n");
		}
#if 0
		// 表示
		console_str_send("mng x:");
		console_val_send_u16(x);
		console_str_send(" y:");
		console_val_send_u16(y);
		console_str_send("\n");
#endif
		// 覚えておく
		this->touch_x_val[this->get_touch_val_cnt] = x;
		this->touch_y_val[this->get_touch_val_cnt] = y;
		this->get_touch_val_cnt++;
		// 指定回数分取得した？
		if (this->get_touch_val_cnt >= AVERAGE_CNT) {
			// 平均値を計算
			sum_x = 0;
			sum_y = 0;
			for (i = 0; i < AVERAGE_CNT; i++) {
				sum_x += this->touch_x_val[i];
				sum_y += this->touch_y_val[i];
			}
			ave_x = sum_x/AVERAGE_CNT;
			ave_y = sum_y/AVERAGE_CNT;
			// タッチパネルから取得した座標をLCDの座標に変換
			conv_touch2lcd(ave_x, ave_y, &lcd_x, &lcd_y);
			if ((lcd_x == 0) && (lcd_y == 0)) {
				volatile int32_t a;
				a++;
			}
			// コールバック
			do_callback(TS_CALLBACK_TYPE_SINGLE, lcd_x, lcd_y);
			// タッチ値取得回数をクリア
			this->get_touch_val_cnt = 0;
			
		}
	// タッチされていない	
	} else {
		// タッチ値取得回数をクリア
		this->get_touch_val_cnt = 0;
		
	}
	
	// タッチ状態をクリア
	this->touch_state_bmp = 0;
}

// タッチの状態を取得
static void ts_msg_write(uint32_t *disp_data)
{
	TOUCH_SCREEN_CTL *this = &ts_ctl;
	int32_t *write_ret;
	int32_t ret;
	
	// 書き込み
	ret = msp2807_write(*disp_data);
	
	// スリープタスクを起こす
	write_ret = kz_kmalloc(sizeof(int32_t));
	*write_ret = ret;
	kz_send(this->slp_msg_id, sizeof(int32_t), write_ret);
}

// キャリブレーション
static void calib(void)
{
	TOUCH_SCREEN_CTL *this = &ts_ctl;
	
	// 設定
	// (*) 本来はちゃんと値を取得して設定したい
	this->calib_info.start_x = 200;
	this->calib_info.end_x = 3900;
	this->calib_info.start_y = 200;
	this->calib_info.end_y = 3900;
	this->calib_info.width = this->calib_info.end_x - this->calib_info.start_x;
	this->calib_info.height = this->calib_info.end_y - this->calib_info.start_y;
	this->calib_info.lcd_width = MSP2807_DISPLAY_WIDTH;
	this->calib_info.lcd_height = MSP2807_DISPLAY_HEIGHT;
	
}

// 状態遷移テーブル
static const FSM fsm[ST_MAX][EVENT_MAX] = {
	// EVENT_CYC								EVENT_WRITE							EVENT_MAX	
	{{NULL,	ST_UNDEIFNED},						{NULL,	ST_UNDEIFNED},				{NULL,	ST_UNDEIFNED},},	// ST_UNINITIALIZED
	{{ts_msg_check_touch_state,	ST_UNDEIFNED},	{ts_msg_write,	ST_UNDEIFNED},		{NULL,	ST_UNDEIFNED},},	// ST_INITIALIZED
	{{NULL,	ST_UNDEIFNED},						{NULL,	ST_UNDEIFNED},				{NULL,	ST_UNDEIFNED},},	// ST_UNDEIFNED
};

// ステートマシンタスク
static int touch_screen_main(int argc, char *argv[])
{
	TOUCH_SCREEN_CTL *this = &ts_ctl;
	TS_MNG_MSG *msg;
	TS_MNG_MSG tmp_msg;
	int32_t size;
	
	while(1){
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);
		// いったんメッセージをローカル変数にコピー
		memcpy(&tmp_msg, msg, sizeof(TS_MNG_MSG));
		// メッセージを解放
		kz_kmfree(msg);
		// 処理を実行
		if (fsm[this->state][tmp_msg.msg_type].func != NULL) {
			fsm[this->state][tmp_msg.msg_type].func(&(tmp_msg.msg_data));
		}
		// 状態遷移
		if (fsm[this->state][tmp_msg.msg_type].nxt_state != ST_UNDEIFNED) {
			this->state = fsm[this->state][tmp_msg.msg_type].nxt_state;
		}
	}
	
	return 0;
}

// 初期化関数
void ts_mng_init(void)
{
	TOUCH_SCREEN_CTL *this = &ts_ctl;
	int32_t ret;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(TOUCH_SCREEN_CTL));
	
	// メッセージIDの設定
	this->msg_id = MSGBOX_ID_TOUCH_SCREEN;
	
	// 周期メッセージの作成
	set_cyclic_message(ts_mng_proc_cyc, CHECK_TOUCH_STATE_PERIOD);
	
	// タスクの生成
	this->tsk_id = kz_run(touch_screen_main, "touch_screen_main",  TOUCH_SCREEN_PRI, TOUCH_SCREEN_STACK, 0, NULL);
	
	// キャリブレーション
	calib();
	
	// 状態の更新
	this->state = ST_INITIALIZED;
	
	return 0;
}

// 周期関数
int32_t ts_mng_proc_cyc(void)
{
	TOUCH_SCREEN_CTL *this = &ts_ctl;
	TS_MNG_MSG *msg;
	
	msg = kz_kmalloc(sizeof(TS_MNG_MSG));
	msg->msg_type = EVENT_CYC;
	msg->msg_data = NULL;
	kz_send(this->msg_id, sizeof(TS_MNG_MSG), msg);
}

// 書き込み関数
int32_t ts_mng_write(uint16_t *disp_data)
{
	TOUCH_SCREEN_CTL *this = &ts_ctl;
	TS_MNG_MSG *msg;
	int32_t size;
	int32_t *rcv_ercd;
	int32_t ercd;
	
	// 書き込みメッセージ送信
	msg = kz_kmalloc(sizeof(TS_MNG_MSG));
	msg->msg_type = EVENT_WRITE;
	msg->msg_data = (uint32_t)disp_data;
	kz_send(this->msg_id, sizeof(TS_MNG_MSG), msg);
	
	// 書き込みデータは更新したらダメなので送信しきるまでスリープ
	kz_recv(this->slp_msg_id, &size, &rcv_ercd);
	ercd = *rcv_ercd;
	// メッセージを解放
	kz_kmfree(rcv_ercd);
	
	return ercd;
}

// 周期関数
int32_t ts_mng_reg_callback(TS_CALLBACK_TYPE type, TS_MNG_CALLBACK callback_fp, void *callback_vp)
{
	TOUCH_SCREEN_CTL *this = &ts_ctl;
	uint8_t i;
	CALLBACK_INFO *cb_info;
	int32_t ret = E_OBJ;
	
	for (i = 0; i < CALLBACK_MAX; i++) {
		cb_info = &(this->callback_info[i]);
		// まだコールバック登録していない？
		if (cb_info->callback_fp == NULL) {
			cb_info->type = type;
			cb_info->callback_fp = callback_fp;
			cb_info->callback_vp = callback_vp;
			ret = E_OK;
			break;
		}
	}
	
	return ret;
}

// コマンド設定関数
void ts_mng_set_cmd(void)
{
	
}
