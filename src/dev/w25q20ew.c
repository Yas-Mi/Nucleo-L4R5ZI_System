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

// 状態
#define ST_UNINITIALIZED	(0)		// 未初期化
#define ST_IDLE				(1)		// アイドル
#define ST_RUNNING			(2)		// 実行中
#define ST_MAX				(3)

// イベント
#define EVENT_CONNECT		(0)
#define EVENT_DISCONNECT	(1)
#define EVENT_SEND			(2)
#define EVENT_CHECK_STS		(3)
#define EVENT_MAX			(4)

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
	// inst		inst_size	inst_if,				addr	addr_size	addr_if				dummy_cycle		data_size	data_if				data	alternate_size		alternate_if
	{ 0x06,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x50,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x04,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x05,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x35,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x01,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x31,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x03,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x0B,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x3B,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x6B,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0xBB,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0xEB,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x77,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x02,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x32,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x52,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0xD8,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0xC7,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x75,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x7A,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0xB9,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x90,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x92,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x94,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x4B,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x9F,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x5A,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x44,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
	{ 0x42,		1, 			OCTOSPI_IF_SINGLE,		0,		0,			OCTOSPI_IF_MAX,		0,				0			OCTOSPI_IF_MAX,		NULL,	0,					OCTOSPI_IF_MAX },
};

// 制御用ブロック
typedef struct {
	uint32_t			state;					// 状態
	kz_thread_id_t		tsk_id;					// タスクID
	BT_RCV_CALLBACK		callback;				// コールバック
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

// OCTOSPIのコールバック
static void octospi_callback(void *vp)
{
	W25Q20EW_CTL *this = (W25Q20EW_CTL*)vp;
	
	
	
	
}

// BlueTooth初期化関数
int32_t w25q20ew_init(void)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	int32_t ret;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(W25Q20EW_CTL));
	
	// octospiオープン
	ret = octospi_open(W25Q20EW_OCTOSPI_CH, &open_par);
	if (ret != 0) {
		return -1;
	}
	
	// 状態の更新
	this->state = ST_IDLE;
	
	return 0;
}

// ライトイネーブル
int32_t w25q20ew_write_enable(void)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	uint32_t state;
	OCTOSPI_COM_CFG *cmd_cfg;
	
	// NULLの場合はエラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// 割込み禁止 (*) 複数のタスクから呼ばれることを想定している
	INTR_DISABLE;
	// 未初期化の場合はエラーを返して終了
	if (state != ST_IDLE) {
		// 割込み禁止解除
		INTR_ENABLE;
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	// 割込み禁止解除
	INTR_ENABLE;
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_WRITE_ENABLE]);
	
	// 送信
	ret = octspi_send(W25Q20EW_OCTOSPI_CH, cmd_cfg);
	if (ret != 0) {
		return -1;
	}
	
	// 通信完了まで寝る
	this->tsk_id = kz_getid();
	kz_sleep();
	
	
	return 0;
}

// BlueTooth送信通知関数
int32_t w25q20ew_send(BT_SEND_TYPE type, uint8_t *data, uint8_t size)
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
	
	
	
	
}

// BlueTooth送信通知関数
int32_t w25q20ew_read(BT_SEND_TYPE type, uint8_t *data, uint8_t size)
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
