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

#include "w25q20ew.h"

// マクロ
#define W25Q20EW_OCTOSPI_CH			OCTOSPI_CH_1	// 使用するUSARTのチャネル
#define W25Q20EW_POLLING_PERIOD		(1)				// 値をチェックするポーリング周期

// デバイス情報
#define W25Q20EW_START_ADDRESS		(0x00000000)	// 先頭アドレス
#define W25Q20EW_END_ADDRESS		(0x0003FFFF)	// 終了アドレス
#define W25Q20EW_MEM_SIZE			(256*1024)		// デバイスのメモリサイズ
#define W25Q20EW_1PAGE_SIZE			(256)			// 1ページのサイズ
#define W25Q20EW_SECTOR_SIZE		(4*1024)		// セクターサイズ
#define W25Q20EW_BLOCK_SIZE			(16*W25Q20EW_SECTOR_SIZE)	
													// ブロックサイズ

// コンパイルスイッチ
#define READ_ACCESS_SINGLE
//#define READ_ACCESS_SINGLE
//#define READ_ACCESS_SINGLE
//#define READ_ACCESS_SINGLE

// 状態
#define ST_UNINITIALIZED	(0)		// 未初期化
#define ST_INITIALIZED		(1)		// 初期化
#define ST_IDLE				(2)		// アイドル
#define ST_RUNNING			(3)		// 実行中
#define ST_MAX				(4)

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

// コマンド
#define CMD_WRITE_ENABLE									(0)
#define CMD_WRITE_ENABLE_FOR_VOLATILE_STATUS_REGISTER		(1)
#define CMD_WRITE_DISABLE									(2)
#define CMD_READ_STATUS_REGISTER_1							(3)
#define CMD_READ_STATUS_REGISTER_2							(4)
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
#define CMD_SECTOR_ERACE									(16)
#define CMD_32KB_BLOCK_ERASE								(17)
#define CMD_64KB_BLOCK_ERASE								(18)
#define CMD_CHIP_ERASE										(19)
#define CMD_ERASE_PROGRAM_SUSPEND							(20)
#define CMD_ERASE_PROGRAM_RESUME							(21)
#define CMD_POWER_DOWN										(22)
#define CMD_READ_MANUFACTURER_DEVICE_ID						(23)
#define CMD_READ_MANUFACTURER_DEVICE_ID_DUAL_IO				(24)
#define CMD_READ_MANUFACTURER_DEVICE_ID_QUAD_IO				(25)
#define CMD_READ_UNIQUE_ID_NUMBER							(26)
#define CMD_READ_JDEC_ID									(27)
#define CMD_READ_SFDP_REGISTER								(28)
#define CMD_ERASE_SECURITY_REGISTER							(29)
#define CMD_PROGRAM_SECURITY_REGISTER						(30)
#define CMD_MAX												(31)

// コマンド情報テーブル
static const OCTOSPI_COM_CFG cmd_config_tbl[CMD_MAX] = {
	// inst		inst_size			inst_if,				addr			addr_size			addr_if				dummy_cycle		data_if				alternate_size		alternate_if		alternate_all_size
	{ 0x06,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_WRITE_ENABLE
	{ 0x50,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_WRITE_ENABLE_FOR_VOLATILE_STATUS_REGISTER
	{ 0x04,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_WRITE_DISABLE
	{ 0x05,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_SINGLE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_READ_STATUS_REGISTER_1
	{ 0x35,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_SINGLE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_READ_STATUS_REGISTER_2
	{ 0x01,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_WRITE_STATUS_REGISTER_1
	{ 0x31,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_WRITE_STATUS_REGISTER_2
	{ 0x03,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_24BIT,	OCTOSPI_IF_SINGLE,	0,				OCTOSPI_IF_SINGLE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_READ_DATA
	{ 0x0B,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_FAST_READ
	{ 0x3B,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_FAST_READ_DUAL_OUTPUT
	{ 0x6B,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_FAST_READ_QUAD_OUTPUT
	{ 0xBB,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_FAST_READ_DUAL_IO
	{ 0xEB,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_FAST_READ_QUAD_IO
	{ 0x77,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_SET_BURST_WITH_WRAP
	{ 0x02,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_PAGE_PROGRAM
	{ 0x32,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_QUAD_INPUT_PAGE_PROGRAM
	{ 0x20,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_24BIT,	OCTOSPI_IF_SINGLE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_SECTOR_ERACE
	{ 0x52,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_32KB_BLOCK_ERASE
	{ 0xD8,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_64KB_BLOCK_ERASE
	{ 0xC7,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_CHIP_ERASE
	{ 0x75,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_ERASE_PROGRAM_SUSPEND
	{ 0x7A,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_ERASE_PROGRAM_RESUME
	{ 0xB9,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_POWER_DOWN
	{ 0x90,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0x00000000,		OCTOSPI_SZ_24BIT,	OCTOSPI_IF_SINGLE,	0,				OCTOSPI_IF_SINGLE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_READ_MANUFACTURER_DEVICE_ID
	{ 0x92,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_READ_MANUFACTURER_DEVICE_ID_DUAL_IO
	{ 0x94,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_READ_MANUFACTURER_DEVICE_ID_QUAD_IO
	{ 0x4B,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_READ_UNIQUE_ID_NUMBER
	{ 0x9F,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_READ_JDEC_ID
	{ 0x5A,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0x00000000,		OCTOSPI_SZ_24BIT,	OCTOSPI_IF_SINGLE,	8,				OCTOSPI_IF_SINGLE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_READ_SFDP_REGISTER
	{ 0x44,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_ERASE_SECURITY_REGISTER
	{ 0x42,		OCTOSPI_SZ_8BIT,	OCTOSPI_IF_SINGLE,		0,				OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0,				OCTOSPI_IF_NONE,	OCTOSPI_SZ_MAX,		OCTOSPI_IF_NONE,	0 },	// CMD_PROGRAM_SECURITY_REGISTER
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
	//50*1000*1000,					// 50MHz
	100*1000,						// 1kHz
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

// SFDPヘッダ
typedef struct {
	char signature[4];		// 'S','F','D','P'
	uint8_t mir_rev;		// SFDP Minor Revison
	uint8_t mar_rev;		// SFDP Major Revison
	uint8_t nph;			// Number of Parameter Headers
	uint8_t unused_0;		// Unused
} SFDP_HEADER;

// SFDPパラメータヘッダ
typedef struct {
	uint8_t jedec_id;		// JEDEC ID
	uint8_t par_mir_rev;	// Parameter Minor Revison
	uint8_t par_mar_rev;	// Parameter Major Revison
	uint8_t par_len;		// Parameter Length
	uint8_t ptp[3];			// Parameter Table Pointer
	uint8_t unused_1;		// Unused
} SFDP_PARAMTER_HEADER;

// SFDP
typedef struct {
	SFDP_HEADER				header;
	SFDP_PARAMTER_HEADER	param_header;
} SFDP;

// OCTOSPIのコールバック (*) 今のところ未使用
static void octospi_callback(void *vp)
{
	;
}

// ポーリングでリードレジスタの値をチェック
static int32_t w25q20ew_polling_read_reg(uint32_t bit, uint32_t flag, uint32_t timeout)
{
	const OCTOSPI_COM_CFG *cmd_cfg;
	uint8_t status = 0;
	uint32_t cnt;
	int32_t ret = E_TMOUT;
	int32_t drv_ret;
	uint8_t expected_status;
	
	// ポーリング用にカウンタを計算
	cnt = timeout/W25Q20EW_POLLING_PERIOD;
	if (cnt == 1) {
		cnt = 1;
	}
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_READ_STATUS_REGISTER_1]);
	
	// 期待するステータスを設定
	expected_status = (flag << (bit - 1));
	
	// ポーリング
	while(cnt-- > 0) {
		// ステータス受信
		drv_ret = octospi_recv(W25Q20EW_OCTOSPI_CH, cmd_cfg, &status, sizeof(status));
		// 期待した値であれば終了
		if ((drv_ret == E_OK) && ((status & bit) == expected_status)) {
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
	
	return 0;
}

int32_t w25q20ew_open(void)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	int32_t ret;
	
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
	const OCTOSPI_COM_CFG *cmd_cfg;
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
	ret = octospi_send(W25Q20EW_OCTOSPI_CH, cmd_cfg, NULL, 0);
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return 0;
}

// WELの値チェック
int32_t w25q20ew_wait_wel(uint32_t flag, uint32_t timeout)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	int32_t ret;
	
	// アイドル出なければ終了
	if (this->state != ST_IDLE) {
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	
	// リード
	ret = w25q20ew_polling_read_reg(FLASH_STATUS_1_WEL, flag, timeout);
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return ret;
}

// ライト完了まで待つ
int32_t w25q20ew_wait_wr_proc(uint32_t timeout)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	int32_t ret;
	
	// アイドル出なければ終了
	if (this->state != ST_IDLE) {
		return -1;
	}
	
	// 状態を更新
	this->state = ST_RUNNING;
	
	// リード
	ret = w25q20ew_polling_read_reg(FLASH_STATUS_1_BUSY, 0, timeout);
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return ret;
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
	
	// コマンド情報を取得
	memcpy(&cfg, &(cmd_config_tbl[CMD_SECTOR_ERACE]), sizeof(OCTOSPI_COM_CFG));
	cfg.addr = addr;
	
	// 送信
	ret = octospi_send(W25Q20EW_OCTOSPI_CH, &cfg, NULL, 0);
	
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
	// コマンド情報を取得
	memcpy(&cfg, &(cmd_config_tbl[CMD_READ_DATA]), sizeof(OCTOSPI_COM_CFG));
	cfg.addr = addr;
	
#else
	// アクセス終了
	goto ACCESS_END;
#endif
	
	// 受信
	ret = octospi_recv(W25Q20EW_OCTOSPI_CH, &cfg, data, size);
	
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
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_READ_MANUFACTURER_DEVICE_ID]);
	
	// 受信
	ret = octospi_recv(W25Q20EW_OCTOSPI_CH, cmd_cfg, id, 2);
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return ret;
}

// SFDP取得
// 引数には256byteの配列を渡す
int32_t w25q20ew_get_sfdp(uint8_t *sfdp)
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
	
	// コマンド情報を取得
	memset(sfdp, 0, sizeof(sfdp));
	cmd_cfg = &(cmd_config_tbl[CMD_READ_SFDP_REGISTER]);
	
	// 受信
	ret = octospi_recv(W25Q20EW_OCTOSPI_CH, cmd_cfg, sfdp, 256);
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return ret;
}

// ライトディセーブル
int32_t w25q20ew_write_disable(void)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	const OCTOSPI_COM_CFG *cmd_cfg;
	int32_t ret;
	
	// アイドル出なければ終了
	if (this->state != ST_IDLE) {
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_WRITE_DISABLE]);
	
	// 送信
	ret = octospi_send(W25Q20EW_OCTOSPI_CH, cmd_cfg, NULL, 0);
	if (ret != 0) {
		return -1;
	}
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return 0;
}

// コマンド
static void w25q20ew_cmd_get_device_id(void)
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
static void w25q20ew_cmd_get_sfdp(void)
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
static void w25q20ew_cmd_write_enable(void)
{
	const OCTOSPI_COM_CFG *cmd_cfg;
	uint8_t status = 0;
	int32_t ret;
	
	// ライトイネーブル
	console_str_send("write enable\n");
	ret = w25q20ew_write_enable();
	if (ret != E_OK) {
		console_str_send("write enable error\n");
		goto WRITE_ENABLE_EXIT;
	}
	
WRITE_ENABLE_EXIT:
	return;
}

// コマンド
static void w25q20ew_cmd_write_disable(void)
{
	int32_t ret;
	
	// ライトイネーブル
	console_str_send("write disable\n");
	ret = w25q20ew_write_disable();
	if (ret != E_OK) {
		console_str_send("write disable error\n");
		goto WRITE_DISABLE_EXIT;
	}
	
WRITE_DISABLE_EXIT:
	return;
}

// コマンド
static void w25q20ew_cmd_read_status(void)
{
	const OCTOSPI_COM_CFG *cmd_cfg;
	uint8_t status = 0;
	int32_t ret;
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_READ_STATUS_REGISTER_1]);
	
	// ステータス取得
	ret = octospi_recv(W25Q20EW_OCTOSPI_CH, cmd_cfg, &status, sizeof(status));
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

// コマンド
static void w25q20ew_cmd_erace(void)
{
	int32_t ret;
	
	// ライトイネーブル
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
static void w25q20ew_cmd_read(void)
{
	int32_t ret;
	uint32_t i, j, k;
	
	// 初期化
	memset(w25q20ew_buff, 0, sizeof(W25Q20EW_BUFF_SIZE));
	
	// ライトイネーブル
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
		for (j = 0; j < W25Q20EW_BUFF_SIZE/16; j++) {
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
	cmd.func = w25q20ew_cmd_get_device_id;
	console_set_command(&cmd);
	cmd.input = "w25q20ew get_sfdp";
	cmd.func = w25q20ew_cmd_get_sfdp;
	console_set_command(&cmd);
	cmd.input = "w25q20ew write enable";
	cmd.func = w25q20ew_cmd_write_enable;
	console_set_command(&cmd);
	cmd.input = "w25q20ew write disable";
	cmd.func = w25q20ew_cmd_write_disable;
	console_set_command(&cmd);
	cmd.input = "w25q20ew read status";
	cmd.func = w25q20ew_cmd_read_status;
	console_set_command(&cmd);
	cmd.input = "w25q20ew erace";
	cmd.func = w25q20ew_cmd_erace;
	console_set_command(&cmd);
	cmd.input = "w25q20ew read";
	cmd.func = w25q20ew_cmd_read;
	console_set_command(&cmd);
}
