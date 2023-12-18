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
	
	console_str_send("callback\n");
	
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
	ret = ts_mng_reg_callback(TS_CALLBACK_TYPE_SINGLE, ts_mng_callback, NULL);
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
	uint8_t i, j;
	uint32_t idx;
	
	// 初期化
	init();
	
	while(1) {
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);
		
		// 覚えておく
		this->x[this->get_cnt] = msg->x;
		this->y[this->get_cnt] = msg->y;
		
		// 表示
		console_str_send("app x:");
		console_val_send_u16(this->x[this->get_cnt]);
		console_str_send(" y:");
		console_val_send_u16(this->y[this->get_cnt]);
		console_str_send("\n");
		
		// メッセージを解放
		kz_kmfree(msg);
		
		// 指定回数取得した
		if (++this->get_cnt >= BUFFRING_NUM) {
			// 取得回数クリア
			this->get_cnt = 0;
			// 軌跡を書く
			for (i = 0; i < BUFFRING_NUM; i++) {
				idx = ((MSP2807_DISPLAY_HEIGHT - this->y[i])*MSP2807_DISPLAY_WIDTH) + this->x[i];
				this->disp_data[idx] = 0x0000;
				// 補間
				if (i != (BUFFRING_NUM - 1)) {
					for (j = 1; j <= INTERPOLATION_NUM; j++) {
						idx = (this->y[i+1] - this->y[i])/INTERPOLATION_NUM
					}
					
					
					
					
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
	int32_t ret;
	
	// コンテキスト初期化
	memset(this, 0, sizeof(TEST2_CTL));
	
	// タスク作成
	this->tsk_id = kz_run(test_app_main, "test_app_main",  3, 0x1000, 0, NULL);
	
	// メッセージID設定
	this->msg_id = MSGBOX_ID_TEST;
}


