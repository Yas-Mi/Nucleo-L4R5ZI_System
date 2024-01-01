/*
 * w25q20ew_cmd.c
 *
 *  Created on: 2023/8/29
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "octspi.h"
#include <string.h>

#include "w25q20ew_cmd.h"
#include "w25q20ew_ctrl.h"
#include "w25q20ew_common.h"

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

// ライトイネーブル
int32_t w25q20ew_cmd_write_enable(void)
{
	const OCTOSPI_COM_CFG *cmd_cfg;
	int32_t ret;
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_WRITE_ENABLE]);
	
	// 送信
	ret = octospi_send(W25Q20EW_OCTOSPI_CH, cmd_cfg, NULL, 0);
	
	return ret;
}

// 書き込み関数 (*)シングルピン
int32_t w25q20ew_cmd_write_single(uint32_t addr, uint8_t *data, uint8_t size)
{
	
	return 0;
}

// 書き込み関数 (*)クアッドピン
int32_t w25q20ew_cmd_write_quad(uint32_t addr, uint8_t *data, uint8_t size)
{
	
	return 0;
}


// イレース関数
int32_t w25q20ew_cmd_erase(uint32_t addr)
{
	OCTOSPI_COM_CFG cfg;
	int32_t ret;
	
	// コマンド情報を取得
	memcpy(&cfg, &(cmd_config_tbl[CMD_SECTOR_ERACE]), sizeof(OCTOSPI_COM_CFG));
	cfg.addr = addr;
	
	// 送信
	ret = octospi_send(W25Q20EW_OCTOSPI_CH, &cfg, NULL, 0);
	
	return ret;
}

// ライトディセーブル
int32_t w25q20ew_cmd_write_disable(void)
{
	const OCTOSPI_COM_CFG *cmd_cfg;
	int32_t ret;
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_WRITE_DISABLE]);
	
	// 送信
	ret = octospi_send(W25Q20EW_OCTOSPI_CH, cmd_cfg, NULL, 0);
	
	return ret;
}

// 読み込み関数
int32_t w25q20ew_cmd_read_single(uint32_t addr, uint8_t *data, uint8_t size)
{
	OCTOSPI_COM_CFG cfg;
	int32_t ret;
	
	// コマンド情報を取得
	memcpy(&cfg, &(cmd_config_tbl[CMD_READ_DATA]), sizeof(OCTOSPI_COM_CFG));
	cfg.addr = addr;
	
	// 受信
	ret = octospi_recv(W25Q20EW_OCTOSPI_CH, &cfg, data, size);
	
	return ret;
}

// ステータスリード1
int32_t w25q20ew_cmd_read_status_1(uint8_t *data)
{
	const OCTOSPI_COM_CFG *cmd_cfg;
	int32_t ret;
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_READ_STATUS_REGISTER_1]);
	
	// 受信
	ret = octospi_recv(W25Q20EW_OCTOSPI_CH, cmd_cfg, data, sizeof(uint8_t));
	
	return ret;
}

// ステータスリード2
int32_t w25q20ew_cmd_read_status_2(uint8_t *data)
{
	const OCTOSPI_COM_CFG *cmd_cfg;
	int32_t ret;
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_READ_STATUS_REGISTER_2]);
	
	// 受信
	ret = octospi_recv(W25Q20EW_OCTOSPI_CH, cmd_cfg, data, sizeof(uint8_t));
	
	return ret;
}

// ステータスライト2
int32_t w25q20ew_cmd_write_status_2(uint8_t *data)
{
	const OCTOSPI_COM_CFG *cmd_cfg;
	int32_t ret;
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_WRITE_STATUS_REGISTER_2]);
	
	// 受信
	ret = octospi_recv(W25Q20EW_OCTOSPI_CH, cmd_cfg, data, sizeof(uint8_t));
	
	return ret;
}

// デバイスID取得
// 引数には2byteの配列を渡す
int32_t w25q20ew_cmd_get_devise_id(uint8_t *id)
{
	const OCTOSPI_COM_CFG *cmd_cfg;
	int32_t ret;
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_READ_MANUFACTURER_DEVICE_ID]);
	
	// 受信
	ret = octospi_recv(W25Q20EW_OCTOSPI_CH, cmd_cfg, id, 2);
	
	return ret;
}

// SFDP取得
// 引数には256byteの配列を渡す
int32_t w25q20ew_cmd_get_sfdp(uint8_t *sfdp)
{
	const OCTOSPI_COM_CFG *cmd_cfg;
	int32_t ret;
	
	// コマンド情報を取得
	cmd_cfg = &(cmd_config_tbl[CMD_READ_SFDP_REGISTER]);
	
	// 受信
	ret = octospi_recv(W25Q20EW_OCTOSPI_CH, cmd_cfg, sfdp, 256);
	
	return ret;
}

