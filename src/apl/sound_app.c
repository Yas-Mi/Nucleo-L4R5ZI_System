/*
 * sound_app.c
 *
 *  Created on: 2023/5/8
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "sound_app.h"

// 状態
#define ST_UNINITIALIZED	(0)		// 未初期化状態
#define ST_IDLE				(1)		// アイドル状態
#define ST_RUN				(2)		// 音楽再生中状態
#define ST_UNDEIFNED		(3)		// 未定義状態
#define ST_MAX				(4)

// イベント
#define EVENT_SOUND_STA		(0)		// 音楽再生開始イベント
#define EVENT_SOUND_STP		(1)		// 音楽再生開始イベント
#define EVENT_MAX			(2)

// マクロ
#define SOUND_DATA_TIME_UNIT		(100)												// 1回の送信で何ms間音楽再生するか[ms]
#define SOUND_DATA_STORE_TIME		(10)												// 何秒分の音楽データを保持しておくか[s]
#define SOUND_DATA_UNIT				(SAMPLING_FREQUENCY/SOUND_DATA_TIME_UNIT)			// 1回の送信で何個の音楽データを再生するか
#define SOUND_DATA_SIZE_UNIT		(SOUND_DATA_UNIT*(SOUND_DATA_WIDTH/2))				// 1回の送信でどれだけのサイズが必要か[byte]
#define SOUND_DATA_COUNT			((SOUND_DATA_STORE_TIME*1000)/SOUND_DATA_TIME_UNIT)	// 何回送信する必要があるか

// 制御用ブロック
typedef struct {
	uint32_t			state;											// 状態
	kz_thread_id_t		tsk_id;											// タスクID
	kz_msgbox_id_t		msg_id;											// メッセージID
	uint16_t			data[SOUND_DATA_COUNT][SOUND_DATA_SIZE_UNIT];	// 音声データ
} SOUND_APP_CTL;
static SOUND_APP_CTL sound_app_ctl;

// メッセージ
typedef struct {
	uint32_t     msg_type;
	void         *msg_data;
}SOUND_APP_MSG;

static void sound_app_sta(void *par)
{
	
}

static void sound_app_stp(void *par)
{
	
}

// 状態遷移テーブル
static const FSM fsm[ST_MAX][EVENT_MAX] = {
	// EVENT_SOUND_STA						EVENT_SOUND_STP
	{{NULL,					ST_UNDEIFNED}, {NULL,					ST_UNDEIFNED}},		// ST_UNINITIALIZED
	{{sound_app_sta,		ST_RUN		}, {NULL,					ST_UNDEIFNED}},		// ST_IDLE
	{{NULL,					ST_UNDEIFNED}, {sound_app_stp,			ST_IDLE		}},		// ST_RUN
	{{NULL,					ST_UNDEIFNED}, {NULL,					ST_UNDEIFNED}},		// ST_UNDEIFNED
};

// BlueToothデバイス状態管理タスク
static int sound_app_main(int argc, char *argv[])
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	SOUND_APP_MSG *msg;
	SOUND_APP_MSG tmp_msg;
	int32_t size;
	
	while(1){
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);
		
		// いったんメッセージをローカル変数にコピー
		memcpy(&tmp_msg, msg, sizeof(SOUND_APP_MSG));
		
		// メッセージを解放
		kz_kmfree(msg);
		
		// 処理を実行
		if (fsm[this->state][msg->msg_type].func != NULL) {
			fsm[this->state][msg->msg_type].func(&(tmp_msg.msg_data));
		}
		
		// 状態遷移
		if (fsm[this->state][msg->msg_type].nxt_state != ST_UNDEIFNED) {
			this->state = fsm[this->state][msg->msg_type].nxt_state;
		}
	}
	
	return 0;
}

// 初期化関数
void sound_app_init(void)
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	int32_t ret;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(SOUND_APP_CTL));
	
	// メッセージIDの設定
	this->msg_id = MSGBOX_ID_SOUND_APP;
	
	// オーディオコーデックのデバイスドライバをオープン
	ret = pcm3060_open(SAMPLING_FREQUENCY, SOUND_DATA_WIDTH);
	
	// タスクの生成
	this->tsk_id = kz_run(sound_app_main, "sound_app_main",  SOUND_APP_PRI, SOUND_APP_STACK, 0, NULL);
	
	// 状態の更新
	this->state = ST_IDLE;
	
	return;
}
