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
#define EVENT_SOUND_STA			(0)		// 音楽再生開始イベント
#define EVENT_SOUND_STA_BEEP	(1)		// 音楽再生開始イベント (beep音)
#define EVENT_SOUND_STP			(2)		// 音楽再生停止イベント
#define EVENT_MAX				(3)

// マクロ
// 音楽
#define SOUND_DATA_TIME_UNIT		(100)												// 1回の送信で何ms間音楽再生するか[ms]
#define SOUND_DATA_STORE_TIME		(10)												// 何秒分の音楽データを保持しておくか[s]
#define SOUND_DATA_UNIT				(SAMPLING_FREQUENCY/SOUND_DATA_TIME_UNIT)			// 1回の送信で何個の音楽データを再生するか
#define SOUND_DATA_SIZE_UNIT		(SOUND_DATA_UNIT*(SOUND_DATA_WIDTH/8))				// 1回の送信でどれだけのサイズが必要か[byte]
#define SOUND_DATA_COUNT			((SOUND_DATA_STORE_TIME*1000)/SOUND_DATA_TIME_UNIT)	// 何回送信する必要があるか

/*
	送信データ格納データの考え方
	data
					 SOUND_DATA_UNIT
	|-----------------------------------------------
		0	1	2	3	4	5	6	7	8	9	10	 11	  12   13	...
	0 d00 d01 d02 d03 d04 d05 d06 d07 d08 d09 d010 d010 d012 d013   ... 1行が1回に送信するデータ SOUND_DATA_TIME_UNIT[ms] SOUND_DATA_SIZE_UNIT[byte]
	1 d10 d11 d12 d13 d14 d15 d16 d17 d18 d19 d110 d110 d112 d113   ...
	2 d20 d21 d22 d23 d24 d25 d26 d27 d28 d29 d210 d210 d212 d213   ...
	3 d30 d31 d32 d33 d34 d35 d36 d37 d38 d39 d310 d310 d312 d313   ...
	4 d40 d41 d42 d43 d44 d45 d46 d47 d48 d49 d410 d410 d412 d413   ...
    ...
*/

// ビープ
#define BEEP_SEND_TIME_UNIT			(100)		// 1回の送信で何msbeep音を流すか[ms]

// 制御用ブロック
typedef struct {
	uint32_t			state;											// 状態
	kz_thread_id_t		tsk_id;											// タスクID
	kz_msgbox_id_t		msg_id;											// メッセージID
	uint16_t			data[SOUND_DATA_COUNT][SOUND_DATA_SIZE_UNIT];	// 音声データ
	uint16_t			w_idx;											// ライトインデックス
	uint16_t			r_idx;											// リードインデックス
} SOUND_APP_CTL;
static SOUND_APP_CTL sound_app_ctl;

// メッセージ
typedef struct {
	uint32_t     msg_type;
	void         *msg_data;
}SOUND_APP_MSG;

// ビープ音 1ms分
static const int32_t beep_1ms[] = {
	0,
	12539,
	23170,
	30273,
	32767,
	30273,
	23170,
	12539,
	0,
	-12539,
	-23170,
	-30273,
	-32767,
	-30273,
	-23170,
	-12539,
};

// 送信完了コールバック
static void pcm3060_callback(void *vp)
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	// 起こす
	kx_wakeup(this->tsk_id);
}

// 音楽再生開始ハンドラ
static void sound_app_sta(void *par)
{
	
}

// 音楽再生開始ハンドラ(beep音)
static void sound_app_sta_beep(void *par)
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	uint32_t play_time = *((uint32_t*)par);
	int32_t play_data[16*BEEP_SEND_TIME_UNIT];
	uint32_t i;
	uint32_t frame_cnt;
	
	// 開始を表示
	console_str_send((uint8_t*)"beep start\n");
	
	// 100ms単位で考える
	for (i = 0; i < BEEP_SEND_TIME_UNIT; i++) {
		memcpy(&play_data[16*i], beep_1ms,  sizeof(beep_1ms));
	}
	
	frame_cnt = play_time / BEEP_SEND_TIME_UNIT;
	
	// 再生する
	while(frame_cnt > 0) {
		// 送信
		pcm3060_play(play_data, sizeof(play_data)/4);
		// 寝る
		kz_sleep();
		// コンソール表示
		console_str_send((uint8_t*)"beep 1frame end\n");
		// フレーム回数を減らす
		frame_cnt--;
	}
	
	// 終了を表示
	console_str_send((uint8_t*)"beep end\n");
}

// 音楽再生停止ハンドラ(beep音)
static void sound_app_stp(void *par)
{
	
}

// 状態遷移テーブル
static const FSM fsm[ST_MAX][EVENT_MAX] = {
	// EVENT_SOUND_STA						EVENT_SOUND_STA_BEEP					EVENT_SOUND_STP
	{{NULL,					ST_UNDEIFNED}, {NULL,					ST_UNDEIFNED}, {NULL,					ST_UNDEIFNED}},		// ST_UNINITIALIZED
	{{sound_app_sta,		ST_RUN		}, {sound_app_sta_beep,		ST_UNDEIFNED}, {NULL,					ST_UNDEIFNED}},		// ST_IDLE
	{{NULL,					ST_UNDEIFNED}, {NULL,					ST_IDLE		}, {sound_app_stp,			ST_IDLE		}},		// ST_RUN
	{{NULL,					ST_UNDEIFNED}, {NULL,					ST_UNDEIFNED}, {NULL,					ST_UNDEIFNED}},		// ST_UNDEIFNED
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
	ret = pcm3060_open(SAMPLING_FREQUENCY, SOUND_DATA_WIDTH, pcm3060_callback, this);
	
	// タスクの生成
	this->tsk_id = kz_run(sound_app_main, "sound_app_main",  SOUND_APP_PRI, SOUND_APP_STACK, 0, NULL);
	
	// 状態の更新
	this->state = ST_IDLE;
	
	return;
}

// BlueTooth送信通知関数
int32_t sound_app_play_beep(uint32_t play_time)
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	SOUND_APP_MSG *msg;
	uint8_t i;
	
	// アイドル出ない場合はエラーを返して終了
	if (this->state != ST_IDLE) {
		return -1;
	}
	
	msg = kz_kmalloc(sizeof(SOUND_APP_MSG));
	msg->msg_type = EVENT_SOUND_STA_BEEP;
	msg->msg_data = play_time;
	
	kz_send(this->msg_id, sizeof(SOUND_APP_MSG), msg);
}
// コマンド
static void sound_app_cmd_beep_1000ms(void)
{
	// 1000msbeep音を流す
	sound_app_play_beep(1000);
}

// コマンド設定関数
void sound_app_set_cmd(void)
{
	COMMAND_INFO cmd;
	
	// コマンドの設定
	cmd.input = "sound beep 1000ms";
	cmd.func = sound_app_cmd_beep_1000ms;
	console_set_command(&cmd);
}
