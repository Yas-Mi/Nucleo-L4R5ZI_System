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
#define MSP2807_SPI_BAUDRATE	(1*1000*1000)		// 通信速度 1MHz
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
#define MSP2807_TOUCH_CMD_ADC_STP_PENDWN_ENABLE		(0x90)
#define MSP2807_TOUCH_CMD_Y_ENABLE_PENDWN_DISABLE	(0x91)
#define MSP2807_TOUCH_CMD_X_ENABLE_PENDWN_DISABLE	(0xD1)

// 状態
#define ST_UNINITIALIZED	(0)		// 未初期化
#define ST_INITIALIZED		(1)		// 初期化
#define ST_IDLE				(2)		// アイドル
#define ST_RUNNING			(3)		// 実行中
#define ST_MAX				(4)

// ペン状態


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
	{GPIOC, GPIO_PIN_9},	// タッチスクリーン PEN
};
#define gpio_reset(val)	HAL_GPIO_WritePin(gpio_cfg[MSP2807_GPIO_TYPE_RESET].pin_group, gpio_cfg[MSP2807_GPIO_TYPE_RESET].pin, val);\
						__DSB();
#define cmd_mode()	HAL_GPIO_WritePin(gpio_cfg[MSP2807_GPIO_TYPE_DCX].pin_group, gpio_cfg[MSP2807_GPIO_TYPE_DCX].pin, GPIO_PIN_RESET); \
					__DSB();
#define data_mode()	HAL_GPIO_WritePin(gpio_cfg[MSP2807_GPIO_TYPE_DCX].pin_group, gpio_cfg[MSP2807_GPIO_TYPE_DCX].pin, GPIO_PIN_SET); \
					__DSB();

// オープンパラメータ(LCD)
static const SPI_OPEN lcd_spi_open_par = {
	MSP2807_SPI_BAUDRATE,		// 通信速度
	SPI_CPOL_POSITIVE,			// Clock Polarity
	SPI_CPHA_FIRST_EDGE,    	// Clock Phase
	SPI_FRAME_FMT_MSB_FIRST,	// フレームフォーマット
	SPI_DATA_SIZE_8BIT,			// データサイズ
};

// オープンパラメータ(タッチスクリーン)
static const SPI_OPEN touch_spi_open_par = {
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
	
	// タッチスクリーンの割り込みを有効
	
	
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
	this->state = ST_INITIALIZED;
	
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
	ret = spi_send_recv(MSP2807_USW_SPI_CH_LCD, (uint8_t*)disp_data, NULL, (MSP2807_DISPLAY_HEIGHT*MSP2807_DISPLAY_WIDTH*2));
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
	
	// X軸有効X軸電圧供給，ペン割り込み無効
	snd_data[0] = MSP2807_TOUCH_CMD_Y_ENABLE_PENDWN_DISABLE;
	snd_data[1] = 0x00;	// ダミー
	snd_data[0] = MSP2807_TOUCH_CMD_Y_ENABLE_PENDWN_DISABLE;
	ret = spi_send_recv(MSP2807_USE_SPI_CH_TOUCH, snd_data, rcv_data, 3);
	if (ret != E_OK) {
		goto MSP2807_GET_TOUCH_POS_EXIT;
	}
	
	// x座標の設定
	*x = (rcv_data[1] << 8) | rcv_data[0];
	
	// Y軸有効Y軸電圧供給，ペン割り込み無効
	snd_data[0] = MSP2807_TOUCH_CMD_X_ENABLE_PENDWN_DISABLE;
	snd_data[1] = 0x00;	// ダミー
	snd_data[0] = MSP2807_TOUCH_CMD_X_ENABLE_PENDWN_DISABLE;
	ret = spi_send_recv(MSP2807_USE_SPI_CH_TOUCH, snd_data, rcv_data, 3);
	if (ret != E_OK) {
		goto MSP2807_GET_TOUCH_POS_EXIT;
	}
	
	// y座標の設定
	*y = (rcv_data[1] << 8) | rcv_data[0];
	
MSP2807_GET_TOUCH_POS_EXIT:
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
	cfg = gpio_cfg[MSP2807_GPIO_TYPE_PEN];
	
	// リード
	state = HAL_GPIO_ReadPin(cfg->pin_group, cfg->pin);
	
	// lowだったら押された！
	if (state == GPIO_PIN_RESET) { 
		ret_state = MSP2807_PEN_STATE_PUSHED;
	}
	
	return ret_state;
}
// ↓ タッチスクリーン
/*
 * touch_screen.c
 *
 *  Created on: 2022/10/29
 *      Author: User
 */
/*
	設計思想
	・タッチパネルのデバイスが変わっても、アプリ側に修正は入らないようにする
	
*/
#include "defines.h"
#include "kozos.h"
#include "bt_dev.h"
#include "console.h"
#include "ctl_main.h"
#include "intr.h"
#include "MSP2807.h"
#include <string.h>

// 状態
#define ST_UNINITIALIZED	(0)
#define ST_INITIALIZED		(1)
#define ST_CONNECTED		(2)
#define ST_DISCONNECTED		(3)
#define ST_UNDEIFNED		(4)
#define ST_MAX				(5)

// イベント
#define EVENT_CYC		(0)
#define EVENT_DISCONNECT	(1)
#define EVENT_SEND			(2)
#define EVENT_CHECK_STS		(3)
#define EVENT_MAX			(4)

// マクロ
#define CHECK_TOUCH_STATE_PERIOD	(10)	// タッチ状態のチェック周期
#define CHECK_TOUCH_STATE_CNT		(3)		// タッチ状態のチェック回数
#define TS_TOUCHED					((1 << CHECK_TOUCH_STATE_CNT) - 1)
											// タッチ確定

// 制御用ブロック
typedef struct {
	uint32_t		state;					// 状態
	uint8_t			touch_state_bmp;		// タッチ状態のビットマップ
	uint8_t			check_touch_state_cnt;	// タッチ状態のチェック回数
	TS_CALLBACK		callback_fp;			// コールバック
} TOUCH_SCREEN_CTL;
static TOUCH_SCREEN_CTL ts_ctl;

// メッセージ
typedef struct {
	uint32_t     msg_type;
	BT_SEND_INFO msg_data;
}BT_MSG;

// タッチの状態を取得
static void ts_msg_check_touch_state(void)
{
	TOUCH_SCREEN_CTL *this = &ts_ctl;
	MSP2807_PEN_STATE state;
	int32_t ret;
	int16_t x, y;
	
	// タッチ状態を取得
	this->touch_state_bmp |= (msp2807_get_touch_state() << this->check_touch_state_cnt++);
	
	// チェック回数に達していない場合は終了
	if (this->check_touch_state_cnt < CHECK_TOUCH_STATE_CNT) {
		return;
	}
	
	// チェック回数をクリア
	this->check_touch_state_cnt = 0;
	
	// タッチされた？
	if (this->touch_state_bmp == TS_TOUCHED) {
		console_str_send("touched!\n");
		// 座標を取得
		x = 0;
		y = 0;
		ret = msp2807_get_touch_pos(&x, &y);
		if (ret != E_OK) {
			console_str_send("msp2807_get_touch_pos error\n");
		}
		
	// タッチされていない	
	} else {
		;
	}
	
	// タッチ状態をクリア
	this->touch_state_bmp = 0;
}

// 状態遷移テーブル
static const FSM fsm[ST_MAX][EVENT_MAX] = {
	// EVENT_CONNECT						EVENT_DISCONNECT								EVENT_SEND								EVENT_CHECK_STS
	{{NULL,					ST_UNDEIFNED},	{NULL,						ST_UNDEIFNED},		{NULL,				ST_UNDEIFNED},		{NULL,					ST_UNDEIFNED}},		// ST_UNINITIALIZED
	{{bt_dev_msg_connected,	ST_CONNECTED},	{bt_dev_msg_unconnected,	ST_DISCONNECTED},	{NULL,				ST_UNDEIFNED},		{bt_dev_msg_check_sts,	ST_UNDEIFNED}},		// ST_INITIALIZED
	{{NULL,					ST_UNDEIFNED},	{bt_dev_msg_unconnected,	ST_DISCONNECTED},	{bt_dev_msg_send,	ST_UNDEIFNED},		{bt_dev_msg_check_sts,	ST_UNDEIFNED}},		// ST_CONNECTED
	{{bt_dev_msg_connected,	ST_CONNECTED},	{NULL,						ST_UNDEIFNED},		{bt_dev_msg_send,	ST_UNDEIFNED},		{bt_dev_msg_check_sts,	ST_UNDEIFNED}},		// ST_DISCONNECTED
	{{NULL,					ST_UNDEIFNED},	{NULL,						ST_UNDEIFNED},		{NULL,				ST_UNDEIFNED},		{NULL,					ST_UNDEIFNED}},		// ST_UNDEIFNED
};

// BlueToothデバイス状態管理タスク
static int bt_dev_sts_main(int argc, char *argv[])
{
	BT_CTL *this = &bt_ctl;
	BT_MSG *msg;
	BT_MSG tmp_msg;
	int32_t size;
	int32_t addr;
	volatile uint32_t a = 0;
	
	while(1){
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);
		// いったんメッセージをローカル変数にコピー
		memcpy(&tmp_msg, msg, sizeof(BT_MSG));
		// メッセージを解放
		kz_kmfree(msg);
		// 処理を実行
		if (fsm[this->state][tmp_msg.msg_type].func != NULL) {
			fsm[this->state][tmp_msg.msg_type].func(&(tmp_msg.msg_data));
		}
		// 状態遷移
		if (fsm[this->state][tmp_msg.msg_type].nxt_state != ST_UNDEIFNED) {
			this->state = fsm[this->state][tmp_msg.msg_type].nxt_state;
		}
	}
	
	return 0;
}

// 初期化関数
void bt_dev_init(void)
{
	BT_CTL *this = &bt_ctl;
	int32_t ret;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(BT_CTL));
	
	// メッセージIDの設定
	this->msg_id = MSGBOX_ID_TOUCH_SCREEN;
	
	// 周期メッセージの作成
	set_cyclic_message(bt_dev_check_sts, CHECK_TOUCH_STATE_PERIOD);
	
	// タスクの生成
	this->tsk_sts_id = kz_run(touch_screen_main, "touch_screen_main",  TOUCH_SCREEN_PRI, TOUCH_SCREEN_STACK, 0, NULL);
	
	// 状態の更新
	this->state = ST_INITIALIZED;
	
	return 0;
}

// コールバック登録関数
int32_t bt_dev_reg_callback(BT_RCV_CALLBACK callback, void *callback_vp)
{
	BT_CTL *this = &bt_ctl;
	
	// コールバックがすでに登録されている？
	if (this->callback != NULL) {
		return -1;
	}
	
	// コールバック登録
	this->callback = callback;
	this->callback_vp = callback_vp;
	
	return 0;
}

// BlueTooth送信通知関数
int32_t bt_dev_send(BT_SEND_TYPE type, uint8_t *data, uint8_t size)
{
	BT_CTL *this = &bt_ctl;
	BT_MSG *msg;
	uint8_t i;
	
	// NULLの場合はエラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// 未初期化の場合はエラーを返して終了
	if (this->state == ST_UNINITIALIZED) {
		return -1;
	}
	
	// 送信中の場合はエラーを返して終了
	if (this->snd_buf.size != 0) {
		return -1;
	}
	
	msg = kz_kmalloc(sizeof(BT_MSG));
	msg->msg_type = EVENT_SEND;
	
	// 割込み禁止
	INTR_DISABLE;
	
	// データをコピー
	for (i = 0; i < size; i++) {
		this->snd_buf.data[i] = *(data++);
	}
	// サイズの設定
	this->snd_buf.size = size;
	
	// 割込み禁止解除
	INTR_ENABLE;
	
	msg->msg_data.type = type;
	msg->msg_data.data = this->snd_buf.data;
	msg->msg_data.size = this->snd_buf.size;
	
	kz_send(this->msg_id, sizeof(BT_MSG), msg);
}

// 状態チェック関数
int32_t bt_dev_check_sts(void)
{
	BT_CTL *this = &bt_ctl;
	BT_MSG *msg;
	BT_MSG *tmp_msg;
	uint32_t i;
	
	msg = kz_kmalloc(sizeof(BT_MSG));
	msg->msg_type = EVENT_CHECK_STS;
	msg->msg_data.data = NULL;
	kz_send(this->msg_id, sizeof(BT_MSG), msg);
}
	
// ボーレート設定関数
int32_t bt_dev_set_baudrate(BT_BAUDRATE_TYPE baudrate)
{
	BT_BAUDRATE_TYPE tmp_baudrate = baudrate;
	
	// ボーレートが範囲外の場合はエラーを返して終了
	if ((tmp_baudrate >= BT_BAUDRATE_TYPE_MAX)||(tmp_baudrate <= BT_BAUDRATE_TYPE_NOT_USED)) {
		return -1;
	}
	
	// 送信
	bt_dev_send(SEND_TYPE_CMD_BAUD, &tmp_baudrate, 1);
	
	return 0;
}

// コマンド設定関数
void bt_dev_set_cmd(void)
{
	COMMAND_INFO cmd;
	
	// コマンドの設定
	cmd.input = "bt_dev connect_check";
	cmd.func = bt_dev_cmd_connect_check;
	console_set_command(&cmd);
	cmd.input = "bt_dev version";
	cmd.func = bt_dev_cmd_version;
	console_set_command(&cmd);
	cmd.input = "bt_dev name";
	cmd.func = bt_dev_cmd_name;
	console_set_command(&cmd);
	cmd.input = "bt_dev baudrate 115200";
	cmd.func = bt_dev_cmd_baudrate_115200;
	console_set_command(&cmd);
	cmd.input = "bt_dev baudrate 9600";
	cmd.func = bt_dev_cmd_baudrate_9600;
	console_set_command(&cmd);
	cmd.input = "bt_dev change baudrate";
	cmd.func = bt_dev_cmd_change_baudrate;
	console_set_command(&cmd);
}
