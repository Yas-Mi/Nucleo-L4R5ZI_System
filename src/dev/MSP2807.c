/*
 * MSP2807.c
 *
 *  Created on: 2023/11/27
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "bt_dev.h"
#include "console.h"
#include "ctl_main.h"
#include "intr.h"
#include "tim.h"
#include <string.h>

#include "spi.h"

// マクロ
#define MSP2807_SPI_BAUDRATE	(1*1000*1000)	// 通信速度 1MHz
#define MSP2807_USW_SPI_CH		(SPI_CH1)		// 使用チャネル

// 状態
#define ST_UNINITIALIZED	(0)		// 未初期化
#define ST_INITIALIZED		(1)		// 初期化
#define ST_IDLE				(2)		// アイドル
#define ST_RUNNING			(3)		// 実行中
#define ST_MAX				(4)

// 制御用ブロック
typedef struct {
	uint32_t			state;					// 状態
} MSP2807_CTL;
static MSP2807_CTL msp2807_ctl;

// オープンパラメータ
static const SPI_OPEN spi_open = {
	MSP2807_SPI_BAUDRATE,		// 通信速度
	SPI_CPOL_POSITIVE,			// Clock Polarity
	SPI_CPHA_FIRST_EDGE,    	// Clock Phase
	SPI_FRAME_FMT_MSB_FIRST,	// フレームフォーマット
	SPI_DATA_SIZE_8BIT,			// データサイズ
};

// 初期化関数
int32_t msp2807_init(void)
{
	MSP2807_CTL *this = &msp2807_ctl;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(MSP2807_CTL));
	
	// 状態の更新
	this->state = ST_INITIALIZED;
	
	return E_OK;
}

// 初期化関数
int32_t msp2807_open(void)
{
	MSP2807_CTL *this = &msp2807_ctl;
	int32_t ret;
	
	// 初期化済みでなければ終了
	if (this->state != ST_INITIALIZED) {
		return -1;
	}
	
	// SPIをオープン
	ret = spi_open(MSP2807_USW_SPI_CH, &spi_open);
	if (ret != E_OK) {
		console_str_send("spi open error\n");
		goto MSP2807_OPEN_EXIT;
	}
	
	// 状態の更新
	this->state = ST_IDLE;
	
MSP2807_OPEN_EXIT:
	return ret;
}
