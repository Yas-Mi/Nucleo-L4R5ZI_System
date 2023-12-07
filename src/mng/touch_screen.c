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
#define EVENT_MAX		(1)

// マクロ
#define CHECK_TOUCH_STATE_PERIOD	(10)	// タッチ状態のチェック周期
#define CHECK_TOUCH_STATE_CNT		(3)		// タッチ状態のチェック回数
#define TS_TOUCHED					((1 << CHECK_TOUCH_STATE_CNT) - 1)
											// タッチ確定

// 制御用ブロック
typedef struct {
	uint32_t			state;					// 状態
	kz_thread_id_t		tsk_id;					// タスクID
	kz_msgbox_id_t		msg_id;					// メッセージID
	uint8_t				touch_state_bmp;		// タッチ状態のビットマップ
	uint8_t				check_touch_state_cnt;	// タッチ状態のチェック回数
	TS_MNG_CALLBACK		callback_fp;			// コールバック
} TOUCH_SCREEN_CTL;
static TOUCH_SCREEN_CTL ts_ctl;

// メッセージ
typedef struct {
	uint32_t	msg_type;
	uint32_t	msg_data;
}TS_MNG_MSG;

// タッチの状態を取得
static void ts_msg_check_touch_state(void)
{
	TOUCH_SCREEN_CTL *this = &ts_ctl;
	MSP2807_PEN_STATE state;
	int32_t ret;
	int16_t x, y;
	
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
		console_str_send("touched!\n");
		// 座標を取得
		x = 0;
		y = 0;
		ret = msp2807_get_touch_pos(&x, &y);
		if (ret != E_OK) {
			console_str_send("msp2807_get_touch_pos error\n");
		}
		console_str_send("x : ");
		console_val_send_u16(x);
		console_str_send(" y : ");
		console_val_send_u16(y);
		console_str_send("\n");
	// タッチされていない	
	} else {
		;
	}
	
	// タッチ状態をクリア
	this->touch_state_bmp = 0;
}

// 状態遷移テーブル
static const FSM fsm[ST_MAX][EVENT_MAX] = {
	// EVENT_CYC			EVENT_MAX	
	{{NULL,	ST_UNDEIFNED},	{NULL,	ST_UNDEIFNED},},	// ST_UNINITIALIZED
	{{NULL,	ST_UNDEIFNED},	{NULL,	ST_UNDEIFNED},},	// ST_INITIALIZED
	{{NULL,	ST_UNDEIFNED},	{NULL,	ST_UNDEIFNED},},	// ST_UNDEIFNED
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

// コマンド設定関数
void ts_mng_set_cmd(void)
{
	
}
