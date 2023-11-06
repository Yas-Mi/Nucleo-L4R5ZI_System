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
//#include "lib.h"
#include "stm32l4xx_hal_gpio.h"
#include "pcm3060.h"
#include "sai.h"
#include "util.h"
#include "console.h"

// 状態
#define PCM3060_ST_NOT_INTIALIZED	(0U) // 未初期状態
#define PCM3060_ST_INITIALIZED		(1U) // 初期化済み
#define PCM3060_ST_OPEND			(2U) // オープン済み
#define PCM3060_ST_RUN				(3U) // 動作中

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
#define PCM3060_WAIT		(0xFF)	// レジスタの定義ではなく、wait用

// 設定値
#define PCM3060_REGISTER68_MUT		(0x03)

// リトライ
#define PCM3060_RETRY_CNT	(5)		// 設定のリトライカウント



// DMAを使用するかしないか
#define DMA_USE	(1)

// 制御用ブロック
typedef struct {
	uint8_t status;				// 状態
	PCM3060_CALLBACK callback;	// コールバック
	void*	callback_vp;		// コールバックパラメータ
}PCM3060_CTL;
static PCM3060_CTL pcm3060_ctl;

// PCM3060の設定情報
typedef struct {
	uint8_t reg;	// レジスタのアドレス
	uint8_t data;	// データ
	char	*reg_name;	// レジスタの名前 (デバッグ用)
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
	SAI_PACK_MSB_FIRST,		// MSB First
	0,						// データ幅(後から設定)
	SAI_BICK_TYPE_64FS,		// BICK = 64fs
	0,						// サンプリング周波数(後から設定)
	TRUE,					// マスタークロックを使用
};

// PCM3060の設定パラメータ
static const PCM3060_SETTING pcm3060_setting[] = {
//	{PCM3060_REGISTER64, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER65, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER66, 0x00}, // デフォルト設定でいいでしょう
	{PCM3060_REGISTER64, 0x30, "reg64"},	// MRST = 0, SRST = 0 (他はデフォルト値)
	{PCM3060_WAIT,       0x64, "wait" },	// 100ms wait
	{PCM3060_REGISTER64, 0xE1, "reg64"},	// DAPSV = 0, S/E = 1 (他はデフォルト値)
	{PCM3060_WAIT,       0x64, "wait" },	// 100ms wait
	{PCM3060_REGISTER67, 0x03, "reg67"},	// FMT21, FMT20 = 1, 1 (他はデフォルト値)
	{PCM3060_WAIT,       0x64, "wait" },	// 100ms wait
//	{PCM3060_REGISTER68, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER69, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER70, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER71, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER72, 0x00}, // デフォルト設定でいいでしょう
//	{PCM3060_REGISTER73, 0x00}, // デフォルト設定でいいでしょう

};

// I2Cのオープンパラメータ
static const I2C_OPEN i2c_open_par = {
	I2C_BPS_100K,
};

// 送受信完了フラグ
volatile static uint8_t send_cmp_flag = FALSE;
volatile static uint8_t read_cmp_flag = FALSE;
volatile static uint8_t err_flag = FALSE;

// SAIからのコールバック
static void sai_callback(SAI_CH ch, void *vp)
{
	PCM3060_CTL *this = (PCM3060_CTL*)vp;
	
	// ちゃんと期待したチャネル？
	if (ch != PCM3060_USE_SAI_CH) {
		return;
	}
	
	// 状態を更新
	this->status = PCM3060_ST_OPEND;
	
	// コールバック
	if (this->callback) {
		this->callback(this->callback_vp);
	}
}

// I2Cの送信完了コールバック (*)未使用
static void i2c_wrapper_snd_callback(I2C_CH ch, int32_t ercd, void *vp)
{
	PCM3060_CTL *this = &pcm3060_ctl;
	
	// 送信完了
	send_cmp_flag = TRUE;
	// エラーフラグ
	if (ercd != 0) {
		err_flag = TRUE;
	} else {
		err_flag = FALSE;
	}
}

// I2Cの受信完了コールバック (*)未使用
static void i2c_wrapper_rcv_callback(I2C_CH ch, int32_t ercd, uint8_t *data, uint32_t size, void *vp)
{
	PCM3060_CTL *this = &pcm3060_ctl;
	
	// 受信完了
	read_cmp_flag = TRUE;
	// エラーフラグ
	if (ercd != 0) {
		err_flag = TRUE;
	} else {
		err_flag = FALSE;
	}
}

// 送信関数 (*)ブロッキング
static int32_t pcm3060_send_data(uint8_t *data, uint8_t size, uint8_t retry_cnt)
{
	int32_t ret;
	uint8_t r_cnt = 0;
	
	// 設定
	do {
		// レジスタライト
		i2c_wrapper_send(PCM3060_USE_I2C_CH, PCM3060_SLAVE_ADDRESS, data, size);
		// 送信完了するまで待つ (初期タスクで処理しているため寝られない...)
		while (send_cmp_flag == FALSE) {};
		// リトライカウンタを増やす
		r_cnt++;
		// フラグをクリア
		send_cmp_flag = FALSE;
	} while ((err_flag) && (r_cnt < retry_cnt));
	
	// リトライ回数やっても駄目だった
	if (r_cnt >= PCM3060_RETRY_CNT) {
		console_str_send((uint8_t*)"The number of retries was sent, but communication was not possible.\n");
		return -1;
	}
	
	return 0;
}

// 受信関数 (*)ブロッキング デバッグ用なので1byteリードのみ対応
static int32_t pcm3060_read_data(uint8_t *data, uint8_t retry_cnt)
{
	int32_t ret;
	uint8_t r_cnt = 0;
	
	// レジスタリード
	do {
		// レジスタリード (ちゃんと書けたか確認) (初期タスクで処理しているため寝られない...)
		i2c_wrapper_read(PCM3060_USE_I2C_CH, PCM3060_SLAVE_ADDRESS, data, 1);
		// ポーリングで受信できるのを待つ (初期タスクで処理しているため寝られない...)
		while (read_cmp_flag == FALSE) {};
		// リトライカウンタを増やす
		r_cnt++;
		// フラグをクリア
		read_cmp_flag = FALSE;
	} while ((err_flag) && (r_cnt < retry_cnt));
	
	// リトライ回数やっても駄目だった
	if (r_cnt >= PCM3060_RETRY_CNT) {
		console_str_send((uint8_t*)"The number of retries was sent, but communication was not possible.\n");
		return -1;
	}
	
	return 0;
}

// デバッグ用 レジスタ設定ができたかどうかを確認
static void reg_check(uint8_t idx)
{
	uint8_t retry_cnt = 0;
	uint8_t data;
	
	console_str_send((uint8_t*)pcm3060_setting[idx].reg_name);
	console_str_send((uint8_t*)"\n");
	console_str_send((uint8_t*)" send : ");
	console_val_send(pcm3060_setting[idx].data);
	console_str_send((uint8_t*)"\n");
	
	// 値を読みたいレジスタを設定
	pcm3060_send_data(&pcm3060_setting[idx], 1, 5);
	
	// 値をリード
	pcm3060_read_data(&data, 5);
	
	// 表示
	console_str_send((uint8_t*)" read : ");
	console_val_send(data);
	console_str_send((uint8_t*)"\n");
}

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

// PCM3060オープン関数
int32_t pcm3060_open(uint32_t fs, uint8_t data_width, PCM3060_CALLBACK callback, void* callback_vp)
{
	PCM3060_CTL *this = &pcm3060_ctl;
	SAI_OPEN open_par;
	uint8_t i;

	int32_t ret;
	uint32_t cnt = 0;
	uint8_t retry_cnt = 0;
	
	// 未初期化の場合はエラーを返して終了
	if (this->status != PCM3060_ST_INITIALIZED) {
		return -1;
	}
	
	// I2Cをオープンする
	ret = i2c_wrapper_open(PCM3060_USE_I2C_CH, &i2c_open_par, i2c_wrapper_snd_callback, i2c_wrapper_rcv_callback, this);
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
#if DMA_USE // DMAを使用する
	// SAIをオープンする
	ret = sai_open_dma(PCM3060_USE_SAI_CH, &open_par, sai_callback, this);
	if (ret != 0) {
		return -1;
	}
#else
	// SAIをオープンする
	ret = sai_open(PCM3060_USE_SAI_CH, &open_par, sai_callback, this);
	if (ret != 0) {
		return -1;
	}
#endif
	// コールバック登録
	this->callback = callback;
	this->callback_vp = callback_vp;
	
	// RST解除
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
	
	// 100wait
	busy_wait(100);
	
	// PCM3060の設定
	for (i = 0; i < sizeof(pcm3060_setting)/sizeof(pcm3060_setting[0]); i++) {
		// wait
		if (pcm3060_setting[i].reg == PCM3060_WAIT) {
			busy_wait(pcm3060_setting[i].data);
			continue;
		}
		// レジスタ設定
		pcm3060_send_data(&pcm3060_setting[i], 2, 5);
#if 1
		// デバッグ用
		reg_check(i);
#endif
	}
	
	// 状態を更新
 	this->status = PCM3060_ST_OPEND;
	
	return 0;
}

// PCM3060再生関数
// dataはアプリ側で32bitを用意する
// sai側で設定されたbitにキャストして送信される
int32_t pcm3060_play(int32_t *data, uint32_t size)
{
	PCM3060_CTL *this = &pcm3060_ctl;
	int32_t ret;
	
	// オープンされていない場合はエラーを返して終了
	if (this->status != PCM3060_ST_OPEND) {
		return -1;
	}
	
#if DMA_USE // DMAを使用する
	// 送信
	ret = sai_send_dma(PCM3060_USE_SAI_CH, data, size);
	if (ret != 0) {
		return -1;
	}
#else
	// 送信
	ret = sai_send(PCM3060_USE_SAI_CH, data, size);
	if (ret != 0) {
		return -1;
	}
#endif
	// 状態を更新
	this->status = PCM3060_ST_RUN;
	
	return ret;
}

// PCM3060停止関数
int32_t pcm3060_stop(void)
{
	PCM3060_CTL *this = &pcm3060_ctl;
	int32_t ret;
	
	// 動作中でない場合はエラーを返して終了
	if (this->status != PCM3060_ST_RUN) {
		return -1;
	}
	
#if DMA_USE // DMAを使用する
	// 送信
	ret = sai_stop_dma(PCM3060_USE_SAI_CH);
	if (ret != 0) {
		return -1;
	}
#else
	// 今のところDMAを使用しないことは考えていない
#endif
	// 状態を更新
	this->status != PCM3060_ST_OPEND;
	
	return ret;
}

// PCM3060 ミュート設定関数
int32_t pcm3060_mute(uint8_t enable)
{
	PCM3060_CTL *this = &pcm3060_ctl;
	int32_t ret;
	uint8_t snd_data[2];
	uint8_t rcv_data;
	
	// オープンされていない場合はエラーを返して終了
	if (this->status != PCM3060_ST_OPEND) {
		return -1;
	}
	
	// 現在設定されている値を取得
	snd_data[0] = PCM3060_REGISTER68;
	pcm3060_send_data(snd_data, 1, 5);
	pcm3060_read_data(&rcv_data, 5);
	
	console_str_send((uint8_t*)"cur : ");
	console_val_send(rcv_data);
	console_str_send((uint8_t*)"\n");
	
	// ミュート設定
	if (enable) {
		snd_data[1] = rcv_data | PCM3060_REGISTER68_MUT;
	} else {
		snd_data[1] = rcv_data & ~PCM3060_REGISTER68_MUT;
	}
	pcm3060_send_data(snd_data, 2, 5);
	
	// 現在設定されている値を取得
	snd_data[0] = PCM3060_REGISTER68;
	pcm3060_send_data(snd_data, 1, 5);
	pcm3060_read_data(&rcv_data, 5);
	
	console_str_send((uint8_t*)"cur : ");
	console_val_send(rcv_data);
	console_str_send((uint8_t*)"\n");
}

// コマンド
static void pcm3060_cmd_mute_enable(void)
{
	pcm3060_mute(TRUE);
}

static void pcm3060_cmd_mute_disable(void)
{
	pcm3060_mute(FALSE);
}

// コマンド設定関数
void pcm3060_set_cmd(void)
{
	COMMAND_INFO cmd;
	
	// コマンドの設定
	cmd.input = "pcm3060 mute enable";
	cmd.func = pcm3060_cmd_mute_enable;
	console_set_command(&cmd);
	cmd.input = "pcm3060 mute disable";
	cmd.func = pcm3060_cmd_mute_disable;
	console_set_command(&cmd);
}
