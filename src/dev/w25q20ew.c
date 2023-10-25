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

#include "w25q20ew.h"

// 状態
#define ST_UNINITIALIZED	(0)		// 未初期化
#define ST_INITIALIZED		(1)		// 初期化
#define ST_IDLE				(2)		// アイドル
#define ST_RUNNING			(3)		// 実行中
#define ST_MAX				(4)

// コマンド
#define CMD_WRITE_ENABLE									(0)
#define CMD_WRITE_ENABLE_FOR_VOLATILE_STATUS_REGISTER		(1)
#define CMD_WRITE_DISABLE									(2)
#define CMD_READ_STATUS_REGISTER_1							(3)
#define CMD_READ_STATUS_REGISTER_1							(4)
#define CMD_WRITE_STATUS_REGISTER_1							(5)
#define CMD_WRITE_STATUS_REGISTER_2							(6)
#define CMD_READ_DATA										(7)
#define CMD_FAST_READ										(8)
#define CMD_FAST_READ_DUAL_OUTPUT							(9)
#define CMD_FAST_READ_QUAD_OUTPUT							(10)
#define CMD_FAST_READ_DUAL_IO								(11)
#define CMD_FAST_READ_QUAD_IO								(12)
#define CMD_SET_BURST_WITH_WRAP								(13)
#define CMD_PAGE_PROGRAM									(14)
#define CMD_QUAD_INPUT_PAGE_PROGRAM							(15)
#define CMD_32KB_BLOCK_ERASE								(16)
#define CMD_64KB_BLOCK_ERASE								(17)
#define CMD_CHIP_ERASE										(18)
#define CMD_ERASE_PROGRAM_SUSPEND							(19)
#define CMD_ERASE_PROGRAM_RESUME							(20)
#define CMD_POWER_DOWN										(21)
#define CMD_READ_MANUFACTURER_DEVICE_ID						(22)
#define CMD_READ_MANUFACTURER_DEVICE_ID_DUAL_IO				(23)
#define CMD_READ_MANUFACTURER_DEVICE_ID_QUAD_IO				(24)
#define CMD_READ_UNIQUE_ID_NUMBER							(25)
#define CMD_READ_JDEC_ID									(26)
#define CMD_READ_SFDP_REGISTER								(27)
#define CMD_ERASE_SECURITY_REGISTER							(28)
#define CMD_PROGRAM_SECURITY_REGISTER						(29)
#define CMD_MAX												(30)

// マクロ
#define W25Q20EW_OCTOSPI_CH			OCTOSPI_CH_1	// 使用するUSARTのチャネル
// デバイス情報
#define W25Q20EW_MEM_SIZE			(256*1024)		// デバイスのメモリサイズ
#define W25Q20EW_1PAGE				(256)			// 1ページのサイズ

// コマンド情報テーブル
static const OCTOSPI_COM_CFG cmd_config_tbl[CMD_MAX] = {
	// inst		inst_size			inst_if,				addr	addr_size			addr_if				dummy_cycle		data_size	data_if				data	alternate_size		alternate_if		alternate_all_size
	{ 0x06,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x50,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x04,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x05,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x35,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x01,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x31,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x03,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x0B,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x3B,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x6B,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0xBB,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0xEB,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x77,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x02,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x32,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x52,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0xD8,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0xC7,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x75,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x7A,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0xB9,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x90,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x92,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x94,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x4B,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x9F,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x5A,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x44,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
	{ 0x42,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,		OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				0,			OCTOSPI_IF_NONE,	NULL,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },
};

// 制御用ブロック
typedef struct {
	uint32_t			state;					// 状態
	W25Q20EW_CALLBACK	callback;				// コールバック
	void				*callback_vp;			// コールバックパラメータ
} W25Q20EW_CTL;
static W25Q20EW_CTL w25q20ew_ctl;

// OCTOSPIオープンパラメータ
static const OCTOSPI_OPEN open_par = {
	50*1000*1000,					// 50MHz
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

// OCTOSPIのコールバック(割込みコンテキスト)
static void octospi_callback(void *vp)
{
	W25Q20EW_CTL *this = (W25Q20EW_CTL*)vp;
}

// 初期化関数
int32_t w25q20ew_init(void)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	int32_t ret;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(W25Q20EW_CTL));
	
	// 状態の更新
	this->state = ST_INITIALIZED;
	
	return 0;
}

int32_t w25q20ew_open(W25Q20EW_CALLBACK callback, void* callback_vp)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	int32_t ret;
	
	// octospiオープン
	ret = octospi_open(W25Q20EW_OCTOSPI_CH, &open_par, octospi_callback, this);
	if (ret != 0) {
		return -1;
	}
	
	// コールバック登録
	this->callback = callback;
	this->callback_vp = callback_vp;
	
	// 状態の更新
	this->state = ST_IDLE;
	
	return 0;
}

// ライトイネーブル
int32_t w25q20ew_write_enable(void)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	OCTOSPI_COM_CFG *cmd_cfg;
	int32_t ret;
	
	// アイドル出なければ終了
	if (this->state != ST_IDLE) {
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_WRITE_ENABLE]);
	
	// 送信
	ret = octspi_send(W25Q20EW_OCTOSPI_CH, cmd_cfg);
	if (ret != 0) {
		return -1;
	}
	
	return 0;
}
#if 0
// 書き込み関数
int32_t w25q20ew_send(BT_SEND_TYPE type, uint8_t *data, uint8_t size)
{
	BT_CTL *this = &bt_ctl;
	BT_MSG *msg;
	uint8_t i;
	
	// NULLの場合はエラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// アイドル出なければ終了
	if (this->state != ST_IDLE) {
		return -1;
	}
	
	return 0;
}

// 読み込み関数
int32_t w25q20ew_read(BT_SEND_TYPE type, uint8_t *data, uint8_t size)
{
	BT_CTL *this = &bt_ctl;
	BT_MSG *msg;
	uint8_t i;
	
	// NULLの場合はエラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// アイドル出なければ終了
	if (this->state != ST_IDLE) {
		return -1;
	}
	
	return 0;
}

// コマンド設定関数
void w25q20ew_set_cmd(void)
{
	COMMAND_INFO cmd;
	
	// コマンドの設定
	cmd.input = "bt_dev connect_check";
	cmd.func = bt_dev_cmd_connect_check;
	console_set_command(&cmd);
}
#endif
