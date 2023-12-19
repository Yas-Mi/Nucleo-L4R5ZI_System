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
#define BUFFRING_NUM		(3)		// 座標をバッファリングする数
#define INTERPOLATION_NUM	(3)		// 座標の2点間の補間する数

// contxt for test2
typedef struct {
	kz_msgbox_id_t		msg_id;				// メッセージID
	kz_thread_id_t		tsk_id;				// タスクID
	uint16_t 			disp_data[MSP2807_DISPLAY_WIDTH*MSP2807_DISPLAY_HEIGHT];
											// 描画データ
	uint16_t			x[BUFFRING_NUM];	// x座標
	uint16_t			y[BUFFRING_NUM];	// y座標
	uint8_t				get_cnt;			// 座標取得回数
} TEST2_CTL;
static TEST2_CTL test2_ctl;

// メッセージ定義
typedef struct {
	uint16_t x;
	uint16_t y;
} MSG_INFO;

// シングルクリックコールバック
static void ts_mng_callback(TS_CALLBACK_TYPE type, uint16_t x, uint16_t y, void *vp)
{
	MSG_INFO *msg;
	TEST2_CTL *this = &test2_ctl;
	
	// メッセージ送信
	msg = kz_kmalloc(sizeof(MSG_INFO));
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
	// シングルクリックコールバック登録
	ret = ts_mng_reg_callback(TS_CALLBACK_TYPE_HOLD_DOWN, ts_mng_callback, NULL);
	if (ret != E_OK) {
		console_str_send("ts_mng_reg_callback error\n");
	}
	// 白書き込み
	memset(this->disp_data, 0xFF, sizeof(this->disp_data));
	msp2807_write(this->disp_data);
}

// メインタスク
static int test_app_main(int argc, char *argv[])
{	
	TEST2_CTL *this = &test2_ctl;
	MSG_INFO *msg;
	int32_t size;
	uint8_t i;
	uint32_t idx;
	uint16_t xn, yn;
	uint16_t start_x, start_y;
	uint16_t end_x, end_y;
	float a;
	int16_t b;
	
	// 初期化
	init();
	
	while(1) {
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);
		
		// 覚えておく
		this->x[this->get_cnt] = msg->x;
		this->y[this->get_cnt] = msg->y;
		
		// メッセージを解放
		kz_kmfree(msg);
#if 0
		// 表示
		console_str_send("app x:");
		console_val_send_u16(this->x[this->get_cnt]);
		console_str_send(" y:");
		console_val_send_u16(this->y[this->get_cnt]);
		console_str_send("\n");
#endif
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
