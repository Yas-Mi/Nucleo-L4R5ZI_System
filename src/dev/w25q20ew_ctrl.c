/*
 * w25q20ew.c
 *
 *  Created on: 2023/8/29
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "intr.h"
#include "octspi.h"
#include <string.h>

#include "w25q20ew_cmd.h"
#include "w25q20ew_ctrl.h"
#include "w25q20ew_common.h"

// マクロ
#define W25Q20EW_POLLING_PERIOD		(1)				// 値をチェックするポーリング周期
#define W25Q20EW_TIMEOUT			(10)			// タイムアウト時間

// デバイス情報
#define W25Q20EW_START_ADDRESS		(0x00000000)	// 先頭アドレス
#define W25Q20EW_END_ADDRESS		(0x0003FFFF)	// 終了アドレス
#define W25Q20EW_MEM_SIZE			(256*1024)		// デバイスのメモリサイズ
#define W25Q20EW_1PAGE_SIZE			(256)			// 1ページのサイズ
#define W25Q20EW_SECTOR_SIZE		(4*1024)		// セクターサイズ
#define W25Q20EW_BLOCK_SIZE			(16*W25Q20EW_SECTOR_SIZE)	
													// ブロックサイズ
// フラッシュ ビット定義
// ステータスレジスタ
#define FLASH_STATUS_1_BUSY		(1 << 0)
#define FLASH_STATUS_1_WEL		(1 << 1)
#define FLASH_STATUS_1_BP0		(1 << 2)
#define FLASH_STATUS_1_BP1		(1 << 3)
#define FLASH_STATUS_1_BP2		(1 << 4)
#define FLASH_STATUS_1_TB		(1 << 5)
#define FLASH_STATUS_1_SEC		(1 << 6)
#define FLASH_STATUS_1_SRP		(1 << 7)
#define FLASH_STATUS_2_SRL		(1 << 0)
#define FLASH_STATUS_2_QE		(1 << 1)
#define FLASH_STATUS_2_RESERVED	(1 << 2)
#define FLASH_STATUS_2_LB1		(1 << 3)
#define FLASH_STATUS_2_LB2		(1 << 4)
#define FLASH_STATUS_2_LB3		(1 << 5)
#define FLASH_STATUS_2_CMP		(1 << 6)
#define FLASH_STATUS_2_SUS		(1 << 7)

// 状態
#define ST_UNINITIALIZED	(0)		// 未初期化
#define ST_INITIALIZED		(1)		// 初期化
#define ST_IDLE				(2)		// アイドル
#define ST_RUNNING			(3)		// 実行中
#define ST_MAX				(4)

// 制御用ブロック
typedef struct {
	uint32_t			state;					// 状態
	W25Q20EW_CALLBACK	callback;				// コールバック
	void				*callback_vp;			// コールバックパラメータ
} W25Q20EW_CTL;
static W25Q20EW_CTL w25q20ew_ctl;

// OCTOSPIオープンパラメータ
static const OCTOSPI_OPEN open_par = {
	//50*1000*1000,					// 50MHz
	1*1000,						// 1kHz
	OCTOSPI_OPE_MODE_INDIRECT,		// インダイレクトモード
	OCTOSPI_IF_QUAD,				// クアッド通信
	OCTOSPI_MEM_STANDARD,			// スタンダード
	W25Q20EW_MEM_SIZE,				// 256Kbyte
	FALSE,							// デュアルモードでない
	FALSE,							// DQSは使用しない
	FALSE,							// NCLKは使用しない
	OCTOSPI_CLKMODE_LOW,			// CSがHighのときはCLKはLow
	FALSE,							// SIOOは使用しない
};

// ポーリングでリードレジスタの値をチェック
static int32_t w25q20ew_polling_read_status(uint32_t bit, uint32_t flag, uint32_t timeout)
{
	uint8_t expected_status;
	uint8_t status = 0;
	uint32_t cnt;
	int32_t ret = E_TMOUT;
	int32_t cmd_ret;
	
	// ポーリング用にカウンタを計算
	cnt = timeout/W25Q20EW_POLLING_PERIOD;
	if (cnt == 1) {
		cnt = 1;
	}
	
	// 期待するステータスを設定
	expected_status = (flag << (bit - 1));
	
	// ポーリング
	while(cnt-- > 0) {
		// ステータス受信
		cmd_ret = w25q20ew_cmd_read_status(&status);
		// 期待した値であれば終了
		if ((cmd_ret == E_OK) && ((status & bit) == expected_status)) {
			ret = E_OK;
			break;
		}
		// ディレイ
		kz_tsleep(W25Q20EW_POLLING_PERIOD);
	}
	
	return ret;
}

// 初期化関数
int32_t w25q20ew_init(void)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(W25Q20EW_CTL));
	
	// 状態の更新
	this->state = ST_INITIALIZED;
	
	return E_OK;
}

int32_t w25q20ew_open(void)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	int32_t ret;
	
	// octospiオープン
	ret = octospi_open(W25Q20EW_OCTOSPI_CH, &open_par);
	if (ret != E_OK) {
		return ret;
	}
	
	// 状態の更新
	this->state = ST_IDLE;
	
	return E_OK;
}

// 書き込み関数
int32_t w25q20ew_write(uint32_t addr, uint8_t *data, uint8_t size)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	
	// NULLの場合はエラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// アイドルでなければ終了
	if (this->state != ST_IDLE) {
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	
	// ★あとで実装
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return 0;
}

// イレース関数
int32_t w25q20ew_erase(uint32_t addr)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	OCTOSPI_COM_CFG cfg;
	int32_t ret;
	
	// アイドル出なければ終了
	if (this->state != ST_IDLE) {
		return E_PAR;
	}
	
	// アドレスチェック
	if (addr > W25Q20EW_END_ADDRESS) {
		return E_PAR;
	}
	
	// 状態を更新
	this->state = ST_RUNNING;
	
	// ライトイネーブル
	ret = w25q20ew_cmd_write_enable();
	if (ret != E_OK) {
		console_str_send("write enable error\n");
		goto ERASE_EXIT;
	}
	
	// ライトイネーブル有効待ち
	ret = w25q20ew_polling_read_status(FLASH_STATUS_1_WEL, 1, W25Q20EW_TIMEOUT);
	if (ret != E_OK) {
		console_str_send("write enable wait 1 error\n");
		goto ERASE_EXIT;
	}
	
	// イレース
	ret = w25q20ew_cmd_erase(addr);
	if (ret != E_OK) {
		console_str_send("erace error\n");
		goto ERASE_EXIT;
	}
	
	// イレース完了待ち
	ret = w25q20ew_polling_read_status(FLASH_STATUS_1_BUSY, 0, W25Q20EW_TIMEOUT);
	if (ret != E_OK) {
		console_str_send("write enable wait 1 error\n");
		goto ERASE_EXIT;
	}
	
	// ライトディセーブル
	ret = w25q20ew_cmd_write_disable();
	if (ret != E_OK) {
		console_str_send("write disable error\n");
		goto ERASE_EXIT;
	}
	
	// ライトディセーブル有効待ち
	ret = w25q20ew_polling_read_status(FLASH_STATUS_1_WEL, 0, W25Q20EW_TIMEOUT);
	if (ret != E_OK) {
		console_str_send("write enable wait 1 error\n");
		goto ERASE_EXIT;
	}
	
ERASE_EXIT:
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return ret;
}

// 読み込み関数
int32_t w25q20ew_read(uint32_t addr, uint8_t *data, uint8_t size)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	OCTOSPI_COM_CFG cfg;
	int32_t ret;
	
	// NULLの場合はエラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// アイドル出なければ終了
	if (this->state != ST_IDLE) {
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	
#ifdef READ_ACCESS_SINGLE	// // シングルアクセス
	// シングルアクセス
	ret = w25q20ew_cmd_read_single(addr, data, size);
	
#else
	// アクセス終了
	goto ACCESS_END;
#endif
	
ACCESS_END:
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return ret;
}

// デバイスID取得
// 引数には2byteの配列を渡す
int32_t w25q20ew_get_devise_id(uint8_t *id)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	const OCTOSPI_COM_CFG *cmd_cfg;
	int32_t ret;
	
	// アイドルでなければ終了
	if (this->state != ST_IDLE) {
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	
	// 受信
	ret = w25q20ew_cmd_get_devise_id(id);
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return ret;
}

// SFDP取得
// 引数には256byteの配列を渡す
int32_t w25q20ew_get_sfdp(uint8_t *sfdp)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	int32_t ret;
	
	// アイドルでなければ終了
	if (this->state != ST_IDLE) {
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	
	// 受信
	ret = w25q20ew_cmd_get_sfdp(sfdp);
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return ret;
}

// コマンド
static void w25q20ew_ctrl_cmd_get_device_id(void)
{
	uint8_t id[2];
	uint32_t ret;
	uint32_t i;
	
	// 初期化
	memset(id, 0, sizeof(id));
	
	// ID取得
	ret = w25q20ew_get_devise_id(id);
	if (ret != 0) {
		console_str_send("w25q20ew_get_devise_id error\n");
	}
	
	// 表示
	console_str_send("id\n");
	for (i = 0; i < 2; i++) {
		console_val_send(id[i]);
		console_str_send("\n");
	}
}

// コマンド
static void w25q20ew_ctrl_cmd_get_sfdp(void)
{
	uint8_t sfdp[256];
	uint32_t ret;
	uint32_t i, j, k;
	
	// 初期化
	memset(sfdp, 0, sizeof(sfdp));
	
	// SFDP取得
	ret = w25q20ew_get_sfdp(sfdp);
	if (ret != 0) {
		console_str_send("w25q20ew_cmd_get_sfdp error\n");
	}
	
	// 表示
	console_str_send("==================== SFDP ============================\n");
	console_str_send("     |  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
	console_str_send("------------------------------------------------------\n");
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 16; j++) {
			k = i*16+j;
			if (j == 0) {
				// アドレス表示
				console_str_send("0x");
				console_val_send_hex(k, 2);
				console_str_send(" |");
			}
			console_str_send(" ");
			console_val_send_hex(sfdp[k], 2);
		}
		console_str_send("\n");
	}
}

// コマンド
static void w25q20ew_ctrl_cmd_write_enable(void)
{
	const OCTOSPI_COM_CFG *cmd_cfg;
	uint8_t status = 0;
	int32_t ret;
	
	// ライトイネーブル
	console_str_send("write enable\n");
	ret = w25q20ew_cmd_write_enable();
	if (ret != E_OK) {
		console_str_send("write enable error\n");
		goto WRITE_ENABLE_EXIT;
	}
	
WRITE_ENABLE_EXIT:
	return;
}

static void w25q20ew_ctrl_cmd_write_disable(void)
{
	int32_t ret;
	
	// ライトイネーブル
	console_str_send("write disable\n");
	ret = w25q20ew_cmd_write_disable();
	if (ret != E_OK) {
		console_str_send("write disable error\n");
		goto WRITE_DISABLE_EXIT;
	}
	
WRITE_DISABLE_EXIT:
	return;
}

static void w25q20ew_ctrl_cmd_read_status(void)
{
	const OCTOSPI_COM_CFG *cmd_cfg;
	uint8_t status = 0;
	int32_t ret;
	
	// ステータス取得
	ret = w25q20ew_cmd_read_status(&status);
	if (ret != E_OK) {
		console_str_send("status read error\n");
		goto READ_STATUS_EXIT;
	}
	console_str_send("status:");
	console_val_send_hex(status, 2);
	console_str_send("\n");
	
READ_STATUS_EXIT:
	return;
}

static void w25q20ew_ctrl_cmd_erace(void)
{
	int32_t ret;
	
	// イレース
	ret = w25q20ew_erase(0);
	if (ret != E_OK) {
		console_str_send("erace error\n");
		goto ERACE_EXIT;
	}
	
ERACE_EXIT:
	return;
}

#define W25Q20EW_BUFF_SIZE	(4*1024)
static uint8_t w25q20ew_buff[W25Q20EW_BUFF_SIZE];
static void w25q20ew_ctrl_cmd_read(void)
{
	int32_t ret;
	uint32_t i, j, k;
	
	// 初期化
	memset(w25q20ew_buff, 0, sizeof(W25Q20EW_BUFF_SIZE));
	
	// リード
	ret = w25q20ew_read(0, w25q20ew_buff, W25Q20EW_BUFF_SIZE);
	if (ret != E_OK) {
		console_str_send("read error\n");
		goto ERACE_EXIT;
	}
	
	// 表示
	console_str_send("==================== BUFF ============================\n");
	console_str_send("     |  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
	console_str_send("------------------------------------------------------\n");
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 16; j++) {
			k = i*16+j;
			if (j == 0) {
				// アドレス表示
				//console_str_send("0x");
				//console_val_send_hex(k, 2);
				console_str_send(" |");
			}
			console_str_send(" ");
			console_val_send_hex(w25q20ew_buff[k], 2);
		}
		console_str_send("\n");
	}
	
ERACE_EXIT:
	return;
}

// コマンド設定関数
void w25q20ew_set_cmd(void)
{
	COMMAND_INFO cmd;
	
	// コマンドの設定
	cmd.input = "w25q20ew get_device_id";
	cmd.func = w25q20ew_ctrl_cmd_get_device_id;
	console_set_command(&cmd);
	cmd.input = "w25q20ew get_sfdp";
	cmd.func = w25q20ew_ctrl_cmd_get_sfdp;
	console_set_command(&cmd);
	cmd.input = "w25q20ew write enable";
	cmd.func = w25q20ew_ctrl_cmd_write_enable;
	console_set_command(&cmd);
	cmd.input = "w25q20ew write disable";
	cmd.func = w25q20ew_ctrl_cmd_write_disable;
	console_set_command(&cmd);
	cmd.input = "w25q20ew read status";
	cmd.func = w25q20ew_ctrl_cmd_read_status;
	console_set_command(&cmd);
	cmd.input = "w25q20ew erace";
	cmd.func = w25q20ew_ctrl_cmd_erace;
	console_set_command(&cmd);
	cmd.input = "w25q20ew read";
	cmd.func = w25q20ew_ctrl_cmd_read;
	console_set_command(&cmd);
}
