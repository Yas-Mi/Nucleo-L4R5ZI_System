/*
 * test_app.c
 *
 *  Created on: 2023/12/18
 *      Author: ronald
 */
// Includes
#include <console.h>
#include <string.h>
#include "stm32l4xx.h"
#include "defines.h"
#include "kozos.h"
#include "interrupt.h"
#include "MSP2807.h"
#include "touch_screen.h"

#include "test_app.h"

// マクロ
#define BUFFRING_NUM		(2)		// 座標をバッファリングする数

// メッセージ
#define MSG_TOUCH_START		(0)
#define MSG_TOUCHING 		(1)
#define MSG_RELEASE			(2)
#define MSG_SINGLE_CLICK 	(3)
#define MSG_MAX			 	(4)

// contxt for test2
typedef struct {
	kz_msgbox_id_t		msg_id;				// メッセージID
	kz_thread_id_t		tsk_id;				// タスクID
	uint16_t 			disp_data[MSP2807_DISPLAY_WIDTH*MSP2807_DISPLAY_HEIGHT];
											// 描画データ
	uint16_t			x[BUFFRING_NUM];	// x座標
	uint16_t			y[BUFFRING_NUM];	// y座標
	uint8_t				get_cnt;			// 座標取得回数
	uint8_t				first_flag;			// 初回フラグ
} TEST2_CTL;
static TEST2_CTL test2_ctl;

// メッセージ定義
typedef struct {
	uint32_t type;
	uint16_t x;
	uint16_t y;
} MSG_INFO;

// 関数
typedef void (*TEST_FUNC)(uint16_t x, uint16_t y);

// タッチ開始コールバック
static void ts_mng_touch_start_callback(TS_CALLBACK_TYPE type, uint16_t x, uint16_t y, void *vp)
{
	MSG_INFO *msg;
	TEST2_CTL *this = &test2_ctl;
	
	console_str_send("touch start\n");
	
	// メッセージ送信
	msg = kz_kmalloc(sizeof(MSG_INFO));
	msg->type = MSG_TOUCH_START;
	msg->x = x;
	msg->y = y;
	kz_send(this->msg_id, sizeof(MSG_INFO), msg);
}

// タッチ中コールバック
static void ts_mng_touching_callback(TS_CALLBACK_TYPE type, uint16_t x, uint16_t y, void *vp)
{
	MSG_INFO *msg;
	TEST2_CTL *this = &test2_ctl;
	
	console_str_send("touching\n");
	
	// メッセージ送信
	msg = kz_kmalloc(sizeof(MSG_INFO));
	msg->type = MSG_TOUCHING;
	msg->x = x;
	msg->y = y;
	kz_send(this->msg_id, sizeof(MSG_INFO), msg);
}

// タッチ中からの離しコールバック
static void ts_mng_release_callback(TS_CALLBACK_TYPE type, uint16_t x, uint16_t y, void *vp)
{
	MSG_INFO *msg;
	TEST2_CTL *this = &test2_ctl;
	
	console_str_send("release\n");
	
	// メッセージ送信
	msg = kz_kmalloc(sizeof(MSG_INFO));
	msg->type = MSG_RELEASE;
	msg->x = x;
	msg->y = y;
	kz_send(this->msg_id, sizeof(MSG_INFO), msg);
}

// シングルクリックコールバック
static void ts_mng_single_click_callback(TS_CALLBACK_TYPE type, uint16_t x, uint16_t y, void *vp)
{
	MSG_INFO *msg;
	TEST2_CTL *this = &test2_ctl;
	
	console_str_send("single click\n");
	
	// メッセージ送信
	msg = kz_kmalloc(sizeof(MSG_INFO));
	msg->type = MSG_SINGLE_CLICK;
	msg->x = x;
	msg->y = y;
	kz_send(this->msg_id, sizeof(MSG_INFO), msg);
}

// 初期化
static void init(void)
{
	TEST2_CTL *this = &test2_ctl;
	int32_t ret;
	
	// MSP2807オープン
	msp2807_open();
	// コールバック登録
	ret = ts_mng_reg_callback(TS_CALLBACK_TYPE_TOUCH_START, ts_mng_touch_start_callback, NULL);
	if (ret != E_OK) {
		console_str_send("ts_mng_reg_callback error\n");
	}
	ret = ts_mng_reg_callback(TS_CALLBACK_TYPE_TOUCHING, ts_mng_touching_callback, NULL);
	if (ret != E_OK) {
		console_str_send("ts_mng_reg_callback error\n");
	}
	ret = ts_mng_reg_callback(TS_CALLBACK_TYPE_RELEASE, ts_mng_release_callback, NULL);
	if (ret != E_OK) {
		console_str_send("ts_mng_reg_callback error\n");
	}
	ret = ts_mng_reg_callback(TS_CALLBACK_TYPE_SINGLE_CLICK, ts_mng_single_click_callback, NULL);
	if (ret != E_OK) {
		console_str_send("ts_mng_reg_callback error\n");
	}
	// 白書き込み
	memset(this->disp_data, 0xFF, sizeof(this->disp_data));
	msp2807_write(this->disp_data);
}

// 描画
static void draw_line(uint16_t x, uint16_t y)
{
	TEST2_CTL *this = &test2_ctl;
	uint16_t xn, yn;
	uint16_t start_x, start_y;
	uint16_t end_x, end_y;
	uint32_t idx[9];
	uint8_t i, j, k;
	float a;
	int16_t b;
	
#if 0
	// 覚えておく
	this->x[this->get_cnt] = x;
	this->y[this->get_cnt] = y;

	// 指定回数取得した
	if (++this->get_cnt >= BUFFRING_NUM) {
		// 取得回数クリア
		this->get_cnt = 0;
		// 軌跡を書く
		// (*) 1つ先の座標まで線を書くため、(BUFFRING_NUM - 1)
		for (i = 0; i < (BUFFRING_NUM - 1) ; i++) {
			// 小さいほうを開始位置、大きいほうを終了位置にする
			if (this->x[i] >  this->x[i+1]) {
				start_x =  this->x[i+1];
				end_x =  this->x[i];
			} else {
				start_x =  this->x[i];
				end_x =  this->x[i+1];
			}
			// 一次関数の傾きと切片を求める (y = ax + b)
			a = (float)((float)(this->y[i+1] - this->y[i])/(float)(this->x[i+1] - this->x[i]));
			b = (int16_t)((float)this->y[i] - (float)this->x[i] * a);
			// 2点間の描画
			for (xn = start_x; xn < end_x; xn++) {
				yn = (int16_t)(a * (float)xn + (float)b);
				
				// インデックスを計算
				idx = ((MSP2807_DISPLAY_HEIGHT - yn)*MSP2807_DISPLAY_WIDTH) + xn;
				// 念のため
				if (idx >= MSP2807_DISPLAY_WIDTH*MSP2807_DISPLAY_HEIGHT) {
					console_str_send("idx error xn:");
					console_val_send_u16(xn);
					console_str_send(" yn:");
					console_val_send_u16(yn);
					console_str_send("\n");
					continue;
				}
				// 黒書き込み
				this->disp_data[idx] = 0x0000;
			}
		}
		// 描画
		ts_mng_write(this->disp_data);
	}
#endif
	
	// 初回は一転のみ描画
	if (this->first_flag == FALSE) {
		start_x =  x;
		end_x =  x;
		this->first_flag = TRUE;
		
	} else {
		// 小さいほうを開始位置、大きいほうを終了位置にする
		if (this->x[0] >  x) {
			start_x =  x;
			end_x =  this->x[0];
		} else {
			start_x =  this->x[0];
			end_x =  x;
		}
	}
	// 一次関数の傾きと切片を求める (y = ax + b)
	a = (float)((float)(y - this->y[0])/(float)(x - this->x[0]));
	b = (int16_t)((float)y- (float)x * a);
	// 2点間の描画
	for (xn = start_x; xn < end_x; xn++) {
		// xn, yn を算出
		yn = (int16_t)(a * (float)xn + (float)b);
		
		// 上下左右斜めの座標を算出
		idx[0] = ((MSP2807_DISPLAY_HEIGHT - (yn - 1))*MSP2807_DISPLAY_WIDTH) + (xn - 1);
		idx[1] = ((MSP2807_DISPLAY_HEIGHT - (yn - 1))*MSP2807_DISPLAY_WIDTH) + (xn + 0);
		idx[2] = ((MSP2807_DISPLAY_HEIGHT - (yn - 1))*MSP2807_DISPLAY_WIDTH) + (xn + 1);
		idx[3] = ((MSP2807_DISPLAY_HEIGHT - (yn + 0))*MSP2807_DISPLAY_WIDTH) + (xn - 1);
		idx[4] = ((MSP2807_DISPLAY_HEIGHT - (yn + 0))*MSP2807_DISPLAY_WIDTH) + (xn + 0);
		idx[5] = ((MSP2807_DISPLAY_HEIGHT - (yn + 0))*MSP2807_DISPLAY_WIDTH) + (xn + 1);
		idx[6] = ((MSP2807_DISPLAY_HEIGHT - (yn + 1))*MSP2807_DISPLAY_WIDTH) + (xn - 1);
		idx[7] = ((MSP2807_DISPLAY_HEIGHT - (yn + 1))*MSP2807_DISPLAY_WIDTH) + (xn + 0);
		idx[8] = ((MSP2807_DISPLAY_HEIGHT - (yn + 1))*MSP2807_DISPLAY_WIDTH) + (xn + 1);
		
		// 書き込み
		for (i = 0; i < 9; i++) {
			// 念のため
			if (idx[i] >= MSP2807_DISPLAY_WIDTH*MSP2807_DISPLAY_HEIGHT) {
				console_str_send("idx error xn:");
				console_val_send_u16(xn);
				console_str_send(" yn:");
				console_val_send_u16(yn);
				console_str_send("\n");
				continue;
			}
			// 黒書き込み
			this->disp_data[idx[i]] = 0x0000;
		}
	}
	
	// 描画
	ts_mng_write(this->disp_data);
	
	this->x[0] = x;
	this->y[0] = y;
}

// 初期化
static void initialize(uint16_t x, uint16_t y)
{
	TEST2_CTL *this = &test2_ctl;
	
	// 取得回数クリア
	this->get_cnt = 0;
	// 初回フラグをクリア
	this->first_flag = FALSE;
}

// クリア
static void all_clear(uint16_t x, uint16_t y)
{
	TEST2_CTL *this = &test2_ctl;
	
	// 白にする
	memset(this->disp_data, 0xFF, sizeof(this->disp_data));
	// 描画
	ts_mng_write(this->disp_data);
}

static const TEST_FUNC fp[MSG_MAX] = {
	NULL,			// MSG_TOUCH_START
	draw_line,		// MSG_TOUCHING
	initialize,		// MSG_RELEASE
	all_clear,		// MSG_SINGLE_CLICK
};

// メインタスク
static int test_app_main(int argc, char *argv[])
{	
	TEST2_CTL *this = &test2_ctl;
	MSG_INFO *msg;
	MSG_INFO tmp_msg;
	int32_t size;
	uint32_t msg_type;
	TEST_FUNC func;
	
	// 初期化
	init();
	
	while(1) {
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);
		// コピー
		memcpy(&tmp_msg, msg, sizeof(MSG_INFO));
		// メッセージを解放
		kz_kmfree(msg);
		
		// メッセージに応じて処理
		func = fp[tmp_msg.type];
		if (func != NULL) {
			func(tmp_msg.x, tmp_msg.y);
		}
		
	}
	
	return 0;
}

// 初期化
void test_init(void)
{
	TEST2_CTL *this = &test2_ctl;
	
	// コンテキスト初期化
	memset(this, 0, sizeof(TEST2_CTL));
	
	// タスク作成
	this->tsk_id = kz_run(test_app_main, "test_app_main",  3, 0x1000, 0, NULL);
	
	// メッセージID設定
	this->msg_id = MSGBOX_ID_TEST;
}
