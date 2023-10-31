/*
 * gysfdmaxb.c
 *
 *  Created on: 2023/11/1
 *      Author: ronald
 */
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "intr.h"
#include <string.h>

#include "w25q20ew.h"

// 状態
#define ST_UNINITIALIZED	(0)		// 未初期化
#define ST_INITIALIZED		(1)		// 初期化
#define ST_IDLE				(2)		// アイドル
#define ST_RUNNING			(3)		// 実行中
#define ST_MAX				(4)

// マクロ
#define GYSFDMAXB_USE_UART_CH			OCTOSPI_CH_2	// 使用するUSARTのチャネル
#define GYSFDMAXB_USE_UART_BAUDRATE		(9600)			// 9600

// 制御用ブロック
typedef struct {
	uint32_t			state;					// 状態
	GYSFDMAXB_CALLBACK	callback_fp;			// コールバック
	void				*callback_vp;			// コールバックパラメータ
} GYSFDMAXB_CTL;
static GYSFDMAXB_CTL gysfdmaxb_ctl;

// OCTOSPIのコールバック (*) 今のところ未使用
static void usart_callback(void *vp)
{
	GYSFDMAXB_CTL *this = (GYSFDMAXB_CTL*)vp;
}

// 受信タスク
static int gysfdmaxb_rcv_main(int argc, char *argv[])
{
	BT_CTL *this = &bt_ctl;
	uint8_t data;
	int32_t size;
	
	while (1) {
		// 要求する受信サイズ分、受信するまで待つ
		size = usart_recv(GYSFDMAXB_USE_UART_CH, &data, 1);
		// 期待したサイズ読めた？
		if (size != 1) {
			; // 特に何もしない たぶん期待したサイズ読めるでしょう
		}
		
	}
	
	return 0;
}



// 初期化関数
int32_t gysfdmaxb_init(void)
{
	GYSFDMAXB_CTL *this = &gysfdmaxb_ctl;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(GYSFDMAXB_CTL));
	
	// usartオープン
	ret = usart_open(GYSFDMAXB_USE_UART_CH, GYSFDMAXB_USE_UART_BAUDRATE);
	// usartをオープンできなかったらエラーを返して終了
	if (ret != 0) {
		return -1;
	}
	
	// タスクの生成
	this->tsk_rcv_id = kz_run(gysfdmaxb_rcv_main, "gysfdmaxb_rcv_main",  GYSFDMAXB_PRI, GYSFDMAXB_STACK, 0, NULL);
	
	// 状態の更新
	this->state = ST_INITIALIZED;
	
	return 0;
}

// コマンド設定関数
void gysfdmaxb_set_cmd(void)
{
	COMMAND_INFO cmd;
	
	// コマンドの設定
	cmd.input = "";
	cmd.func = ;
	console_set_command(&cmd);
}
