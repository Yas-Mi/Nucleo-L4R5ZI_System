/*
 * MSP2807.c
 *
 *  Created on: 2023/11/27
 *      Author: User
 */
#include <string.h>
#include "defines.h"
#include "kozos.h"
#include "bt_dev.h"
#include "console.h"
#include "ctl_main.h"
#include "intr.h"
#include "tim.h"
#include "spi.h"

#include "MSP2807.h"

// マクロ
#define MSP2807_SPI_BAUDRATE	(1*1000*1000)	// 通信速度 1MHz
#define MSP2807_USW_SPI_CH		(SPI_CH1)		// 使用チャネル

// コマンド
#define MSP2807_CMD_SOFTWARE_RESET			(0x01)
#define MSP2807_CMD_SLEEP_OUT				(0x11)
#define MSP2807_CMD_NORMAL_DISPLAY_ON		(0x13)
#define MSP2807_CMD_DISPLAY_ON				(0x29)
#define MSP2807_CMD_COLUMN_ADDRESS_SET		(0x2A)
#define MSP2807_CMD_PAGE_ADDRESS_SET		(0x2B)
#define MSP2807_CMD_MEMORY_WRITE			(0x2C)
#define MSP2807_CMD_MEMORY_ACCESS_CONTROL	(0x36)
#define MSP2807_CMD_PIXEL_FOMART_SET		(0x3A)

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

// GPIO情報
typedef enum {
	MSP2807_GPIO_TYPE_RESET = 0,
	MSP2807_GPIO_TYPE_DCX,
	MSP2807_GPIO_TYPE_MAX,
} MSP2807_GPIO_TYPE;
typedef struct {
	GPIO_TypeDef	*pin_group;
	uint32_t		pin;
} MSP2807_GPIO_CFG;

// GPIO情報テーブル
static const MSP2807_GPIO_CFG gpio_cfg[MSP2807_GPIO_TYPE_MAX] = {
	{GPIOA, GPIO_PIN_8},	// リセットピン
	{GPIOC, GPIO_PIN_9},	// DC/RS
};
#define gpio_reset(val)	HAL_GPIO_WritePin(gpio_cfg[MSP2807_GPIO_TYPE_RESET].pin_group, gpio_cfg[MSP2807_GPIO_TYPE_RESET].pin, val)
#define gpio_dcrs(val)	HAL_GPIO_WritePin(gpio_cfg[MSP2807_GPIO_TYPE_DCX].pin_group, gpio_cfg[MSP2807_GPIO_TYPE_DCX].pin, val)

// オープンパラメータ
static const SPI_OPEN spi_open_par = {
	MSP2807_SPI_BAUDRATE,		// 通信速度
	SPI_CPOL_POSITIVE,			// Clock Polarity
	SPI_CPHA_FIRST_EDGE,    	// Clock Phase
	SPI_FRAME_FMT_MSB_FIRST,	// フレームフォーマット
	SPI_DATA_SIZE_8BIT,			// データサイズ
};

// MSP2807セットアップ
// https://github.com/YuruPuro/MSP2807/blob/main/ILI9341/ILI9341-DEMO-SIMPLE/ILI9341-DEMO-SIMPLE.ino
static int32_t msp2807_setup(void)
{
	int32_t ret;
	uint8_t snd_data;
	
	// コマンドモード
	gpio_dcrs(GPIO_PIN_RESET);
	
	// ソフトウェアリセット
	snd_data = MSP2807_CMD_SOFTWARE_RESET;
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data, 1, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	kz_tsleep(150);
	
	// Normal Display Mode ON
	snd_data = MSP2807_CMD_NORMAL_DISPLAY_ON;
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data, 1, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	// Memory Access Control
	snd_data = MSP2807_CMD_MEMORY_ACCESS_CONTROL;
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data, 1, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	// データモード
	gpio_dcrs(GPIO_PIN_SET);
	
	// データ送信
	snd_data = 0x48;	// MX = ON , GBR = ON (縦画面,左から右に表示)
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data, 1, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	// コマンドモード
	gpio_dcrs(GPIO_PIN_RESET);
	
	// Memory Access Control
	snd_data = MSP2807_CMD_PIXEL_FOMART_SET;
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data, 1, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	// データモード
	gpio_dcrs(GPIO_PIN_SET);
	
	// データ送信
	snd_data = 0x55;	// 16Bits / 1Pixcel
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data, 1, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	// コマンドモード
	gpio_dcrs(GPIO_PIN_RESET);
	
	// Sleep Out
	snd_data = MSP2807_CMD_SLEEP_OUT;
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data, 1, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	kz_tsleep(60);
	
	// Display ON
	snd_data = MSP2807_CMD_DISPLAY_ON;
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data, 1, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
MSP2807_SETUP_EXIT:	
	return ret;
}

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
	ret = spi_open(MSP2807_USW_SPI_CH, &spi_open_par);
	if (ret != E_OK) {
		console_str_send("spi_open error\n");
		goto MSP2807_OPEN_EXIT;
	}
	
	// MSP2807リセット解除
	gpio_reset(GPIO_PIN_SET);
	kz_tsleep(20);	// (*) このディレイは必要らしい
	
	// MSP2807設定
	ret = msp2807_setup();
	if (ret != E_OK) {
		console_str_send("msp2807_setup error\n");
		goto MSP2807_OPEN_EXIT;
	}
	
	// 状態の更新
	this->state = ST_IDLE;
	
MSP2807_OPEN_EXIT:
	return ret;
}

// 描画関数
int32_t msp2807_write(uint16_t *disp_data)
{
	MSP2807_CTL *this = &msp2807_ctl;
	uint8_t snd_data[2];
	uint32_t x, y;
	uint16_t pixel;
	int32_t ret;
	
	// IDLEでなければ終了
	if (this->state != ST_IDLE) {
		return -1;
	}
	
	// コマンドモード
	gpio_dcrs(GPIO_PIN_RESET);
	
	// Column Address Set
	snd_data[0] = MSP2807_CMD_COLUMN_ADDRESS_SET;
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data[0], 1, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// データモード
	gpio_dcrs(GPIO_PIN_SET);
	
	// データ送信
	snd_data[0] = 0x00;
	snd_data[1] = 0x00;	// 開始カラム 0
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data[0], 2, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// データ送信
	snd_data[0] = 0x00;
	snd_data[1] = 0xEF;	// 終了カラム 239
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data[0], 2, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// コマンドモード
	gpio_dcrs(GPIO_PIN_RESET);
	
	// Page Address Set
	snd_data[0] = MSP2807_CMD_PAGE_ADDRESS_SET;
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data[0], 1, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// データモード
	gpio_dcrs(GPIO_PIN_SET);
	
	// データ送信
	snd_data[0] = 0x00;
	snd_data[1] = 0x00;	// 開始ページ 0
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data[0], 2, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// データ送信
	snd_data[0] = 0x01;
	snd_data[1] = 0x3F;	// 終了ページ 319
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data[0], 2, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// コマンドモード
	gpio_dcrs(GPIO_PIN_RESET);
	
	// Memory Write
	snd_data[0] = MSP2807_CMD_MEMORY_WRITE;
	ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data[0], 1, NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// データモード
	gpio_dcrs(GPIO_PIN_SET);
	
	// 描画
	ret = spi_send_recv(MSP2807_USW_SPI_CH, (uint8_t*)disp_data, (MSP2807_DISPLAY_HEIGHT*MSP2807_DISPLAY_WIDTH*2), NULL, 0);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
#if 0
	// 描画
	for (y = 0; y < MSP2807_DISPLAY_HEIGHT; y++) {
		for (x = 0; x < MSP2807_DISPLAY_WIDTH; x++) {
			pixel = disp_data[y*MSP2807_DISPLAY_HEIGHT+x];
			snd_data[0] = (uint8_t)(pixel & 0x00FF);
			snd_data[1] = (uint8_t)((pixel & 0xFF00) >> 8);
			ret = spi_send_recv(MSP2807_USW_SPI_CH, &snd_data[0], 2, NULL, 0);
			if (ret != E_OK) {
				goto MSP2807_WRITE_EXIT;
			}
		}
	}
#endif
	
MSP2807_WRITE_EXIT:
	return ret;
}
