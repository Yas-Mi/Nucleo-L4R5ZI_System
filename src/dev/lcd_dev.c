/*
 * lcd_dev.c
 *
 *  Created on: 2022/10/29
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "i2c_wrapper.h"
#include "lcd_dev.h"
#include "lib.h"

// 状態
#define LCD_ST_NOT_INTIALIZED	(0U) // 未初期化
#define LCD_ST_INITIALIZED		(1U) // 初期化済み

// 使用するI2Cのチャネル
#define LCD_USE_I2C_CH		I2C_CH1 // CH1を使用

// LCDデバイスのスレーブアドレス
#define LCD_SLAVE_ADDRESS	(0x3E)

// LCDへのコマンド
#define CLEAR_DISPLAY_COMMAND		(0x01)
#define SET_DDRAM_ADDRESS_COMMAND	(0x80)

// 送信するデータタイプ
#define CONTROL_TYPE		(0)
#define DATA_CGRAM_TYPE		(1)

// 制御用ブロック
typedef struct {
	uint8_t status;											// 状態
	uint8_t lcd_img[LCD_DISPLAY_WIDTH*LCD_DISPLAY_HEIGHT];	// LCDのイメージRAM
}LCD_CTL;

typedef struct {
	uint16_t sjis; // SJISコード
	uint8_t  ptn;  // LCDの文字パターン
}CHAR_PTN;

// LCD初期化コマンドテーブル
static const uint8_t init_cmd[] = {
	0x38, // Function Set, 2 line mode normal display 
	0x39, // Unction Set, extension instruction be selected
	0x14, // BS=0:1/5 BIAS;F2 F1 F0:100(internal osc) 
	0x7A, // Contrast set 
	0x56, // Icon on,booster circuit on
	0x6C, // Follower circuit on
	0x38, // IS=0 : extention mode cancel
	0x0C, // Entire display on 
	0x01, // Clear display 
	0x06, // Entry Mode Set ,increment 
};

// 文字列パターンテーブル
static const CHAR_PTN char_ptn[] = {
	// 0～9
	{0x824F, 0x30}, {0x8250, 0x31}, {0x8251, 0x32}, {0x8252, 0x33}, {0x8253, 0x34},  // 0～4
	{0x8254, 0x35}, {0x8255, 0x36}, {0x8256, 0x37}, {0x8257, 0x38}, {0x8258, 0x39},  // 5～9
	// A～Z
	{0x8260, 0x41}, {0x8261, 0x42}, {0x8262, 0x43}, {0x8263, 0x44}, {0x8264, 0x45}, // A～E
	{0x8265, 0x46}, {0x8266, 0x47}, {0x8267, 0x48}, {0x8268, 0x49}, {0x8269, 0x4A}, // F～J
	{0x826A, 0x4B}, {0x826B, 0x4C}, {0x826C, 0x4D}, {0x826D, 0x4E}, {0x826E, 0x4F}, // K～O
	{0x826F, 0x50}, {0x8270, 0x51}, {0x8271, 0x52}, {0x8272, 0x53}, {0x8273, 0x54}, // P～T
	{0x8274, 0x55}, {0x8275, 0x56}, {0x8276, 0x57}, {0x8277, 0x58}, {0x8278, 0x59}, // U～Y
	{0x8279, 0x5A},                                                                 // Z
	// a～z
	{0x8281, 0x61}, {0x8282, 0x62}, {0x8283, 0x63}, {0x8284, 0x64}, {0x8285, 0x65}, // a～e
	{0x8286, 0x66}, {0x8287, 0x67}, {0x8288, 0x68}, {0x8289, 0x69}, {0x828A, 0x6A}, // f～j
	{0x828B, 0x6B}, {0x828C, 0x6C}, {0x828D, 0x6D}, {0x828E, 0x6E}, {0x828F, 0x6F}, // k～o
	{0x8290, 0x70}, {0x8291, 0x71}, {0x8292, 0x72}, {0x8293, 0x73}, {0x8294, 0x74}, // p～t
	{0x8295, 0x75}, {0x8296, 0x76}, {0x8297, 0x77}, {0x8298, 0x78}, {0x8299, 0x79}, // u～y
	{0x829A, 0x7A},                                                                 // z
	// ア～ン
	{0x8341, 0xB1}, {0x8343, 0xB2}, {0x8345, 0xB3}, {0x8347, 0xB4}, {0x8349, 0xB5}, // ア行
	{0x834A, 0xB6}, {0x834C, 0xB7}, {0x834E, 0xB8}, {0x8350, 0xB9}, {0x8352, 0xBA}, // カ行
	{0x8354, 0xBB}, {0x8356, 0xBC}, {0x8358, 0xBD}, {0x835A, 0xBE}, {0x835C, 0xBF}, // サ行
	{0x835E, 0xC0}, {0x8360, 0xC1}, {0x8363, 0xC2}, {0x8365, 0xC3}, {0x8367, 0xC4}, // タ行
	{0x8369, 0xC5}, {0x836A, 0xC6}, {0x836B, 0xC7}, {0x836C, 0xC8}, {0x836D, 0xC9}, // ナ行
	{0x836E, 0xCA}, {0x8371, 0xCB}, {0x8374, 0xCC}, {0x8377, 0xCD}, {0x837A, 0xCE}, // ハ行
	{0x837D, 0xCF}, {0x837E, 0xD0}, {0x8380, 0xD1}, {0x8381, 0xD2}, {0x8382, 0xD3}, // マ行
	{0x8384, 0xD4}, {0x8386, 0xD5}, {0x8388, 0xD6},                                 // ヤ行
	{0x8389, 0xD7}, {0x838A, 0xD8}, {0x838B, 0xD9}, {0x838C, 0xDA}, {0x838D, 0xDB}, // ラ行
	{0x838F, 0xDC}, {0x8392, 0xA6}, {0x8393, 0xDD},                                 // ワ行
	{0x8340, 0xA7}, {0x8342, 0xA8}, {0x8343, 0xA9}, {0x8346, 0xAA}, {0x8348, 0xAB}, // ァ行
	{0x8383, 0xAC}, {0x8385, 0xAD}, {0x8387, 0xAE},                                 // ャ行
	// 記号
	{0x8140, 0x20},                                                                 // 　
	{0x814A, 0xDE},                                                                 // ゛
	{0x814B, 0xDF},                                                                 // ゜
	{0x8144, 0x2E},                                                                 //．
	{0x815B, 0x2D},                                                                 //ー	
	{0x8183, 0x3C},                                                                 //＜
	{0x8184, 0x3E},                                                                 //＞
};

static LCD_CTL lcd_ctl; // 制御ブロックの実体

//内部関数
// I2Cへデータを送信する関数
static void lcd_send_data(uint32_t type, const uint8_t *send_data, uint8_t size) 
{
	uint8_t data[2];
	uint8_t i;
	
	// 送信するデータによって最初に送るコマンドを決定する
	switch(type) {
		case CONTROL_TYPE:
			data[0] = 0x00;
			break;
		case DATA_CGRAM_TYPE:
			data[0] = 0x40;
			break;
		default:
			break;
	}
	
	for (i = 0; i < size; i++) {
		data[1] = send_data[i];
		// データを送信
		i2c_wrapper_send(LCD_USE_I2C_CH, LCD_SLAVE_ADDRESS, data, 2);
		// 5msのディレイ
		kz_tsleep(5);
	}
}

// sjisコードからLCDの文字パターンに変換する関数
static uint8_t lcd_get_ptn(uint16_t sjis)
{
	uint16_t i;
	
	// パターンを検索
	for (i = 0; i < sizeof(char_ptn)/sizeof(char_ptn[0]); i++) {
		// パターンが見つかったらそのパターンを返す
		if (sjis == char_ptn[i].sjis) {
			return char_ptn[i].ptn;
		}
	}
	
	// ありえないパターンを返してエラーとする
	return 0xFF;
}

// 外部公開関数
// LCD初期化関数
void LCD_dev_init(void)
{
	LCD_CTL *this = &lcd_ctl;
	
	// 制御ブロックの初期化
	memset(&lcd_ctl, 0, sizeof(lcd_ctl));
	
	// I2Cをオープンする
	i2c_wrapper_open(LCD_USE_I2C_CH);
	
	// LCDの初期化
	lcd_send_data(CONTROL_TYPE, init_cmd, sizeof(init_cmd)/sizeof(init_cmd[0]));
	
	// 状態の更新
	this->status = LCD_ST_INITIALIZED;
	
	return;
}

// LCD書き込み関数
int8_t LCD_dev_write(uint8_t x, uint8_t y, uint16_t *str)
{
	LCD_CTL *this = &lcd_ctl;
	uint16_t sjis;
	uint16_t ptn;
	uint8_t ofst = 0;
	
	// strがNULLの場合、エラーを返す
	if (str == NULL) {
		return -1;
	}
	
	// 状態が未初期化の場合、エラーを返す
	if (this->status == LCD_ST_NOT_INTIALIZED) {
		return -1;
	}
	
	// LCDの表示位置が範囲外の場合、エラーを返す
	if ((x > LCD_DISPLAY_WIDTH) || (y > LCD_DISPLAY_HEIGHT)) {
		return -1;
	}
	
	// 表示文字が行をまたぐ場合、エラーを返す
	if((x + strlen(str)/2) > LCD_DISPLAY_WIDTH) {
		return -1;
	}
	
	// 文字列をLCDイメージRAMに格納
	while (*str != '\0') {
		sjis = *str;
		// byteが反対になっているため反対にする
		sjis = (uint16_t)((sjis << 8) & 0xFF00) | (uint16_t)((sjis >> 8) & 0x00FF);
		
		// sjisからlcdに表示する文字列のパターンに変換
		ptn = lcd_get_ptn(sjis);
		
		// 0xFFは対応するパターンがないということのため、表示をクリアしてエラーを返す
		if (ptn == 0xFF) {
			// 表示クリア
			LCD_dev_clear_disp();
			return -1;
		}
		
		// 格納箇所が空白(0x20)の場合(上書き防止)
		if (this->lcd_img[(y*LCD_DISPLAY_WIDTH+x)+ofst] != 0x20) {
			// すでに何かしらの文字が格納されている場合はエラーを返す
			return -1;
		} else {
			// 一文字をLCDイメージRAMに格納
			this->lcd_img[(y*LCD_DISPLAY_WIDTH+x)+ofst] = ptn;
		}
		
		// 文字を進める
		str++;
		// オフセットを進める
		ofst++;
	}
	
	return 0;
}

// LCDクリア関数
int8_t LCD_dev_clear(uint8_t x, uint8_t y, uint8_t num)
{
	LCD_CTL *this = &lcd_ctl;
	uint8_t i = 0;
	
	// 状態が未初期化の場合、エラーを返す
	if (this->status == LCD_ST_NOT_INTIALIZED) {
		return -1;
	}
	
	// LCDの表示位置が範囲外の場合、エラーを返す
	if ((x > LCD_DISPLAY_WIDTH) || (y > LCD_DISPLAY_HEIGHT)) {
		return -1;
	}
	
	// 表示文字が行をまたぐ場合、エラーを返す
	if(x + num > LCD_DISPLAY_WIDTH) {
		return -1;
	}
	
	// 空白(0x20)を設定
	for (i = 0; i < num; i++) {
		this->lcd_img[(y * LCD_DISPLAY_WIDTH + x) + i] = 0x20;
	}
	
	return 0;
}

// LCDイメージRAMをクリアする関数
int8_t LCD_dev_clear_disp(void)
{
	LCD_CTL *this = &lcd_ctl;
	uint8_t data = CLEAR_DISPLAY_COMMAND;
	
	// 状態が未初期化の場合、エラーを返す
	if (this->status == LCD_ST_NOT_INTIALIZED) {
		return -1;
	}
	
	// LCDイメージRAMを0x20でクリア (0x20が文字パターンの空白に対応)
	memset(this->lcd_img, 0x20, LCD_DISPLAY_WIDTH*LCD_DISPLAY_HEIGHT);
	
	return 0;
}

// LCDイメージRAMをＡＱＭ１６０２Ｙ－ＮＬＷ－ＢＢＷに表示する関数
int8_t LCD_dev_disp(void)
{
	LCD_CTL *this = &lcd_ctl;
	uint8_t i;
	uint8_t pos;
	
	// 状態が未初期化の場合、エラーを返す
	if (this->status == LCD_ST_NOT_INTIALIZED) {
		return -1;
	}
	
	// LCDイメージRAMをＡＱＭ１６０２Ｙ－ＮＬＷ－ＢＢＷに送信
	// DDRAM ADDRESS
	//     →                     使用領域                 ← →未使用
	// x    1  2  3  4  5  6  7  8  9 10 11 12 13 13 14 15 16 17 … 40
	// y -------------------------------------------------------------
	// 1 | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11 … 27
	// 2 | 40 41 42 43 44 45 46 47 48 49 4A 4B 4C 4D 4E 4F 50 51 … 67
	// 1行目を送信
	pos = SET_DDRAM_ADDRESS_COMMAND | 0;
	lcd_send_data(CONTROL_TYPE, &pos, 1);
	lcd_send_data(DATA_CGRAM_TYPE, this->lcd_img,  LCD_DISPLAY_WIDTH); 
	// 2行目を送信
	pos = SET_DDRAM_ADDRESS_COMMAND | 0x40;
	lcd_send_data(CONTROL_TYPE, &pos, 1);
	lcd_send_data(DATA_CGRAM_TYPE, this->lcd_img+16,  LCD_DISPLAY_WIDTH);
	
	return 0;
}
