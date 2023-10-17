/*
 * w25q20ew.c
 *
 *  Created on: 2023/8/29
 *      Author: User
 */
#if 0
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "intr.h"

// 状態
#define ST_UNINITIALIZED	(0)
#define ST_INITIALIZED		(1)
#define ST_CONNECTED		(2)
#define ST_DISCONNECTED		(3)
#define ST_UNDEIFNED		(4)
#define ST_MAX				(5)

// イベント
#define EVENT_CONNECT		(0)
#define EVENT_DISCONNECT	(1)
#define EVENT_SEND			(2)
#define EVENT_CHECK_STS		(3)
#define EVENT_MAX			(4)

// マクロ
#define W25Q20EW_OCTOSPI_CH			OCTOSPI_CH_1								// 使用するUSARTのチャネル

// 制御用ブロック
typedef struct {
	uint32_t			state;					// 状態
	BT_RCV_CALLBACK		callback;				// コールバック
	void				*callback_vp;			// コールバックパラメータ
} W25Q20EW_CTL;
static W25Q20EW_CTL w25q20ew_ctl;

// BlueTooth初期化関数
int32_t w25q20ew_init(void)
{
	W25Q20EW_CTL *this = &w25q20ew_ctl;
	int32_t ret;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(W25Q20EW_CTL));
	
	// octospiオープン
	ret = octospi_open(W25Q20EW_OCTOSPI_CH, this->baudrate);
	if (ret != 0) {
		return -1;
	}
	
	// 状態の更新
	this->state = ST_INITIALIZED;
	
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

// コマンド設定関数
void w25q20ew_set_cmd(void)
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
#endif
