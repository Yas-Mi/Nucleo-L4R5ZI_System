/*
 * MSP2807.c
 *
 *  Created on: 2023/11/27
 *      Author: User
 */
/*
	メモ
	MSP28097
	LCD コントローラー：ILI9341
	タッチパネルコントローラ：XPT2048 or TSC2046



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
#define MSP2807_SPI_LCD_BAUDRATE	(60*1000*1000)	// 通信速度 60MHz
#define MSP2807_SPI_TOUCH_BAUDRATE	(1*1000*1000)	// 通信速度 1MHz
#define MSP2807_USW_SPI_CH_LCD		(SPI_CH1)		// LCDコントローラの使用チャネル
#define MSP2807_USE_SPI_CH_TOUCH	(SPI_CH2)		// タッチスクリーンコントローラの使用チャネル

// LCDコントローラコマンド
#define MSP2807_LCD_CMD_SOFTWARE_RESET				(0x01)
#define MSP2807_LCD_CMD_SLEEP_OUT					(0x11)
#define MSP2807_LCD_CMD_NORMAL_DISPLAY_ON			(0x13)
#define MSP2807_LCD_CMD_DISPLAY_ON					(0x29)
#define MSP2807_LCD_CMD_COLUMN_ADDRESS_SET			(0x2A)
#define MSP2807_LCD_CMD_PAGE_ADDRESS_SET			(0x2B)
#define MSP2807_LCD_CMD_MEMORY_WRITE				(0x2C)
#define MSP2807_LCD_CMD_MEMORY_ACCESS_CONTROL		(0x36)
#define MSP2807_LCD_CMD_PIXEL_FOMART_SET			(0x3A)

// タッチスクリーンコントローラコマンド
/* コマンドのフォーマット
	+---+----------+------+---------+---------+
	| S | A2 A1 A0 | MODE | SER/DFR | PD1 PD0 |
	+---+----------+------+---------+---------+
	S       : スタートビット
	A2-A0   : チャネル選択ビット
	MODE    : 12/8bit選択ビット
	SER/DFR : シングルエンド/差動リファレンスの選択ビット
	PD1-PD0 : パワーダウン選択ビット
*/
#define MSP2807_TOUCH_CMD_START_BIT					(1 << 7)
#define MSP2807_TOUCH_CMD_A2						(1 << 6)
#define MSP2807_TOUCH_CMD_A1						(1 << 5)
#define MSP2807_TOUCH_CMD_A0						(1 << 4)
#define MSP2807_TOUCH_CMD_MODE						(1 << 3)
#define MSP2807_TOUCH_CMD_SERDFR					(1 << 2)
#define MSP2807_TOUCH_CMD_P1						(1 << 1)
#define MSP2807_TOUCH_CMD_P0						(1 << 0)
#define MSP2807_TOUCH_CMD_X_MEASURE					(MSP2807_TOUCH_CMD_A2|MSP2807_TOUCH_CMD_A0)
#define MSP2807_TOUCH_CMD_Y_MEASURE					(MSP2807_TOUCH_CMD_A0)
#define MSP2807_TOUCH_CMD_REFERENCE_OFF_ADC_ON		(MSP2807_TOUCH_CMD_P0)
#define MSP2807_TOUCH_CMD_REFERENCE_ON_ADC_OFF		(MSP2807_TOUCH_CMD_P1)
#define MSP2807_TOUCH_CMD_ADC_STP_PENDWN_ENABLE		(MSP2807_TOUCH_CMD_START_BIT)
#define MSP2807_TOUCH_CMD_X_ENABLE_PENDWN_DISABLE	(MSP2807_TOUCH_CMD_START_BIT|MSP2807_TOUCH_CMD_X_MEASURE|MSP2807_TOUCH_CMD_REFERENCE_OFF_ADC_ON)
#define MSP2807_TOUCH_CMD_Y_ENABLE_PENDWN_DISABLE	(MSP2807_TOUCH_CMD_START_BIT|MSP2807_TOUCH_CMD_Y_MEASURE|MSP2807_TOUCH_CMD_REFERENCE_OFF_ADC_ON)

// 状態
#define ST_UNINITIALIZED	(0)		// 未初期化
#define ST_INITIALIZED		(1)		// 初期化
#define ST_IDLE				(2)		// アイドル
#define ST_RUNNING			(3)		// 実行中
#define ST_MAX				(4)

// 制御用ブロック
typedef struct {
	uint32_t	lcd_state;		// LCD状態
	uint32_t	touch_state;	// タッチスクリーン状態
} MSP2807_CTL;
static MSP2807_CTL msp2807_ctl;

// GPIO情報
typedef enum {
	MSP2807_GPIO_TYPE_RESET = 0,
	MSP2807_GPIO_TYPE_DCX,
	MSP2807_GPIO_TYPE_PEN,
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
	{GPIOD, GPIO_PIN_15},	// タッチスクリーン PEN
};
#define gpio_reset(val)	HAL_GPIO_WritePin(gpio_cfg[MSP2807_GPIO_TYPE_RESET].pin_group, gpio_cfg[MSP2807_GPIO_TYPE_RESET].pin, val);\
						__DSB();
#define cmd_mode()	HAL_GPIO_WritePin(gpio_cfg[MSP2807_GPIO_TYPE_DCX].pin_group, gpio_cfg[MSP2807_GPIO_TYPE_DCX].pin, GPIO_PIN_RESET); \
					__DSB();
#define data_mode()	HAL_GPIO_WritePin(gpio_cfg[MSP2807_GPIO_TYPE_DCX].pin_group, gpio_cfg[MSP2807_GPIO_TYPE_DCX].pin, GPIO_PIN_SET); \
					__DSB();

// オープンパラメータ(LCD)
static const SPI_OPEN lcd_spi_open_par = {
	MSP2807_SPI_LCD_BAUDRATE,	// 通信速度
	SPI_CPOL_NEGATIVE,			// Clock Polarity
	SPI_CPHA_FIRST_EDGE,    	// Clock Phase
	SPI_FRAME_FMT_MSB_FIRST,	// フレームフォーマット
	SPI_DATA_SIZE_8BIT,			// データサイズ
};

// オープンパラメータ(タッチスクリーン)
static const SPI_OPEN touch_spi_open_par = {
	MSP2807_SPI_TOUCH_BAUDRATE,	// 通信速度
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
	
	// LCDの設定
	// コマンドモード
	cmd_mode();
	
	// ソフトウェアリセット
	snd_data = MSP2807_LCD_CMD_SOFTWARE_RESET;
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, &snd_data, NULL, 1);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	kz_tsleep(150);
	
	// Normal Display Mode ON
	snd_data = MSP2807_LCD_CMD_NORMAL_DISPLAY_ON;
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, &snd_data, NULL, 1);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	// Memory Access Control
	snd_data = MSP2807_LCD_CMD_MEMORY_ACCESS_CONTROL;
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, &snd_data, NULL, 1);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	// データモード
	data_mode();
	
	// データ送信
	snd_data = 0x48;	// MX = ON , GBR = ON (縦画面,左から右に表示)
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, &snd_data, NULL, 1);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	// コマンドモード
	cmd_mode();
	
	// Memory Access Control
	snd_data = MSP2807_LCD_CMD_PIXEL_FOMART_SET;
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, &snd_data, NULL, 1);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	// データモード
	data_mode();
	
	// データ送信
	snd_data = 0x55;	// 16Bits / 1Pixcel
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, &snd_data, NULL, 1);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	// コマンドモード
	cmd_mode();
	
	// Sleep Out
	snd_data = MSP2807_LCD_CMD_SLEEP_OUT;
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, &snd_data, NULL, 1);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	kz_tsleep(60);
	
	// Display ON
	snd_data = MSP2807_LCD_CMD_DISPLAY_ON;
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, &snd_data, NULL, 1);
	if (ret != E_OK) {
		goto MSP2807_SETUP_EXIT;
	}
	
	// タッチスクリーンの設定
	// ADC停止、ペンダウン検出有効
	snd_data = MSP2807_TOUCH_CMD_ADC_STP_PENDWN_ENABLE;
	ret = spi_send_recv(MSP2807_USE_SPI_CH_TOUCH, &snd_data, NULL, 1);
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
	this->lcd_state = ST_INITIALIZED;
	this->touch_state = ST_INITIALIZED;
	
	return E_OK;
}

// オープン関数
int32_t msp2807_open(void)
{
	MSP2807_CTL *this = &msp2807_ctl;
	int32_t ret;
	
	// 初期化済みでなければ終了
	if (this->lcd_state != ST_INITIALIZED) {
		return -1;
	}
	if (this->touch_state != ST_INITIALIZED) {
		return -1;
	}
	
	// SPIをオープン
	// LCD側
	ret = spi_open(MSP2807_USW_SPI_CH_LCD, &lcd_spi_open_par);
	if (ret != E_OK) {
		console_str_send("lcd spi_open error\n");
		goto MSP2807_OPEN_EXIT;
	}
	
	// タッチスクリーン側
	ret = spi_open(MSP2807_USE_SPI_CH_TOUCH, &touch_spi_open_par);
	if (ret != E_OK) {
		console_str_send("touch spi_open error\n");
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
	this->lcd_state = ST_IDLE;
	this->touch_state = ST_IDLE;
	
MSP2807_OPEN_EXIT:
	return ret;
}

// クローズ関数
int32_t msp2807_close(void)
{
	MSP2807_CTL *this = &msp2807_ctl;
	int32_t ret;
	
	// アイドルでなければ終了
	if (this->lcd_state != ST_IDLE) {
		return -1;
	}
	if (this->touch_state != ST_IDLE) {
		return -1;
	}
	
	// SPIのクローズは作ってないので呼ばない
	
	// MSP2807リセット
	gpio_reset(GPIO_PIN_RESET);
	kz_tsleep(20);	// (*) このディレイは必要らしい
	
	// 状態の更新
	this->lcd_state = ST_INITIALIZED;
	this->touch_state = ST_INITIALIZED;
	
MSP2807_CLOSE_EXIT:
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
	if (this->lcd_state != ST_IDLE) {
		return -1;
	}
	
	// 状態を更新
	this->lcd_state = ST_RUNNING;
	
	// コマンドモード
	cmd_mode();
	
	// Column Address Set
	snd_data[0] = MSP2807_LCD_CMD_COLUMN_ADDRESS_SET;
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, snd_data, NULL, 1);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// データモード
	data_mode();
	
	// データ送信
	snd_data[0] = 0x00;
	snd_data[1] = 0x00;	// 開始カラム 0
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, snd_data, NULL, 2);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// データ送信
	snd_data[0] = 0x00;
	snd_data[1] = 0xEF;	// 終了カラム 239
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, snd_data, NULL, 2);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// コマンドモード
	cmd_mode();
	
	// Page Address Set
	snd_data[0] = MSP2807_LCD_CMD_PAGE_ADDRESS_SET;
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, snd_data, NULL, 1);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// データモード
	data_mode();
	
	// データ送信
	snd_data[0] = 0x00;
	snd_data[1] = 0x00;	// 開始ページ 0
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, snd_data, NULL, 2);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// データ送信
	snd_data[0] = 0x01;
	snd_data[1] = 0x3F;	// 終了ページ 319
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, snd_data, NULL, 2);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// コマンドモード
	cmd_mode();
	
	// Memory Write
	snd_data[0] = MSP2807_LCD_CMD_MEMORY_WRITE;
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, snd_data, NULL, 1);
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
	
	// データモード
	data_mode();
	
	// 描画
#if 0
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, (uint8_t*)disp_data, NULL, (MSP2807_DISPLAY_HEIGHT*MSP2807_DISPLAY_WIDTH*2));
	if (ret != E_OK) {
		goto MSP2807_WRITE_EXIT;
	}
#endif
	ret = spi_send_dma(MSP2807_USW_SPI_CH_LCD, (uint8_t*)disp_data, (MSP2807_DISPLAY_HEIGHT*MSP2807_DISPLAY_WIDTH*2));
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
			ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, &snd_data[0], 2, NULL, 0);
			if (ret != E_OK) {
				goto MSP2807_WRITE_EXIT;
			}
		}
	}
#endif
	
MSP2807_WRITE_EXIT:
	
	// 状態を更新
	this->lcd_state = ST_IDLE;
	
	return ret;
}

// タッチ箇所のx,y座標を取得
// https://www.renesas.com/jp/ja/document/apn/723181
int32_t msp2807_get_touch_pos(uint16_t *x, uint16_t *y)
{
	MSP2807_CTL *this = &msp2807_ctl;
	int32_t ret;
	uint8_t snd_data[3];
	uint8_t rcv_data[3];
	
	// NULLチェック
	if ((x == NULL) || (y == NULL)) {
		return E_PAR;
	}
	
	// IDLEでなければ終了
	if (this->touch_state != ST_IDLE) {
		return E_OBJ;
	}
	
	snd_data[0] = MSP2807_TOUCH_CMD_X_ENABLE_PENDWN_DISABLE;
	snd_data[1] = 0; // ダミー
	snd_data[2] = 0; // ダミー;
	ret = spi_send_recv(MSP2807_USE_SPI_CH_TOUCH, snd_data, rcv_data, 3);
	if (ret != E_OK) {
		goto MSP2807_GET_TOUCH_POS_EXIT;
	}
	
	// x座標の設定
	*x = ((((uint16_t)rcv_data[1] << 8) | ((uint16_t)rcv_data[2])) >> 3);
	
	// 安定待ち時間
	kz_tsleep(1);
	
	snd_data[0] = MSP2807_TOUCH_CMD_Y_ENABLE_PENDWN_DISABLE;
	snd_data[1] = 0; // ダミー
	snd_data[2] = 0; // ダミー;
	ret = spi_send_recv(MSP2807_USE_SPI_CH_TOUCH, snd_data, rcv_data, 3);
	if (ret != E_OK) {
		goto MSP2807_GET_TOUCH_POS_EXIT;
	}
	
	// y座標の設定
	*y = ((((uint16_t)rcv_data[1] << 8) | ((uint16_t)rcv_data[2])) >> 3);
	
	// 安定待ち時間
	kz_tsleep(1);
	
MSP2807_GET_TOUCH_POS_EXIT:
	
	// ADC停止，ペン割り込み有効
	snd_data[0] = MSP2807_TOUCH_CMD_ADC_STP_PENDWN_ENABLE;
	ret = spi_send_recv(MSP2807_USE_SPI_CH_TOUCH, snd_data, NULL, 1);
	
	// 安定待ち時間
	kz_tsleep(1);
	
	return ret;
}

// ペンの状態を取得
MSP2807_PEN_STATE msp2807_get_touch_state(void)
{
	MSP2807_CTL *this = &msp2807_ctl;
	const MSP2807_GPIO_CFG *cfg;
	MSP2807_PEN_STATE ret_state = MSP2807_PEN_STATE_RELEASE;
	GPIO_PinState state;
	
	// GPIO情報を取得
	cfg = &gpio_cfg[MSP2807_GPIO_TYPE_PEN];
	
	// リード
	state = HAL_GPIO_ReadPin(cfg->pin_group, cfg->pin);
	
	// lowだったら押された！
	if (state == GPIO_PIN_RESET) { 
		ret_state = MSP2807_PEN_STATE_PUSHED;
	}
	
	return ret_state;
}
