/*
 * flash_mng.c
 *
 *  Created on: 2023/10/18
 *      Author: ronald
 */
#include <string.h>
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "intr.h"
#include "w25q20ew_ctrl.h"

#include "flash_mng.h"

// マクロ
#define FLASH_MNG_TIMEOUT	(1000)	// タイムアウト 1000ms

// 状態
#define ST_UNINITIALIZED	(0)		// 未初期化
#define ST_INITIALIZED		(1)		// 初期化済
#define ST_IDLE				(2)		// アイドル
#define ST_RUNNING			(3)		// 実行中
#define ST_MAX				(4)

// 制御用ブロック
typedef struct {
	uint32_t			state;					// 状態
} FLASH_MNG_CTL;
static FLASH_MNG_CTL fls_mng_ctl[FLASH_MNG_KIND_MAX];

// フラッシュ関数情報
typedef int32_t (*FLASH_MNG_INIT)(void);
typedef int32_t (*FLASH_MNG_OPEN)(void);
typedef int32_t (*FLASH_MNG_CLOSE)(void);
typedef int32_t (*FLASH_MNG_WRITE)(uint32_t addr, uint8_t *data, uint8_t size);
typedef int32_t (*FLASH_MNG_ERASE)(uint32_t addr);
typedef int32_t (*FLASH_MNG_READ)(uint32_t addr, uint8_t *data, uint8_t size);
typedef int32_t (*FLASH_MNG_SET_MEM_MAPPED)(void);
typedef struct {
	FLASH_MNG_INIT				init;			// 初期化
	FLASH_MNG_OPEN				open;			// オープン
	FLASH_MNG_CLOSE				close;			// クローズ
	FLASH_MNG_WRITE				write;			// ライト
	FLASH_MNG_ERASE				erase;			// イレース
	FLASH_MNG_READ				read;			// リード
	FLASH_MNG_SET_MEM_MAPPED	set_mem_mapped;	// メモリマップドモード
} FLASH_MNG_FUNC;

// フラッシュ関数情報テーブル
static const FLASH_MNG_FUNC fls_mng_func_tbl[FLASH_MNG_KIND_MAX] =
{
	// FLASH_MNG_KIND_W25Q20EW
	{
		w25q20ew_init,
		w25q20ew_open,
		w25q20ew_close,
		w25q20ew_write,
		w25q20ew_erase,
		w25q20ew_read,
		w25q20ew_set_memory_mappd,
	},
	// ここに他のFLASHを追加していく
};

// デバッグ用のRAM
#define FLASH_MNG_BUFF_SIZE		(4*1024)
static uint8_t flash_mng_buff[FLASH_MNG_BUFF_SIZE];

// 初期化関数
int32_t flash_mng_init(void)
{
	FLASH_MNG_CTL *this;
	const FLASH_MNG_FUNC *func;
	uint8_t i;
	int32_t ret;
	
	// 各Flashの初期化とオープン
	for (i = 0; i < FLASH_MNG_KIND_MAX; i++) {
		// thisの初期化
		this = &(fls_mng_ctl[i]);
		memset(this, 0, sizeof(FLASH_MNG_CTL));
		
		// 関数テーブルを取得
		func = &(fls_mng_func_tbl[i]);
		
		// 初期化
		ret = func->init();
		if (ret != 0) {
			return -1;
		}
		
		// 状態の更新
		this->state = ST_INITIALIZED;
	}
	
	return 0;
}

// オープン関数
int32_t flash_mng_open(uint32_t kind)
{
	FLASH_MNG_CTL *this;
	const FLASH_MNG_FUNC *func;
	int32_t ret;
	
	// パラメータチェック
	if (kind >= FLASH_MNG_KIND_MAX) {
		return -1;
	}
	
	// コンテキストを取得
	this = &(fls_mng_ctl[kind]);
	
	// 初期化済みでない場合は終了
	if (this->state != ST_INITIALIZED) {
		return -1;
	}
	
	// 関数テーブルを取得
	func = &(fls_mng_func_tbl[kind]);
	
	// オープン
	ret = func->open();
	if (ret != 0) {
		return -1;
	}
	
	// 状態の更新
	this->state = ST_IDLE;
	
	return 0;
}

// イレース
int32_t flash_mng_erace(uint32_t kind, uint32_t addr)
{
	FLASH_MNG_CTL *this;
	const FLASH_MNG_FUNC *func;
	int32_t ret;
	
	// パラメータチェック
	if (kind >= FLASH_MNG_KIND_MAX) {
		return -1;
	}
	
	// コンテキストを取得
	this = &(fls_mng_ctl[kind]);
	
	// 割込み禁止 (*) 複数のタスクから呼ばれることを想定している
	INTR_DISABLE;
	// アイドル出ない場合は終了
	if (this->state != ST_IDLE) {
		// 割込み禁止解除
		INTR_ENABLE;
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	// 割込み禁止解除
	INTR_ENABLE;
	
	// 関数テーブルを取得
	func = &(fls_mng_func_tbl[kind]);
	
	// イレース
	ret = func->erase(addr);
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return ret;
}

// 書き込み
int32_t flash_mng_write(uint32_t kind, uint32_t addr, uint8_t *data, uint32_t size)
{
	FLASH_MNG_CTL *this;
	const FLASH_MNG_FUNC *func;
	int32_t ret;
	
	// NULLの場合はエラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// パラメータチェック
	if (kind >= FLASH_MNG_KIND_MAX) {
		return -1;
	}
	
	// コンテキストを取得
	this = &(fls_mng_ctl[kind]);
	
	// 割込み禁止 (*) 複数のタスクから呼ばれることを想定している
	INTR_DISABLE;
	// アイドル出ない場合は終了
	if (this->state != ST_IDLE) {
		// 割込み禁止解除
		INTR_ENABLE;
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	// 割込み禁止解除
	INTR_ENABLE;
	
	// 関数テーブルを取得
	func = &(fls_mng_func_tbl[kind]);
	
	// ライトイネーブル
	ret = func->write(addr, data, size);
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return ret;
}

// リード関数
int32_t flash_mng_read(uint32_t kind, uint32_t addr, uint8_t *data, uint32_t size)
{
	FLASH_MNG_CTL *this = (FLASH_MNG_CTL*)&fls_mng_ctl;
	const FLASH_MNG_FUNC *func;
	int32_t ret;
	
	// NULLの場合はエラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// パラメータチェック
	if (kind >= FLASH_MNG_KIND_MAX) {
		return -1;
	}
	
	// コンテキストを取得
	this = &(fls_mng_ctl[kind]);
	
	// 割込み禁止 (*) 複数のタスクから呼ばれることを想定している
	INTR_DISABLE;
	// アイドル出ない場合は終了
	if (this->state != ST_IDLE) {
		// 割込み禁止解除
		INTR_ENABLE;
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	// 割込み禁止解除
	INTR_ENABLE;
	
	// 関数テーブルを取得
	func = &(fls_mng_func_tbl[kind]);
	
	// リード
	ret = func->read(addr, data, size);
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return ret;
}

// リード関数
int32_t flash_mng_set_memory_mapped(uint32_t kind)
{
	FLASH_MNG_CTL *this = (FLASH_MNG_CTL*)&fls_mng_ctl;
	const FLASH_MNG_FUNC *func;
	int32_t ret;
	
	// パラメータチェック
	if (kind >= FLASH_MNG_KIND_MAX) {
		return -1;
	}
	
	// コンテキストを取得
	this = &(fls_mng_ctl[kind]);
	
	// 割込み禁止 (*) 複数のタスクから呼ばれることを想定している
	INTR_DISABLE;
	// アイドル出ない場合は終了
	if (this->state != ST_IDLE) {
		// 割込み禁止解除
		INTR_ENABLE;
		return -1;
	}
	// 状態を更新
	this->state = ST_RUNNING;
	// 割込み禁止解除
	INTR_ENABLE;
	
	// 関数テーブルを取得
	func = &(fls_mng_func_tbl[kind]);
	
	// リード
	ret = func->set_mem_mapped();
	
	// 状態を更新
	this->state = ST_IDLE;
	
	return ret;
}

// コマンド
static void flash_mng_cmd_write(void)
{
	int32_t ret;
	
	// メモリの初期化
	memset(flash_mng_buff, 0x5A, sizeof(flash_mng_buff));
	
	ret = flash_mng_write(FLASH_MNG_KIND_W25Q20EW, 0, flash_mng_buff, FLASH_MNG_BUFF_SIZE);
	if (ret != E_OK) {
		console_str_send("flash_mng_write error\n");
	}
}

static void flash_mng_cmd_erace(void)
{
	int32_t ret;
	
	ret = flash_mng_erace(FLASH_MNG_KIND_W25Q20EW, 0);
	if (ret != E_OK) {
		console_str_send("flash_mng_erace error\n");
	}
}

static void flash_mng_cmd_read(void)
{
	int32_t ret;
	uint32_t i, j, k;
	
	// 初期化
	memset(flash_mng_buff, 0, sizeof(flash_mng_buff));
	
	ret = flash_mng_read(FLASH_MNG_KIND_W25Q20EW, 0, flash_mng_buff, FLASH_MNG_BUFF_SIZE);
	if (ret != E_OK) {
		console_str_send("flash_mng_erace error\n");
		return;
	}
	// 表示
	console_str_send("==================== read ============================\n");
	console_str_send("     |  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
	console_str_send("------------------------------------------------------\n");
	for (i = 0; i < 16; i++) {
		for (j = 0; j < FLASH_MNG_BUFF_SIZE/16; j++) {
			k = i*16+j;
			if (j == 0) {
				// アドレス表示
				console_str_send(" |");
			}
			console_str_send(" ");
			console_val_send_hex(flash_mng_buff[k], 2);
		}
		console_str_send("\n");
	}
}

static void flash_mng_cmd_set_mem_mapped(void)
{
	int32_t ret;
	
	ret = flash_mng_set_memory_mapped(FLASH_MNG_KIND_W25Q20EW);
	if (ret != E_OK) {
		console_str_send("flash_mng_set_memory_mapped error\n");
	}
}

static void flash_mng_cmdt_mem_mapped_read(void)
{
	uint8_t *base_addr = (uint8_t*)0x90000000;
	uint32_t i, j, k;
	
	// 表示
	console_str_send("==================== read ============================\n");
	console_str_send("     |  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
	console_str_send("------------------------------------------------------\n");
	for (i = 0; i < 16; i++) {
		for (j = 0; j < FLASH_MNG_BUFF_SIZE/16; j++) {
			k = i*16+j;
			if (j == 0) {
				// アドレス表示
				console_str_send(" |");
			}
			console_str_send(" ");
			console_val_send_hex(*(base_addr+k), 2);
		}
		console_str_send("\n");
	}
}

// コマンド設定関数
void flash_mng_set_cmd(void)
{
	COMMAND_INFO cmd;
	
	// コマンドの設定
	cmd.input = "flash_mng erace";
	cmd.func = flash_mng_cmd_erace;
	console_set_command(&cmd);
	// コマンドの設定
	cmd.input = "flash_mng read";
	cmd.func = flash_mng_cmd_read;
	console_set_command(&cmd);
	// コマンドの設定
	cmd.input = "flash_mng write";
	cmd.func = flash_mng_cmd_write;
	console_set_command(&cmd);
	// コマンドの設定
	cmd.input = "flash_mng set_mem_mapped";
	cmd.func = flash_mng_cmd_set_mem_mapped;
	console_set_command(&cmd);
	// コマンドの設定
	cmd.input = "flash_mng mem_mapped_read";
	cmd.func = flash_mng_cmdt_mem_mapped_read;
	console_set_command(&cmd);
}
