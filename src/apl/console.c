#include <console.h>
#include <string.h>
#include <stdio.h>
#include "str_util.h"
#include "usart.h"
#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
//#include "lib.h"

#define CONOLE_BUF_SIZE		(128U)		// コマンドラインバッファサイズ
#define CONOLE_CMD_NUM		(32U)		// 設定できるコマンドの数
#define CONSOLE_USART_CH	USART_CH1	// 使用するUSARTのチャネル
#define CONSOLE_BAUDRATE	(115200)	// コンソールのボーレート
#define CONSOLE_ARG_MAX		(10)		// 引数の個数の最大値

// 制御ブロック
typedef struct {
	kz_thread_id_t		tsk_id;						// タスクID
	kz_msgbox_id_t		msg_id;						// メッセージID
	char				buf[CONOLE_BUF_SIZE];		// コマンドラインバッファ
	uint8_t				buf_idx;					// コマンドラインバッファインデックス
	COMMAND_INFO		cmd_info[CONOLE_CMD_NUM];	// コマンド関数
	uint8_t				cmd_idx;					// コマンド関数インデックス
} CONSOLE_CTL;
static CONSOLE_CTL console_ctl;


// 数値からASCIIに変換する処理
static uint8_t val2ascii(uint8_t val)
{
	if (val > 9) {
		return 0xFF;
	}
	
	return 0x30+val;
}

// コンソールからの入力を受信する関数
static uint8_t console_recv(void)
{
	uint8_t data;
	int32_t size;
	
	// 受信できるで待つ
	size = usart_recv(CONSOLE_USART_CH, &data, 1);
	// 期待したサイズ読めた？
	if (size != 1) {
		; // 特に何もしない たぶん期待したサイズ読めるでしょう
	}
	
	return data;
}

// コンソールからの入力を受信する関数
static void console_analysis(uint8_t data)
{
	CONSOLE_CTL *this = &console_ctl;
	COMMAND_INFO *cmd_info;
	uint8_t i, j;
	uint8_t argc = 0;
	char *argv[CONSOLE_ARG_MAX];
	uint8_t base_pos = 0;
	uint8_t sp_pos = 0;
	
	switch (data) {
		case '\t':	// tab
			// コマンドの一覧を表示
			console_str_send("\n");
			for (i = 0; i < this->cmd_idx; i++) {
				cmd_info = &(this->cmd_info[i]);
				console_str_send((uint8_t*)cmd_info->input);
				console_str_send("\n");
			}
			console_str_send("\n");
			break;
		case '\b':	// back space
			break;
		case '\n':	// Enter
			// NULL文字を設定
			this->buf[this->buf_idx++] = '\0';
			// コマンドに設定されている？
			for (i = 0; i < this->cmd_idx; i++) {
				cmd_info = &(this->cmd_info[i]);
				// コマンド名が一致した
				if (memcmp(this->buf, cmd_info->input, strlen(cmd_info->input)) == 0) {
					// 引数解析 (*) 先頭に空白は絶対に入れないこと！！
					for (j = 0; j < CONSOLE_ARG_MAX; j++) {
						// 空白を検索
						sp_pos = find_str(' ', &(this->buf[base_pos]));
						// 引数設定
						argv[argc++] = &(this->buf[base_pos]);
						// 空白がなかった
						if (sp_pos == 0) {
							break;
						}
						// 空白をNULL文字に設定
						this->buf[base_pos+sp_pos] = '\0';
						// 検索開始位置を更新 
						base_pos += sp_pos + 1;
					}
					// コマンド実行
					cmd_info->func(argc, argv);
					break;
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

// コンソールタスク
static int console_main(int argc, char *argv[])
{
	CONSOLE_CTL *this = &console_ctl;
	uint8_t data[2];
	uint32_t ret;
	
	while (1) {
		// "command>"を出力
		if (this->buf_idx == 0) {
			console_str_send("command>");
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

// 外部公開関数
void console_init(void)
{
	CONSOLE_CTL *this;
	
	// 制御ブロックの取得
	this = &console_ctl;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(CONSOLE_CTL));
	
	// タスクの生成
	this->tsk_id = kz_run(console_main, "console",  CONZOLE_PRI, CONSOLE_STACK, 0, NULL);
	
	// メッセージIDの設定
	this->msg_id = MSGBOX_ID_CONSOLE;

	// USARTのオープン
	usart_open(CONSOLE_USART_CH, CONSOLE_BAUDRATE);
	
	return;
}

// コンソールへ文字列を送信する関数
uint8_t console_str_send(char *data)
{
	int32_t ret;
	uint8_t len;
	
	// 文字列の長さを取得
	len = strlen((char*)data);
	
	// 送信
	ret = usart_send(CONSOLE_USART_CH, data, len);
	
	return ret;
}

// コンソールへ数値を送信する関数(8bit版)
uint8_t console_val_send(uint8_t data)
{
	int32_t ret;
	uint8_t hundreds, tens_place, ones_place;
	uint8_t snd_data[4];
	
	// 初期化
	memset(snd_data, '\0', sizeof(snd_data));
	
	// 桁数を求める
	hundreds = data/100;
	tens_place = (data - hundreds * 100)/10;
	ones_place = (data - hundreds * 100 - tens_place * 10);
	
	// 3桁？
	if (hundreds != 0) {
		snd_data[0] = val2ascii(hundreds);
		snd_data[1] = val2ascii(tens_place);
		snd_data[2] = val2ascii(ones_place);
	// 2桁?
	} else if (tens_place != 0) {
		snd_data[0] = val2ascii(tens_place);
		snd_data[1] = val2ascii(ones_place);
	// 1桁？
	} else {
		snd_data[0] = val2ascii(ones_place);
	}
	
	// 送信
	ret = console_str_send(snd_data);
	
	return ret;
}

// コンソールへ数値を送信する関数(16bit版)
uint8_t console_val_send_u16(uint16_t data)
{
	const uint8_t digit_max = 5;		// 桁数の最大値 (*) 16bitの最大値は66535で5桁
	uint8_t snd_data[6];
	uint8_t digit = 0;
	uint16_t mul = 1;
	uint16_t val = 1;
	uint8_t idx = 0;
	int8_t i;
	int32_t ret;
	
	// 初期化
	memset(snd_data, '\0', sizeof(snd_data));
	
	// 桁数取得のための値を計算
	i = digit_max - 1;
	while (i != 0) {
		mul = mul * 10;
		i--;
	}
	
	// 各数値をアスキーにする
	for (i = (digit_max - 1); i >= 0; i--) {
		// 各桁の数値を取得
		val = data / mul;
		// 桁確定
		if ((val != 0) && (digit == 0)) {
			digit = i + 1;
		}
		// 桁確定した？
		if ((digit != 0) || (i == 0)) {
			snd_data[idx] = val2ascii(val);
			data -= val * mul;
			idx++;
		}
		// 10で割る
		mul /= 10;
	}
	// 送信
	ret = console_str_send(snd_data);
	
	return ret;
}

// コンソールへ数値(16進数)を送信する関数(8bit版)
// (*) https://qiita.com/kaiyou/items/93af0fe0d49ff21fe20a
int32_t console_val_send_hex(uint8_t data, uint8_t digit)
{
	int32_t i, j;
	uint8_t temp;
	int32_t num[10];
	char answer[10];
	char snd_data[10];
	int32_t ret;
	
	if (digit == 0) {
		return -1;
	}
	
	//各桁の値を求める（num[0]が1の位）
	i = 0;
	temp = data;
	do {
		num[i] = temp % 16;
		temp = temp /16;
		i++;
	} while (temp != 0);
	
	//値が10以上になる桁をA~Fに変換する（answer[i-1]が1の位）
	for (j = 0; i > j; j++) {
		if (num[i - j - 1] > 9) {
			answer[j] = num[i - j - 1] + 'A' - 10;
		} else {
			answer[j] = '0' + num[i - j - 1];
		}
		answer[j + 1] = '\0';
	}
	
	// 桁数調整
	memset(snd_data, '0', sizeof(snd_data));
	// (*) iは2以上にならないため範囲を超えてコピーすることはない
	memcpy(&(snd_data[digit - i]), &(answer[0]), (i+1));
	
	// 送信
	ret = console_str_send(snd_data);
	
	return ret;
}

// コンソールへ数値(16進数)を送信する関数(32bit版)
int32_t console_val_send_hex_ex(uint32_t data, uint8_t digit)
{
	char tmp_data[16];
	char snd_data[8+1];
	int len;
	int8_t sft_size;
	int32_t ret;
	
	// 8より大きい桁は意味わからん
	if (digit > 8) {
		return -1;
	}
	
	// 初期化
	memset(snd_data, '0', sizeof(snd_data));
	
	// 16進数に変換
	len = sprintf(tmp_data, "%x", data);
	
	// シフトする数を計算
	sft_size = digit - len;
	
	// 送信データを作成
	if (sft_size >= 0) {
		memcpy(&(snd_data[sft_size]), tmp_data, len);
		snd_data[digit] = '\0';
	} else {
		memcpy(snd_data, tmp_data, len);
		snd_data[len] = '\0';
	}
	
	// 送信
	ret = console_str_send(snd_data);
	
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

// 割込みで文字列を出す関数
void console_output_for_int(char *str)
{
	uint8_t len;
	
	// 文字列の長さを取得
	len = strlen((char*)str);
	
	// 送信
	usart_send_for_int(CONSOLE_USART_CH, str, len);
}
