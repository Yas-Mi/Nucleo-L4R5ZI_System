#include <console.h>
#include "usart.h"
#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "lib.h"

#define CONOLE_BUF_SIZE		(128U)		// コマンドラインバッファサイズ
#define CONOLE_CMD_NUM		(16U)		// 設定できるコマンドの数
#define CONSOLE_USART_CH	USART_CH1	// 使用するUSARTのチャネル
#define CONSOLE_BAUDRATE	(9600)		// コンソールのボーレート

// 制御ブロック
typedef struct {
	kz_thread_id_t		tsk_id;						// コンソールタスクのID
	kz_thread_id_t		snd_tsk_id;					// 送信要求を出したタスクのID
	kz_msgbox_id_t		msg_id;						// メッセージID
	char				buf[CONOLE_BUF_SIZE];		// コマンドラインバッファ
	uint8_t				buf_idx;					// コマンドラインバッファインデックス
	COMMAND_INFO		cmd_info[CONOLE_CMD_NUM];	// コマンド関数
	uint8_t				cmd_idx;					// コマンド関数インデックス
} CONSOLE_CTL;
static CONSOLE_CTL console_ctl;

// 受信コールバック
static void console_recv_callback(USART_CH ch, void *vp)
{
	CONSOLE_CTL *this;
	
	// 制御ブロックの取得
	this = &console_ctl;
	
	// コンソールタスクを起こす
	kx_wakeup(this->tsk_id);
}

// 送信コールバック
static void console_send_callback(USART_CH ch, void *vp)
{
	// 特に何もしない
}

static void console_init(void)
{
	CONSOLE_CTL *this;
	int32_t ret;
	
	// 制御ブロックの取得
	this = &console_ctl;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(CONSOLE_CTL));
	
	// 自タスクIDの取得
	this->tsk_id = kz_getid();
	this->msg_id = MSGBOX_ID_BTMAIN;
	
	// コールバックの設定
	usart_reg_recv_callback(CONSOLE_USART_CH, console_recv_callback, this);
	usart_reg_send_callback(CONSOLE_USART_CH, console_send_callback, this);
	
	// USARTのオープン
	ret = usart_open(CONSOLE_USART_CH, CONSOLE_BAUDRATE);
	
	return;
}

// コンソールからの入力を受信する関数
static uint8_t console_recv(void)
{
	uint8_t data;
	
	// 受信できるで待つ
	while (usart_recv(CONSOLE_USART_CH, &data, 1)) {
		// 受信データがない場合は寝る
		kz_sleep();
	}
	
	return data;
}

// コンソールからの入力を受信する関数
static void console_analysis(uint8_t data)
{
	CONSOLE_CTL *this;
	COMMAND_INFO *cmd_info;
	uint8_t i;
	
	// 制御ブロックの取得
	this = &console_ctl;
	
	switch (data) {
		case '\t':	// tab
			break;
		case '\b':	// back space
			break;
		case '\n':	// Enter
			// NULL文字を設定
			this->buf[this->buf_idx++] = '\0';
			// コマンドに設定されている？
			for (i = 0; i < this->cmd_idx; i++) {
				cmd_info = &(this->cmd_info[i]);
				// コマンド実行
				if (strcmp(this->buf, cmd_info->input)) {
					cmd_info->func();
				}
			}
			// その他あればここで処理する
			// コマンドラインバッファインデックスをクリア
			this->buf_idx = 0;
			break;
		default:
			// データをバッファを格納
			this->buf[this->buf_idx++] = data;
			break;
	}
	
	return;
}

// 外部公開関数
// コンソールタスク
int console_main(int argc, char *argv[])
{
	CONSOLE_CTL *this;
	uint8_t data[2];
	uint32_t ret;
	
	// 制御ブロックの取得
	this = &console_ctl;
	
	// コンソールタスクの初期化
	console_init();
	
	while (1) {
		// "command>"を出力
		if (this->buf_idx == 0) {
			console_str_send((uint8_t*)"command>");
		}
		// コンソールからの入力を受信する
		data[0] = console_recv();
		data[1] = '\0';
		// 改行コード変換(\r→\n)
		if (data[0] == '\r') data[0] = '\n';
		// エコーバック
		ret = console_str_send((uint8_t*)data);
		// 受信データ解析
		console_analysis(data[0]);
	}
	
	return 0;
}

// コンソールへ文字列を送信する関数
uint8_t console_str_send(uint8_t *data)
{
	int32_t ret;
	uint8_t len;
	
	// 文字列の長さを取得
	len = strlen((char*)data);
	
	// 送信
	ret = usart_send(CONSOLE_USART_CH, data, len);
	
	return ret;
}

// コマンドを設定する関数
int32_t console_set_command(COMMAND_INFO *cmd_info)
{
	CONSOLE_CTL *this;
	
	// コマンド関数がNULLの場合エラーを返して終了
	if (cmd_info == NULL) {
		return -1;
	}
	
	// 制御ブロックの取得
	this = &console_ctl;
	
	// もうコマンド関数を登録できない場合はエラーを返して終了
	if (this->cmd_idx >= CONOLE_CMD_NUM) {
		return -1;
	}
	
	// 登録
	this->cmd_info[this->cmd_idx].input = cmd_info->input;
	this->cmd_info[this->cmd_idx].func = cmd_info->func;
	this->cmd_idx++;
	
	return 0;
}
