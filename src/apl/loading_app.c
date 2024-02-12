/*
 * loading_app.c
 *
 *  Created on: 2024/1/7
 *      Author: ronald
 */
// Includes
#include <console.h>
#include <string.h>
#include <stdio.h>
#include "stm32l4xx.h"
#include "str_util.h"
#include "defines.h"
#include "kozos.h"
#include "interrupt.h"
#include "bt_dev.h"
#include "flash_mng.h"

#include "loading_app.h"

// 状態
#define ST_UNINITIALIZED	(0)
#define ST_INITIALIZED		(1)
#define ST_UNDEFINED		(2)
#define ST_MAX				(3)

// イベント
#define MSG_FLASH_PROGRAM		(0)
#define MSG_XIP					(1)
#define MSG_CLEAR				(2)
#define MSG_MAX					(3)

// マクロ
#define PROGRAM_SIZE_MAX		(128*1024)		// プログラムサイズ最大値 128kB
#define FLASH_PROGRAM_ADDR		(0x90000581)	// フラッシュプログラム (*)リセットハンドラのアドレス

// コンテキスト
typedef struct {
	uint32_t			state;				// 状態
	kz_msgbox_id_t		msg_id;				// メッセージID
	kz_thread_id_t		tsk_id;				// タスクID
	uint8_t				load_program_buf[PROGRAM_SIZE_MAX];
											// プログラム格納バッファ
	uint32_t			buf_idx;			// バッファインデックス
	uint32_t			rcv_cnt;			// 受信カウンタ
	uint32_t			conv_data[2];		// 変換データ
} LOADING_APP_CTL;
static LOADING_APP_CTL loading_app_ctl;

// メッセージ
typedef struct {
	uint32_t	type;
	uint32_t	data;
}LOADING_APP_MSG;

// 評価用
#define DEBUG_BUF_SIZE_MAX		(64*1024)		// デバッグ用のバッファサイズ
#define DEBUG_INFO_SIZE			(512)			// デバッグ用のバッファサイズ
typedef struct {
	uint32_t addr;
	uint8_t expected_val;
	uint8_t read_val;
} DBG;
static DBG dbg[DEBUG_INFO_SIZE] = {0};
static uint32_t	dbg_idx = 0;
static uint32_t	dbg_idx_ovr_flow = 0;
static uint8_t dbg_buf[DEBUG_BUF_SIZE_MAX] = {0};

// BlueToothコールバック
static void bt_dev_callback(uint8_t data, void *vp)
{
	LOADING_APP_CTL *this = (LOADING_APP_CTL*)vp;
	
	// データ格納
	this->conv_data[(this->rcv_cnt % 2)] = hex2num(data);
	
	// 受信データはアスキーのため2つの文字を受信して1byte分のデータとなる
	if ((this->rcv_cnt % 2) != 0) {
		// プログラム格納バッファに空きがある
		if (this->buf_idx < PROGRAM_SIZE_MAX) {
			// 受信データ格納
			this->load_program_buf[this->buf_idx] = this->conv_data[0]*16 + this->conv_data[1];
			this->buf_idx++;
			// クリア
			memset(this->conv_data, 0x00, sizeof(this->conv_data));
		// もう空きがない
		} else {
			console_str_send("program buf is full\n");
		}
	}
	
	// 受信カウンタ更新
	this->rcv_cnt++;
}

// プログラムをフラッシュに書く
static void loading_app_msg_write_program(uint32_t par)
{
	LOADING_APP_CTL *this = &loading_app_ctl;
	int32_t ret;
	uint32_t i;
	uint8_t data;
	uint8_t verify_ret = 1;
	char snd_str[256];
	
	// 書き込み
	ret = flash_mng_write(FLASH_MNG_KIND_W25Q20EW, 0, this->load_program_buf, this->buf_idx);
	if (ret != E_OK) {
		console_str_send("flash_mng_write error\n");
	}
	
	// ベリファイ
	for (i = 0; i < this->buf_idx; i++) {
		// 読み込み
		ret = flash_mng_read(FLASH_MNG_KIND_W25Q20EW, i, &data, 1);
		// 判定
		if (this->load_program_buf[i] != data) {
			sprintf(snd_str, "addr:%x expexted:%x read:%x\n", i, this->load_program_buf[i], data);
			console_str_send(snd_str);
			verify_ret = 0;
		}
	}
	
	if (verify_ret) {
		console_str_send("write success\n");
	}
}

// フラッシュ上のプログラムを実行する
static void loading_app_msg_xip(uint32_t par)
{
	int32_t ret;
	volatile int (*f)(void);
	
	// メモリマップドモードにする
	ret = flash_mng_set_memory_mapped(FLASH_MNG_KIND_W25Q20EW);
	if (ret != E_OK) {
		console_str_send("flash_mng_set_memory_mapped error\n");
	}
	
	// フラッシュへジャンプ
	f = (int(*)(void))(FLASH_PROGRAM_ADDR);
	f();
}

// クリア
static void loading_app_msg_clear(uint32_t par)
{
	LOADING_APP_CTL *this = &loading_app_ctl;
	
	// バッファ情報初期化
	this->buf_idx = 0;
	memset(this->load_program_buf, 0x00, PROGRAM_SIZE_MAX);
}

// 状態遷移テーブル
static const FSM fsm[ST_MAX][MSG_MAX] = {
	// MSG_FLASH_PROGRAM								MSG_XIP										MSG_CLEAR
	{{NULL,								ST_UNDEFINED},	{NULL,						ST_UNDEFINED},	{NULL,						ST_UNDEFINED},},		// ST_UNINITIALIZED
	{{loading_app_msg_write_program,	ST_UNDEFINED},	{loading_app_msg_xip,		ST_UNDEFINED},	{loading_app_msg_clear,		ST_UNDEFINED},},		// ST_INITIALIZED
	{{NULL,								ST_UNDEFINED},	{NULL,						ST_UNDEFINED},	{NULL,						ST_UNDEFINED},},		// ST_UNDEIFNED
};

// メインタスク
static int loading_app_main(int argc, char *argv[])
{	
	LOADING_APP_CTL *this = &loading_app_ctl;
	LOADING_APP_MSG *msg;
	LOADING_APP_MSG tmp_msg;
	int32_t size;
	
	// ここしかない。。。
	flash_mng_open(FLASH_MNG_KIND_W25Q20EW);
	
	while(1) {
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);
		// いったんメッセージをローカル変数にコピー
		memcpy(&tmp_msg, msg, sizeof(LOADING_APP_MSG));
		// メッセージを解放
		kz_kmfree(msg);
		// 処理を実行
		if (fsm[this->state][msg->type].func != NULL) {
			fsm[this->state][msg->type].func(tmp_msg.data);
		}
		// 状態遷移
		if (fsm[this->state][msg->type].nxt_state != ST_UNDEFINED) {
			this->state = fsm[this->state][msg->type].nxt_state;
		}
	}
	
	return 0;
}

// 初期化
void loading_app_init(void)
{
	LOADING_APP_CTL *this = &loading_app_ctl;
	int32_t ret;
	
	// コンテキスト初期化
	memset(this, 0, sizeof(LOADING_APP_CTL));
	
	// usartオープン
	ret = bt_dev_reg_callback(bt_dev_callback, this);
	// usartをオープンできなかったらエラーを返して終了
	if (ret != 0) {
		return -1;
	}
	
	// タスク作成
	this->tsk_id = kz_run(loading_app_main, "loading_app_main",  3, 0x1000, 0, NULL);
	
	// メッセージID設定
	this->msg_id = MSGBOX_ID_LOADING_APP_MAIN;
	
	// 状態の更新
	this->state = ST_INITIALIZED;
}

// ====コマンド====
static void loading_app_cmd_write_program(int argc, char *argv[])
{
	LOADING_APP_CTL *this = &loading_app_ctl;
	LOADING_APP_MSG *msg;
	
	msg = kz_kmalloc(sizeof(LOADING_APP_MSG));
	msg->type = MSG_FLASH_PROGRAM;
	msg->data = 0;
	kz_send(this->msg_id, sizeof(LOADING_APP_MSG), msg);
}

static void loading_app_cmd_xip(int argc, char *argv[])
{
	LOADING_APP_CTL *this = &loading_app_ctl;
	LOADING_APP_MSG *msg;
	
	msg = kz_kmalloc(sizeof(LOADING_APP_MSG));
	msg->type = MSG_XIP;
	msg->data = 0;
	kz_send(this->msg_id, sizeof(LOADING_APP_MSG), msg);
}

static void loading_app_cmd_clear(int argc, char *argv[])
{
	LOADING_APP_CTL *this = &loading_app_ctl;
	LOADING_APP_MSG *msg;
	
	msg = kz_kmalloc(sizeof(LOADING_APP_MSG));
	msg->type = MSG_CLEAR;
	msg->data = 0;
	kz_send(this->msg_id, sizeof(LOADING_APP_MSG), msg);
}

// メモリマップドモードの読み出し
static void loading_app_cmd_check_mem_mapped(int argc, char *argv[])
{
	LOADING_APP_CTL *this = &loading_app_ctl;
	uint8_t *base_addr = (uint8_t*)0x90000000;
	uint32_t i, j, k;
	uint8_t data;
	char snd_str[256];
	
	// 評価用情報初期化
	dbg_idx = 0;
	dbg_idx_ovr_flow = 0;
	
	// 表示
	console_str_send("==================== read ==================================\n");
	console_str_send("           |  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
	console_str_send("------------------------------------------------------------\n");
	for (i = 0; i < 128*1024; i++) {
		for (j = 0; j < 16; j++) {
			k = i*16+j;
			// 最後まで読んだら終了
			if (this->buf_idx <= k) {
				goto EXIT;
			}
			// アドレス表示
			if (j == 0) {
				console_str_send("0x");
				console_val_send_hex_ex(k, 8);
				console_str_send(" |");
			}
			// データ取得して表示
			data = *(base_addr+k);
			console_str_send(" ");
			console_val_send_hex_ex(data, 2);
			// 間違っていたらその情報を保存
			if (this->load_program_buf[k] != data) {
				if (dbg_idx < DEBUG_INFO_SIZE) {
					dbg[dbg_idx].addr = k;
					dbg[dbg_idx].expected_val = this->load_program_buf[k];
					dbg[dbg_idx].read_val = data;
					dbg_idx++;
				} else {
					dbg_idx_ovr_flow = 1;
				}
			}
		}
		console_str_send("\n");
	}
EXIT:
	console_str_send("\n");
	
	// 間違っていた情報を表示
	sprintf(snd_str, "dbg_idx:%d dbg_idx_ovr_flow:%d\n", dbg_idx, dbg_idx_ovr_flow);
	console_str_send(snd_str);
	for (i = 0; i < dbg_idx; i++) {
		sprintf(snd_str, "addr:%x expexted:%x read:%x\n", dbg[i].addr, dbg[i].expected_val, dbg[i].read_val);
		console_str_send(snd_str);
	}
}

// 全リード
static void loading_app_cmd_check_all_read(int argc, char *argv[])
{
	LOADING_APP_CTL *this = &loading_app_ctl;
	uint32_t i, j, k;
	int32_t ret;
	char snd_str[256];
	
	// 評価用情報初期化
	dbg_idx = 0;
	dbg_idx_ovr_flow = 0;
	
	// Read
	ret = flash_mng_read(FLASH_MNG_KIND_W25Q20EW, 0, dbg_buf, this->buf_idx);
	if (ret != E_OK) {
		console_str_send("flash_mng_read failed\n");
		return;
	}
	
	// 表示
	console_str_send("==================== read ==================================\n");
	console_str_send("           |  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
	console_str_send("------------------------------------------------------------\n");
	for (i = 0; i < 128*1024; i++) {
		for (j = 0; j < 16; j++) {
			k = i*16+j;
			// 最後まで読んだら終了
			if (this->buf_idx <= k) {
				goto EXIT;
			}
			// アドレス表示
			if (j == 0) {
				console_str_send("0x");
				console_val_send_hex_ex(k, 8);
				console_str_send(" |");
			}
			// 読んだ値を表示
			console_str_send(" ");
			console_val_send_hex_ex(dbg_buf[k], 2);
			// 間違っていたらその情報を保存
			if (this->load_program_buf[k] != dbg_buf[k]) {
				if (dbg_idx < DEBUG_INFO_SIZE) {
					dbg[dbg_idx].addr = k;
					dbg[dbg_idx].expected_val = this->load_program_buf[k];
					dbg[dbg_idx].read_val = dbg_buf[k];
					dbg_idx++;
				} else {
					dbg_idx_ovr_flow = 1;
				}
			}
		}
		console_str_send("\n");
	}
EXIT:
	console_str_send("\n");
	
	// 間違っていた情報を表示
	sprintf(snd_str, "dbg_idx:%d dbg_idx_ovr_flow:%d\n", dbg_idx, dbg_idx_ovr_flow);
	console_str_send(snd_str);
	for (i = 0; i < dbg_idx; i++) {
		sprintf(snd_str, "addr:%x expexted:%x read:%x\n", dbg[i].addr, dbg[i].expected_val, dbg[i].read_val);
		console_str_send(snd_str);
	}
	
	return;
}

// 1byteずつリード
static void loading_app_cmd_check(int argc, char *argv[])
{
	LOADING_APP_CTL *this = &loading_app_ctl;
	uint32_t i, j, k;
	uint8_t data;
	int32_t ret;
	uint32_t last_idx;
	char snd_str[256];
	
	// 評価用情報初期化
	dbg_idx = 0;
	dbg_idx_ovr_flow = 0;
	
	// 表示
	console_str_send("==================== read ==================================\n");
	console_str_send("           |  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
	console_str_send("------------------------------------------------------------\n");
	for (i = 0; i < 128*1024; i++) {
		for (j = 0; j < 16; j++) {
			k = i*16+j;
			// 最後まで読んだら終了
			if (this->buf_idx <= k) {
				goto EXIT;
			}
			// アドレス表示
			if (j == 0) {
				console_str_send("0x");
				console_val_send_hex_ex(k, 8);
				console_str_send(" |");
			}
			// 1byteリードして表示
			ret = flash_mng_read(FLASH_MNG_KIND_W25Q20EW, k, &data, 1);
			if (ret != E_OK) {
				console_str_send("flash_mng_read failed\n");
				goto EXIT;
			}
			console_str_send(" ");
			console_val_send_hex_ex(data, 2);
			// 間違っていたらその情報を保存
			if (this->load_program_buf[k] != data) {
				if (dbg_idx < DEBUG_INFO_SIZE) {
					dbg[dbg_idx].addr = k;
					dbg[dbg_idx].expected_val = this->load_program_buf[k];
					dbg[dbg_idx].read_val = data;
					dbg_idx++;
				} else {
					dbg_idx_ovr_flow = 1;
				}
			}
		}
		console_str_send("\n");
	}
EXIT:
	console_str_send("\n");
	
	// 間違っていた情報を表示
	console_str_send("==================== result =================================\n");
	sprintf(snd_str, "dbg_idx:%d dbg_idx_ovr_flow:%d\n", dbg_idx, dbg_idx_ovr_flow);
	console_str_send(snd_str);
	for (i = 0; i < dbg_idx; i++) {
		sprintf(snd_str, "addr:%x expexted:%x read:%x\n", dbg[i].addr, dbg[i].expected_val, dbg[i].read_val);
		console_str_send(snd_str);
	}
	
	return;
}

// コマンド設定関数
void loading_app_set_cmd(void)
{
	COMMAND_INFO cmd;
	
	// コマンドの設定
	cmd.input = "loading_app write program";
	cmd.func = loading_app_cmd_write_program;
	console_set_command(&cmd);
	cmd.input = "loading_app xip";
	cmd.func = loading_app_cmd_xip;
	console_set_command(&cmd);
	cmd.input = "loading_app clear";
	cmd.func = loading_app_cmd_clear;
	console_set_command(&cmd);
	cmd.input = "loading_app check_mem_mapped";
	cmd.func = loading_app_cmd_check_mem_mapped;
	console_set_command(&cmd);
	cmd.input = "loading_app check_all_read";
	cmd.func = loading_app_cmd_check_all_read;
	console_set_command(&cmd);
	cmd.input = "loading_app check";
	cmd.func = loading_app_cmd_check;
	console_set_command(&cmd);
}
