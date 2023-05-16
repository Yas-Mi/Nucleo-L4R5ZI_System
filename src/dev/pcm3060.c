/*
 * pcm3060.c
 *
 *  Created on: 2023/1/6
 *      Author: User
 */
/*
	PCM30601の設計メモ
	対応フォーマット
		24bit左
		24bitI2S
		24bit右
		16bit右
*/
#include "defines.h"
#include "kozos.h"
#include "i2c_wrapper.h"
#include "lib.h"
#include "stm32l4xx_hal_gpio.h"
#include "sai.h"

// 状態
#define PCM3060_ST_NOT_INTIALIZED	(0U) // 未初期状態
#define PCM3060_ST_INITIALIZED		(1U) // 初期化済み
#define PCM3060_ST_OPEND			(2U) // オープン済み

// 使用するペリフェラルのチャネル
#define PCM3060_USE_I2C_CH		I2C_CH2 // IICのCH2を使用
#define PCM3060_USE_SAI_CH		SAI_CH1 // SAIのCH1を使用

// PCM3060デバイスのスレーブアドレス
#define PCM3060_SLAVE_ADDRESS	(0x46)

// アドレスの定義
#define PCM3060_REGISTER64	(0x40)
#define PCM3060_REGISTER65	(0x41)
#define PCM3060_REGISTER66	(0x42)
#define PCM3060_REGISTER67	(0x43)
#define PCM3060_REGISTER68	(0x44)
#define PCM3060_REGISTER69	(0x45)
#define PCM3060_REGISTER70	(0x46)
#define PCM3060_REGISTER71	(0x47)
#define PCM3060_REGISTER72	(0x48)
#define PCM3060_REGISTER73	(0x49)

// 制御用ブロック
typedef struct {
	uint8_t status;		// 状態
}PCM3060_CTL;
static PCM3060_CTL pcm3060_ctl;

// PCM3060の設定情報
typedef struct {
	uint8_t reg;	// レジスタのアドレス
	uint8_t data;	// データ
}PCM3060_SETTING;

// データ幅変換テーブル
static const fs_conv_tbl[SAI_DATA_WIDTH_MAX] = {
	8,		// 8bit
	10,		// 10bit
	16,		// 16bit
	20,		// 20bit
	24,		// 24bit
	32,		// 32bit
};

// SAIのオープンパラメータ
static const SAI_OPEN sai_open_par = {
	SAI_MODE_MONAURAL,		// モノラル
	SAI_FMT_MSB_JUSTIFIED,	// 右詰め
	SAI_PACK_LSB_FIRST,		// LSB First
	0,						// データ幅(後から設定)
	0,						// サンプリング周波数(後から設定)
	TRUE,					// マスタークロックを使用
};

// PCM3060の設定パラメータ
static const PCM3060_SETTING pcm3060_setting[] = {
//	{PCM3060_REGISTER64, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER65, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER66, 0x00}, // デフォルト設定でいいでしょう
	{PCM3060_REGISTER67, 0x03},
//	{PCM3060_REGISTER68, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER69, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER70, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER71, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER72, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER73, 0x00}, // デフォルト設定でいいでしょう
};

// 外部公開関数
// PCM3060初期化関数
void pcm3060_init(void)
{
	PCM3060_CTL *this = &pcm3060_ctl;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(PCM3060_CTL));
	
	// 状態の更新
	this->status = PCM3060_ST_INITIALIZED;
	
	return;
}

int32_t pcm3060_open(uint32_t fs, uint8_t data_width)
{
	PCM3060_CTL *this = &pcm3060_ctl;
	SAI_OPEN open_par;
	uint8_t i;
	int32_t ret;
	
	// I2Cをオープンする
	ret = i2c_wrapper_open(PCM3060_USE_I2C_CH);
	if (ret != 0) {
		return -1;
	}
	
	// SAIのオープンパラメータを設定
	memcpy(&open_par, &sai_open_par, sizeof(SAI_OPEN));
	// 渡されたデータ幅をSAIのオープンパラメータ用に変換
	for (i = 0; i < SAI_DATA_WIDTH_MAX; i++) {
		if (data_width == fs_conv_tbl[i]) {
			break;
		}
	}
	
	// 渡されたデータ幅が有効でない場合はエラーを返して終了
	if (i >= SAI_DATA_WIDTH_MAX) {
		return -1;
	}
	// データ幅とサンプリング周波数を設定
	open_par.width = i;
	open_par.fs = fs;
	// SAIをオープンする
	ret = sai_open(PCM3060_USE_SAI_CH, &open_par);
	if (ret != 0) {
		return -1;
	}
	
	// RST解除
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
	
	// PCM3060の設定
	for (i = 0; i < sizeof(pcm3060_setting)/sizeof(pcm3060_setting[0]); i++) {
		i2c_wrapper_send(PCM3060_USE_I2C_CH, PCM3060_SLAVE_ADDRESS, &pcm3060_setting[i], 2);
	}
	
	// 状態を更新
 	this->status = PCM3060_ST_OPEND;
	
	return 0;
}


