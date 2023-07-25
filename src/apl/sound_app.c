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
#include "pcm3060.h"
#include "wav.h"
#include <string.h>

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
#define SOUND_TIME_PER_FRAME		(100)												// 1回の送信で何ms間音楽再生するか[ms]    : 1フレーム 100ms
#define SOUND_DATA_STORE_TIME		(3*1000)											// 何ms分の音楽データを保持しておくか[ms] : 3秒間の音声データを格納
#define SOUND_DATA_NUM_PAR_FRAME	((SAMPLING_FREQUENCY/1000)*SOUND_TIME_PER_FRAME)	// 1フレームの音楽データ数                : 16000/1000=16個(=1msに必要なデータ数) 16*100=1600(=SOUND_TIME_PER_FRAME[ms]に必要なデータ数)
#define SOUND_FRAME_NUM				(SOUND_DATA_STORE_TIME/SOUND_TIME_PER_FRAME)		// SOUND_DATA_STORE_TIME[s]保存するためには何フレーム必要か 3*1000 = 3000ms 3000ms / 100ms = 30フレーム
#define SOUND_FIRST_SEND_FRAME_NUM	(20)												// 最初にどれだけのフレームがたまったら送信するか
#define SOUND_SEND_FRAME_NUM		(10)												// 2回目以降どれだけのフレームがたまったら送信するか

/*
	送信データ格納データの考え方
	data
				|-----------------------------------------------
					0	1	2	3	4	5	6	7	8	9	10	 11	  12   13	...  SOUND_DATA_UNIT
	1フレーム	0 d00 d01 d02 d03 d04 d05 d06 d07 d08 d09 d010 d010 d012 d013   ...  d0(SOUND_DATA_UNIT-1)  1行が1回に送信するデータ SOUND_DATA_TIME_UNIT[ms] SOUND_DATA_SIZE_UNIT[byte]
	2フレーム	1 d10 d11 d12 d13 d14 d15 d16 d17 d18 d19 d110 d110 d112 d113   ...  d1(SOUND_DATA_UNIT-1)
	3フレーム	2 d20 d21 d22 d23 d24 d25 d26 d27 d28 d29 d210 d210 d212 d213   ...  d2(SOUND_DATA_UNIT-1)
	4フレーム	3 d30 d31 d32 d33 d34 d35 d36 d37 d38 d39 d310 d310 d312 d313   ...  d3(SOUND_DATA_UNIT-1)
	5フレーム	4 d40 d41 d42 d43 d44 d45 d46 d47 d48 d49 d410 d410 d412 d413   ...  d4(SOUND_DATA_UNIT-1)
    ...
*/

// ビープ
#define BEEP_PLAY_TIME_3000ms	(3000 << 0)		// 3秒
#define BEEP_PLAY_TIME_1000ms	(1000 << 0)		// 1秒
#define BEEP_PLAY_0KHZ			(   0 << 16)	// 0kHz
#define BEEP_PLAY_1KHZ			(   1 << 16)	// 1kHz

// 制御用ブロック
typedef struct {
	uint32_t			state;												// 状態
	kz_thread_id_t		tsk_id;												// タスクID
	kz_msgbox_id_t		msg_id;												// メッセージID
	int32_t				data[SOUND_FRAME_NUM][SOUND_DATA_NUM_PAR_FRAME];	// 音声データ
	uint16_t			w_idx;												// ライトインデックス
	uint16_t			r_idx;												// リードインデックス
	uint16_t			req_frame_w_idx;									// 現在送信中の送信終了フレームインデックス
	uint16_t			req_frame_r_idx;									// 現在送信中の送信開始フレームインデックス
	uint16_t			cur_frame_idx;										// 現在送信中のフレームインデックス
	uint8_t				first_send_flag;									// 初回送信フラグ
	uint8_t				is_expected;										// 期待する音楽データの種類かどうか
	WAV_INFO			wav_info;											// 受信情報
} SOUND_APP_CTL;
static SOUND_APP_CTL sound_app_ctl;

// 送信情報
typedef struct {
	uint16_t start_frame_idx;
	uint16_t end_frame_idx;
} SOUND_APP_SEND_INFO;

// メッセージ
typedef struct {
	uint32_t     msg_type;
	uint32_t     msg_data;
} SOUND_APP_MSG;

// ビープ音(0Hz) 1ms分
static const int32_t beep_0kHz_1ms[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};

// ビープ音(1kHz) 1ms分
static const int32_t beep_1kHz_1ms[] = {
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

// ビープデータテーブル
static const int32_t *beep_data[] = {
	beep_0kHz_1ms,
	beep_1kHz_1ms,
};

// フレーム計算
static uint16_t sound_app_get_frame_num(uint16_t frame_w_idx, uint16_t frame_r_idx)
{
	uint16_t frame_num;
	
	// フレーム数を計算
	if (frame_r_idx > frame_w_idx) {
		frame_num = (SOUND_FRAME_NUM - frame_r_idx) + frame_w_idx;
	} else {
		frame_num = frame_w_idx - frame_r_idx;
	}
	
	return frame_num;
}

// 音声鳴動要求
static void sound_app_req_pray(SOUND_APP_CTL *this, uint16_t frame_w_idx, uint16_t frame_r_idx)
{
	SOUND_APP_MSG *msg;
	
	// 送信要求フレームインデックスを更新
	this->req_frame_w_idx = frame_w_idx;
	this->req_frame_r_idx = frame_r_idx;
	// データがたまったことを通知
	msg = kz_kmalloc(sizeof(SOUND_APP_MSG));
	msg->msg_type = EVENT_SOUND_STA;
	msg->msg_data = (frame_w_idx << 16) | (frame_r_idx << 0);
	kz_send(this->msg_id, sizeof(SOUND_APP_MSG), msg);
}

// 音楽データ開始コールバック (*) タスクコンテキスト
static void wav_sta_callback(WAV_INFO *wav_info, void *vp)
{
	SOUND_APP_CTL *this = (SOUND_APP_CTL*)vp;
	
	// 状態の初期値を設定
	this->is_expected = FALSE;
	
	// チャンネル数は期待通り？
	if (wav_info->ch_num != WAV_CH_MONO) {
		return;
	}
	
	// サンプリング周波数は期待通り？
	if (wav_info->sample_rate != WAV_SAMPLE_RATE_16kHz) {
		return;
	}
	
	// ビット/サンプルは期待通り？
	if (wav_info->bps != WAV_BPS_16BIT) {
		return;
	}
	
	// 状態を更新
	this->is_expected = TRUE;
	
	// 状態を取得
	memcpy(&(this->wav_info), wav_info, sizeof(WAV_INFO));
}

// 音楽データ受信コールバック (*) タスクコンテキスト
static void wav_rcv_callback(int32_t data, void *vp)
{
	SOUND_APP_CTL *this = (SOUND_APP_CTL*)vp;
	uint16_t frame_w_idx, frame_r_idx;
	uint16_t data_idx;
	uint16_t stored_frame_num;
	uint16_t tmp_w_idx, tmp_r_idx;
	uint16_t tmp_cur_frame_idx;
	
	// 期待する音楽データではない場合は即リターン
	if (this->is_expected == FALSE) {
		return;
	}
	
	// (*) 下記は、pcm3060_callback()で書き換えられているので割込み禁止で参照する必要がある。
	//      this->w_idx
	//      this->r_idx
	//      this->cur_frame_idx
	// 割込み禁止
	INTR_DISABLE;
	// ライトインデックスを進める
	tmp_w_idx = this->w_idx + 1;
	if (tmp_w_idx >= SOUND_FRAME_NUM*SOUND_DATA_NUM_PAR_FRAME) {
		tmp_w_idx = 0;
	}
	// リードインデックス取得
	tmp_r_idx = this->r_idx;
	// 現在送信中のフレームインデックス取得
	tmp_cur_frame_idx = this->cur_frame_idx;
	// 割込み禁止解除
	INTR_ENABLE;
	
	// バッファに空きがある？
	if (tmp_w_idx != tmp_r_idx) {
		// データ位置の計算
		frame_w_idx = (uint16_t)(this->w_idx / SOUND_DATA_NUM_PAR_FRAME);
		data_idx = this->w_idx - (frame_w_idx * SOUND_DATA_NUM_PAR_FRAME);
		// データ格納
		this->data[frame_w_idx][data_idx] = data;
		// ライトインデックスを更新
		this->w_idx = tmp_w_idx;
		// 最後に送信したフレームを計算
		frame_r_idx = (uint16_t)(tmp_r_idx / SOUND_DATA_NUM_PAR_FRAME);
		// たまったフレームを計算
		stored_frame_num = sound_app_get_frame_num(frame_w_idx, frame_r_idx);
		// 1回目は一定フレーム数たまるまで待つ
		if ((this->first_send_flag == FALSE) && (stored_frame_num >= SOUND_FIRST_SEND_FRAME_NUM)) {
			// 初回送信フラグを更新
			this->first_send_flag = TRUE;
			// 音声鳴動要求
			sound_app_req_pray(this, frame_w_idx, frame_r_idx);
		// 2回目以降
		} else if ((stored_frame_num >= SOUND_SEND_FRAME_NUM) &&		// 10フレームたまった?
				   (this->req_frame_w_idx == tmp_cur_frame_idx) &&		// 要求したフレームまで送信した?
				   (this->first_send_flag == TRUE)) {					// 2回目以降の送信
			// 音声鳴動要求
			sound_app_req_pray(this, frame_w_idx, frame_r_idx);
		}
	// バッファがいっぱい？
	} else {
		console_str_send((uint8_t*)"sound app buffer full.\n");
		// バッファがいっぱいになることはないでしょう
		;
	}
}

// 音楽終了コールバック (*) タスクコンテキスト
static void wav_end_callback(void *vp) 
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	SOUND_APP_MSG *msg;
	
	// 停止通知
	msg = kz_kmalloc(sizeof(SOUND_APP_MSG));
	msg->msg_type = EVENT_SOUND_STP;
	msg->msg_data = 0;
	kz_send(this->msg_id, sizeof(SOUND_APP_MSG), msg);
}

// 送信完了コールバック (*) 割込みコンテキスト
static void pcm3060_callback(void *vp)
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	uint16_t send_frame_num;
	
	// 送信したフレームを計算
	if (this->req_frame_r_idx > this->req_frame_w_idx) {
		send_frame_num = (SOUND_FRAME_NUM - this->req_frame_r_idx) + this->req_frame_w_idx;
	} else {
		send_frame_num = this->req_frame_w_idx - this->req_frame_r_idx;
	}
	
	// リードインデックスを更新
	this->r_idx += send_frame_num*SOUND_DATA_NUM_PAR_FRAME;
	if (this->r_idx >= SOUND_FRAME_NUM*SOUND_DATA_NUM_PAR_FRAME) {
		this->r_idx = 0;
	}
	// リードフレームインデックスを更新
	this->cur_frame_idx = this->r_idx/SOUND_DATA_NUM_PAR_FRAME;
	
	console_str_send((uint8_t*)"frame end.\n");
}

// 音楽再生開始ハンドラ
static void sound_app_sta(uint32_t par)
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	uint16_t frame_w_idx, frame_r_idx;
	uint32_t frame_cnt = 0;
	
	// 送信開始/終了フレームインデックスを取得
	frame_w_idx = (par & 0xFFFF0000) >> 16;
	frame_r_idx = (par & 0x0000FFFF) >> 0;
	
	// 送信フレーム数を計算
	if (frame_r_idx > frame_w_idx) {
		frame_cnt = (SOUND_FRAME_NUM - frame_r_idx) + frame_w_idx;
	} else {
		frame_cnt = frame_w_idx - frame_r_idx;
	}
	console_str_send((uint8_t*)"frame_r_idx:");
	console_val_send((uint8_t)frame_r_idx);
	console_str_send((uint8_t*)" frame_w_idx:");
	console_val_send((uint8_t)frame_w_idx);
	console_str_send((uint8_t*)"\n");
	
	// 送信
	pcm3060_play(this->data[frame_r_idx], SOUND_DATA_NUM_PAR_FRAME*4*frame_cnt);
}

// 音楽再生開始ハンドラ(beep音)
static void sound_app_sta_beep(uint32_t par)
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	uint32_t play_time = (par & 0x0000FFFF) >> 0;
	uint32_t play_type = (par & 0xFFFF0000) >> 16;
	const int32_t *data;
	uint32_t i, j;
	uint32_t frame_cnt;
	
	// 開始
	console_str_send((uint8_t*)"beep start\n");
	
	// 音声データを取得
	data = beep_data[play_type];
	
	// フレーム数を計算
	frame_cnt = play_time / SOUND_TIME_PER_FRAME;
	
	// 音声データを格納
	for (i = 0; i < frame_cnt; i++) {
		for (j = 0; j < SOUND_TIME_PER_FRAME; j++) {
			memcpy(&(this->data[i][j*16]), data, sizeof(beep_1kHz_1ms));
		}
	}
	
	// 送信
	pcm3060_play(this->data[0], frame_cnt*SOUND_DATA_NUM_PAR_FRAME*4);
}

// 音楽再生停止ハンドラ
static void sound_app_stp(uint32_t par)
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	
	// バッファクリア
	memset(this->data[0], 0, sizeof(this->data));
	// インデックス初期化
	this->w_idx = 0;
	this->r_idx = 0;
	this->req_frame_w_idx = 0;
	this->req_frame_r_idx = 0;
	this->cur_frame_idx = 0;
	this->first_send_flag = 0;
}

// 状態遷移テーブル
static const FSM fsm[ST_MAX][EVENT_MAX] = {
	// EVENT_SOUND_STA					EVENT_SOUND_STA_BEEP					EVENT_SOUND_STP
	{{NULL,				ST_UNDEIFNED}, {NULL,					ST_UNDEIFNED}, {NULL,				ST_UNDEIFNED}},		// ST_UNINITIALIZED
	{{sound_app_sta,	ST_RUN		}, {sound_app_sta_beep,		ST_RUN		}, {NULL,				ST_UNDEIFNED}},		// ST_IDLE
	{{sound_app_sta,	ST_UNDEIFNED}, {sound_app_sta_beep,		ST_UNDEIFNED}, {sound_app_stp,		ST_IDLE		}},		// ST_RUN
	{{NULL,				ST_UNDEIFNED}, {NULL,					ST_UNDEIFNED}, {NULL,				ST_UNDEIFNED}},		// ST_UNDEIFNED
};

// サウンドアプリ
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
			fsm[this->state][msg->msg_type].func(tmp_msg.msg_data);
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
	
	// wavマネージャをオープン
	ret = wav_open(wav_sta_callback, wav_rcv_callback, wav_end_callback, this);
	
	// オーディオコーデックのデバイスドライバをオープン
	ret = pcm3060_open(SAMPLING_FREQUENCY, SOUND_DATA_WIDTH, pcm3060_callback, this);
	
	// タスクの生成
	this->tsk_id = kz_run(sound_app_main, "sound_app_main",  SOUND_APP_PRI, SOUND_APP_STACK, 0, NULL);
	
	// 状態の更新
	this->state = ST_IDLE;
	
	return;
}

// ビープ音通知関数
int32_t sound_app_play_beep(uint32_t play_info)
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	SOUND_APP_MSG *msg;
	
	// 初期かれていない場合はエラーを返して終了
	if (this->state == ST_UNINITIALIZED) {
		return -1;
	}
	
	msg = kz_kmalloc(sizeof(SOUND_APP_MSG));
	msg->msg_type = EVENT_SOUND_STA_BEEP;
	msg->msg_data = play_info;
	
	return kz_send(this->msg_id, sizeof(SOUND_APP_MSG), msg);
}

// 音楽データ通知関数
int32_t sound_app_play(void)
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	SOUND_APP_MSG *msg;
	
	msg = kz_kmalloc(sizeof(SOUND_APP_MSG));
	msg->msg_type = EVENT_SOUND_STA;
	msg->msg_data = 0;
	
	return kz_send(this->msg_id, sizeof(SOUND_APP_MSG), msg);
}

// コマンド
static void sound_app_cmd_beep_1000ms(void)
{
	// 1000msbeep音を流す
	sound_app_play_beep(BEEP_PLAY_TIME_1000ms | BEEP_PLAY_1KHZ);
}

// コマンド
static void sound_app_cmd_beep_3000ms(void)
{
	// 3000msbeep音を流す
	sound_app_play_beep(BEEP_PLAY_TIME_3000ms | BEEP_PLAY_1KHZ);
}

// コマンド
static void sound_app_cmd_beep_0kHz(void)
{
	// 3000msbeep音を流す
	sound_app_play_beep(BEEP_PLAY_TIME_1000ms | BEEP_PLAY_0KHZ);
}

// コマンド
static void sound_app_cmd_stop(void)
{
	SOUND_APP_CTL *this = &sound_app_ctl;
	SOUND_APP_MSG *msg;
	
	// バッファクリア
	memset(this->data[0], 0, sizeof(this->data));
	// インデックス初期化
	this->w_idx = 0;
	this->r_idx = 0;
	this->req_frame_w_idx = 0;
	this->req_frame_r_idx = 0;
	this->cur_frame_idx = 0;
	this->first_send_flag = 0;
	
	// 停止通知
	msg = kz_kmalloc(sizeof(SOUND_APP_MSG));
	msg->msg_type = EVENT_SOUND_STP;
	msg->msg_data = 0;
	kz_send(this->msg_id, sizeof(SOUND_APP_MSG), msg);
}

// コマンド設定関数
void sound_app_set_cmd(void)
{
	COMMAND_INFO cmd;
	
	// コマンドの設定
	cmd.input = "sound beep 1000ms";
	cmd.func = sound_app_cmd_beep_1000ms;
	console_set_command(&cmd);
	cmd.input = "sound beep 3000ms";
	cmd.func = sound_app_cmd_beep_3000ms;
	console_set_command(&cmd);
	cmd.input = "sound 0kHz";
	cmd.func = sound_app_cmd_beep_0kHz;
	console_set_command(&cmd);
	cmd.input = "sound stop";
	cmd.func = sound_app_cmd_stop;
	console_set_command(&cmd);
}
