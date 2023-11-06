/*
 * gysfdmaxb.c
 *
 *  Created on: 2023/11/1
 *      Author: ronald
 */
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "intr.h"
#include "util.h"
#include "str_util.h"
#include <string.h>

#include "gysfdmaxb.h"

// 状態
#define ST_UNINITIALIZED	(0)		// 未初期化
#define ST_INITIALIZED		(1)		// 初期化
#define ST_IDLE				(2)		// アイドル
#define ST_RUNNING			(3)		// 実行中
#define ST_UNDEFINED		(4)		// 未定義
#define ST_MAX				(5)

// イベント
#define EVENT_SEND			(0)		// 送信
#define EVENT_MAX			(1)

// マクロ
#define GYSFDMAXB_USE_UART_CH			USART_CH3		// 使用するUSARTのチャネル
#define DATA_SIZE_NONE					(0x00)
#define DATA_SIZE_VALIABLE				(0xFF)

// デバイス依存マクロ
#define GYSFDMAXB_SENTENCE_MAX			(96)
#define GYSFDMAXB_PACKET_TALKER_ID		"PMTK"

// 送信マクロ
#define GYSFDMAXB_SEND_PACKET_TEST				(0)		// テスト
#define GYSFDMAXB_SEND_PACKET_HOTSTART			(1)		// ホットスタート
#define GYSFDMAXB_SEND_PACKET_WARMSTART			(2)		// ウォームスタート
#define GYSFDMAXB_SEND_PACKET_COLDSTART			(3)		// コールドスタート
#define GYSFDMAXB_SEND_PACKET_FULL_COLDSTART	(4)		// フルコールドスタート
#define GYSFDMAXB_SEND_PACKET_CLEAR_FLASH_AID	(5)		// フラッシュクリア
#define GYSFDMAXB_SEND_PACKET_STANDBY_MODE		(6)		// スタンバイモード
#define GYSFDMAXB_SEND_PACKET_BAUDRATE			(7)		// ボーレート変更
#define GYSFDMAXB_SEND_PACKET_INTERVAL			(8)		// 出力間隔変更
#define GYSFDMAXB_SEND_PACKET_MAX				(9)

// 解析結果
#define DATA_ANALYZING				(0)		// 解析中
#define DATA_ANALYZE_OK				(1)		// 解析OK
#define DATA_ANALYZE_NG				(2)		// 解析NG

// パケット情報取得マクロ
#define get_packet_type(a)	((a >> 16) & 0x0000FFFF)
#define get_packet_val(a)	((a >> 0) & 0x0000FFFF)

// バッファ
typedef struct {
	uint8_t data[GYSFDMAXB_SENTENCE_MAX];
	uint8_t idx;
} BUFF;

// 制御用ブロック
typedef struct {
	uint32_t			state;			// 状態
	kz_thread_id_t		tsk_rcv_id;		// 受信タスクID
	kz_thread_id_t		tsk_sts_id;		// 状態管理タスクID
	kz_msgbox_id_t		msg_id;			// メッセージID
	GYSFDMAXB_CALLBACK	callback_fp;	// コールバック
	void				*callback_vp;	// コールバックパラメータ
	uint32_t			analyze_state;	// 解析状態
	BUFF				rcv_buf;		// 受信バッファ
	uint32_t			baudrate;		// ボーレート
	uint8_t				debug_flag;		// デバッグフラグ
} GYSFDMAXB_CTL;
static GYSFDMAXB_CTL gysfdmaxb_ctl;

// メッセージ
typedef struct {
	uint32_t	event;
	uint32_t	data;
} GYSFDMAXB_MSG;

// 送信パケット情報
typedef struct {
	char 		*pkt_type;
	uint8_t		data_size;
} SEND_PACKET_INFO;

// 送信パケット情報テーブル
static const SEND_PACKET_INFO send_packet_info_tbl[GYSFDMAXB_SEND_PACKET_MAX] = {
	//pkt_type	data_size
	{"000",	 	DATA_SIZE_NONE  	},	// テスト
	{"101",	 	DATA_SIZE_NONE  	},	// ホットスタート
	{"102",	 	DATA_SIZE_NONE  	},	// ウォームスタート
	{"103",	 	DATA_SIZE_NONE  	},	// コールドスタート
	{"104",	 	DATA_SIZE_NONE  	},	// フルコールドスタート
	{"120",	 	DATA_SIZE_NONE  	},	// フラッシュクリア
	{"161",	 	1					},	// スタンバイモード
	{"251",	 	DATA_SIZE_VALIABLE	},	// ボーレート変更
	{"314",	 	1					},	// 出力間隔変更
};

// 受信パケット情報
typedef uint32_t (*DATA_FUNC)(uint8_t *data);
typedef struct {
	char		*data_field;	// データフィールド
	DATA_FUNC	func;			// データ抽出関数
} RECV_PACKET_INFO;

// プロトタイプ
static uint32_t rmc_func(uint8_t *data);

// 受信パケット情報テーブル
static const RECV_PACKET_INFO recv_packet_info_tbl[] = {
//	{"$GPGGA,", gga_func},
//	{"$GPGLL,", gll_func},
//	{"$GPGSA,", gsa_func},
//	{"$GPGSV,", gsv_func},
	{"$GPRMC,", rmc_func},
//	{"$GPVTG,", vtg_func},
//	{"$GPZDA,", zda_func},
};

// ボーレート情報
typedef struct {
	uint32_t 	baudrate_raw;
	char		*baudrate_ascii;
} SEND_PACKET_BAUDRATE_INFO;

static const SEND_PACKET_BAUDRATE_INFO send_packet_baudrate_tbl[GYSFDMAXB_BAUDRATE_MAX] = {
	{9600, "9600"},
	{115200, "115200"},
};

// チェックサム計算
static uint8_t calc_chksum(uint8_t *p_buff, uint32_t size)
{
	uint8_t chksum = 0;
	
	p_buff++;
	while (size--) {
		if (*p_buff == '*') {
			break;
		}
		chksum ^= *p_buff++;
	}
	
	return chksum;
}

// データセット関数
static uint8_t set_data(SEND_PACKET_INFO *info, uint8_t *p_data, uint16_t val)
{
	uint8_t len;
	
	// データがない？
	if (info->data_size == DATA_SIZE_NONE) {
		len = 0;
		
	// データが可変長？
	} else if (info->data_size == DATA_SIZE_VALIABLE) {
		// アスキー変換
		
	// データが固定長？
	} else {
		// アスキー変換
	}
	
	return len;
}

// 送信関数
// 参考URL:https://garchiving.com/gps-setting-change/
static void gysfdmaxb_msg_send(uint32_t par)
{
	uint16_t type = get_packet_type(par);
	uint16_t val = get_packet_val(par);
	SEND_PACKET_INFO *info;
	uint8_t len;
	uint8_t send_packet[GYSFDMAXB_SENTENCE_MAX];
	uint8_t idx = 0;
	
	// 念のため種別チェック
	if (type >= GYSFDMAXB_SEND_PACKET_MAX) {
		return;
	}
	
	// 送信パケットの初期化
	memset(send_packet, 0, sizeof(send_packet));
	
	// 送信パケット情報取得
	info = &(send_packet_info_tbl[type]);
	
	// NMEA Packet Formatにしたがいパケットを作成
	// NMEA Packet Format:Preamble TalkerID PacketType DataField * CHK1 CHK2 CR LF
	
	// Preamble設定
	send_packet[idx++] = '$';
	// Talker ID設定
	memcpy(&send_packet[idx], GYSFDMAXB_PACKET_TALKER_ID, sizeof(GYSFDMAXB_PACKET_TALKER_ID));
	idx += sizeof(GYSFDMAXB_PACKET_TALKER_ID);
	// Packet Type設定
	len = strlen(info->pkt_type);
	memcpy(&send_packet[idx], info->pkt_type, len);
	idx += len;
	// Data Field設定
	len = set_data(info, &send_packet[idx], val);
	idx += len;
	// '*'設定
	send_packet[idx] = '*';
	idx++;
	// チェックサム設定
	//calc_chksum()
	// CR LF設定
	send_packet[idx] = '\r';
	idx++;
	send_packet[idx] = '\n';
	idx++;
	
	// 送信
	usart_send(GYSFDMAXB_USE_UART_CH, send_packet, idx);
}

// 状態遷移テーブル
static const FSM fsm[ST_MAX][EVENT_MAX] = {
	// EVENT_SEND					
	{{NULL,					ST_UNDEFINED},},	// ST_UNINITIALIZED
	{{NULL,					ST_UNDEFINED},},	// ST_INITIALIZED
	{{gysfdmaxb_msg_send,	ST_RUNNING  },},	// ST_IDLE
	{{NULL,					ST_UNDEFINED},},	// ST_RUNNING
};

// 状態管理タスク
static int gysfdmaxb_stm_main(int argc, char *argv[])
{
	GYSFDMAXB_CTL *this = &gysfdmaxb_ctl;
	GYSFDMAXB_MSG *msg;
	GYSFDMAXB_MSG tmp_msg;
	int32_t size;
	
	while(1){
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);
		// いったんメッセージをローカル変数にコピー
		memcpy(&tmp_msg, msg, sizeof(GYSFDMAXB_MSG));
		// メッセージを解放
		kz_kmfree(msg);
		// 処理を実行
		if (fsm[this->state][tmp_msg.event].func != NULL) {
			fsm[this->state][tmp_msg.event].func(tmp_msg.data);
		}
		// 状態遷移
		if (fsm[this->state][tmp_msg.event].nxt_state != ST_UNDEFINED) {
			this->state = fsm[this->state][tmp_msg.event].nxt_state;
		}
	}
	
	return 0;
}

// RMCデータ抽出関数
static uint32_t rmc_func(uint8_t *data)
{
	GYSFDMAXB_CTL *this = &gysfdmaxb_ctl;
	GYSFDMAXB_DATA callback_data;
	uint8_t *base = data;
	uint8_t comma_pos;
	uint32_t ret;
	uint8_t i;
	
	// RMCフォーマット
	// $GPRMC,135559.000,A,3540.5648,N,13944.6910,E,0.23,126.59,091121,,,A*6D
	
	// コンマの位置を取得
	comma_pos = find_str(',', base);
	if (comma_pos == 0) {
		ret = DATA_ANALYZE_NG;
		goto EXIT_RMC_FUNC;
	}
	// 時刻を取得
	callback_data.hour = ascii2num(base[0])*10+ascii2num(base[1]); 
	callback_data.minute = ascii2num(base[2])*10+ascii2num(base[3]); 
	
	// コンマの位置を取得
	base += comma_pos + 1;
	comma_pos = find_str(',', base);
	if (comma_pos == 0) {
		ret = DATA_ANALYZE_NG;
		goto EXIT_RMC_FUNC;
	}
	
	// 測位状態を取得
	callback_data.status = base[0];
	
	// コンマの位置を取得
	base += comma_pos + 1;
	comma_pos = find_str(',', base);
	if (comma_pos == 0) {
		ret = DATA_ANALYZE_NG;
		goto EXIT_RMC_FUNC;
	}
	
	// 緯度を取得 (*) 南緯は考えない
	callback_data.latitude = ascii2num(base[0])*10+ascii2num(base[1]);
	
	for (i = 0; i < 2; i++) {
		// コンマの位置を取得
		base += comma_pos + 1;
		comma_pos = find_str(',', base);
		if (comma_pos == 0) {
			ret = DATA_ANALYZE_NG;
			goto EXIT_RMC_FUNC;
		}
	}
	
	// 経度を取得 (*) 西経は考えない
	callback_data.longitude = ascii2num(base[0])*100+ascii2num(base[1])*10+ascii2num(base[2]);
	
	for (i = 0; i < 4; i++) {
		// コンマの位置を取得
		base += comma_pos + 1;
		comma_pos = find_str(',', base);
		if (comma_pos == 0) {
			ret = DATA_ANALYZE_NG;
			goto EXIT_RMC_FUNC;
		}
	}
	
	// 日付を取得
	callback_data.year = ascii2num(base[4])*10+ascii2num(base[5]);
	callback_data.month = ascii2num(base[2])*10+ascii2num(base[3]);
	callback_data.day = ascii2num(base[0])*10+ascii2num(base[1]);
	
	// コールバック
	if (this->callback_fp != NULL) {
		this->callback_fp(this->callback_vp, &callback_data);
	}
	
	ret = DATA_ANALYZE_OK;
	
EXIT_RMC_FUNC:
	return ret;
}

// データ解析
static uint32_t analyze_data(uint8_t data)
{
	GYSFDMAXB_CTL *this = &gysfdmaxb_ctl;
	BUFF *buf = &(this->rcv_buf);
	int32_t ret = DATA_ANALYZING;
	uint8_t ast_idx = 0;
	uint8_t chksum, rcv_chksum;
	uint8_t data_idx, data_len;
	uint8_t tmp_data[2];
	
	// データをバッファに格納
	buf->data[buf->idx] = data;
	
	// 最初のデータが$？
	if ((buf->idx == 0) && (buf->data[0] == '$')) {
		// バッファインデックス更新
		buf->idx++;
		
	// データが1以上ある？
	} else if (buf->idx > 0) {
		//  末尾がCRLF？
		if ((buf->data[buf->idx - 1] == '\r') && (buf->data[buf->idx] == '\n')) {
			// コンソール表示
			if (this->debug_flag != FALSE) {
				console_str_send((uint8_t*)buf->data);
			}
			
			// 受信対象のデータ？
			for (data_idx = 0; data_idx < sizeof(recv_packet_info_tbl)/sizeof(recv_packet_info_tbl[0]); data_idx++) {
				data_len = strlen(recv_packet_info_tbl[data_idx].data_field);
				if (memcmp(recv_packet_info_tbl[data_idx].data_field, buf->data, data_len) == 0) {
					break;
				}
			}
			// 受信対象のデータではなかった場合は解析NG
			if (data_idx == sizeof(recv_packet_info_tbl)/sizeof(recv_packet_info_tbl[0])) {
				ret = DATA_ANALYZE_NG;
				goto EXIT_ANALIZE;
			}
			
			// '*'の位置を取得
			for (ast_idx = buf->idx; ast_idx > 0; ast_idx--) {
				if (buf->data[ast_idx] == '*') {
					break;
				}
			}
			// '*'がなかった場合は解析NG
			if (ast_idx == 0) {
				ret = DATA_ANALYZE_NG;
				goto EXIT_ANALIZE;
			}
			
			// チェックサム計算
			chksum = calc_chksum(&(buf->data[0]), buf->idx);
			rcv_chksum = ascii2num(buf->data[ast_idx+1])*16+ascii2num(buf->data[ast_idx+2]);
			// チェックサムが一致しなければ解析NG
			if (chksum != rcv_chksum) {
				ret = DATA_ANALYZE_NG;
				goto EXIT_ANALIZE;
			}
			
			// データ抽出
			ret = recv_packet_info_tbl[data_idx].func(&(buf->data[data_len]));
			
EXIT_ANALIZE:
			// バッファクリア
			memset(buf, 0, sizeof(BUFF));
			
		} else {
			// バッファインデックス更新
			buf->idx++;
			
		}
	}
	
	return ret;
}

// 受信タスク
static int gysfdmaxb_rcv_main(int argc, char *argv[])
{
	GYSFDMAXB_CTL *this = &gysfdmaxb_ctl;
	uint8_t data;
	int32_t size;
	uint32_t ret;
	
	while (1) {
		// 要求する受信サイズ分、受信するまで待つ
		size = usart_recv(GYSFDMAXB_USE_UART_CH, &data, 1);
		// 期待したサイズ読めた？
		if (size != 1) {
			; // 特に何もしない たぶん期待したサイズ読めるでしょう
		}
		
		// データ解析
		ret = analyze_data(data);
		
		// 解析中
		if (ret == DATA_ANALYZING) {
			;
		// 解析OK
		} else if (ret == DATA_ANALYZE_OK) {
			;
		// 解析NG
		} else if (ret == DATA_ANALYZE_NG) {
			;
		}
		
	}
	
	return 0;
}

// 初期化関数
int32_t gysfdmaxb_init(void)
{
	GYSFDMAXB_CTL *this = &gysfdmaxb_ctl;
	int32_t ret;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(GYSFDMAXB_CTL));
	// 初期化
	this->baudrate = send_packet_baudrate_tbl[GYSFDMAXB_BAUDRATE_9600].baudrate_raw;
	
	// usartオープン
	ret = usart_open(GYSFDMAXB_USE_UART_CH, this->baudrate);
	// usartをオープンできなかったらエラーを返して終了
	if (ret != 0) {
		return -1;
	}
	
	// メッセージIDの設定
	this->msg_id = MSGBOX_ID_GYSFDMAXB;
	
	// タスクの生成
	this->tsk_rcv_id = kz_run(gysfdmaxb_rcv_main, "gysfdmaxb_rcv_main",  GYSFDMAXB_PRI, GYSFDMAXB_STACK, 0, NULL);
	this->tsk_sts_id = kz_run(gysfdmaxb_stm_main, "gysfdmaxb_stm_main",  GYSFDMAXB_PRI, GYSFDMAXB_STACK, 0, NULL);
	
	// 状態の更新
	this->state = ST_INITIALIZED;
	
	return 0;
}

// コールバック登録関数
int32_t gysfdmaxb_reg_callback(GYSFDMAXB_CALLBACK callback_fp, void *callback_vp)
{
	GYSFDMAXB_CTL *this = &gysfdmaxb_ctl;
	
	// コールバックがすでに登録されている？
	if (this->callback_fp != NULL) {
		return -1;
	}
	
	// コールバック登録
	this->callback_fp = callback_fp;
	this->callback_vp = callback_vp;
	
	return 0;
}

// 通信ボーレート変更
int32_t gysfdmaxb_change_baudrate(GYSFDMAXB_BAUDRATE baudrate)
{
	GYSFDMAXB_CTL *this = &gysfdmaxb_ctl;
	GYSFDMAXB_MSG *msg;
	
	// パラメータチェック
	if (baudrate >= GYSFDMAXB_BAUDRATE_MAX) {
		return -1;
	}
	
	// メッセージ用のメモリを確保
	msg = kz_kmalloc(sizeof(GYSFDMAXB_MSG));
	
	// メッセージ設定
	msg->event = EVENT_SEND;
	msg->data = ((GYSFDMAXB_SEND_PACKET_BAUDRATE & 0x0000FFFF) << 16) | (baudrate << 0);
	
	// メッセージ送信
	kz_send(this->msg_id, sizeof(GYSFDMAXB_MSG), msg);
	
	return 0;
}

// 出力間隔変更
int32_t gysfdmaxb_change_interval(uint8_t interval)
{
	GYSFDMAXB_CTL *this = &gysfdmaxb_ctl;
	GYSFDMAXB_MSG *msg;
	
	msg = kz_kmalloc(sizeof(GYSFDMAXB_MSG));
	
	// メッセージ設定
	msg->event = EVENT_SEND;
	msg->data = ((GYSFDMAXB_SEND_PACKET_INTERVAL & 0x0000FFFF) << 16) | (interval << 0);
	
	// メッセージ送信
	kz_send(this->msg_id, sizeof(GYSFDMAXB_MSG), msg);
}

// コマンド
static void gysfdmaxb_cmd_change_baudrate_9600(void)
{
	gysfdmaxb_change_baudrate(GYSFDMAXB_BAUDRATE_9600);
}

static void gysfdmaxb_cmd_change_baudrate_115200(void)
{
	gysfdmaxb_change_baudrate(GYSFDMAXB_BAUDRATE_115200);
}

static void gysfdmaxb_cmd_debug_on(void)
{
	GYSFDMAXB_CTL *this = &gysfdmaxb_ctl;
	
	// 割込み禁止
	INTR_DISABLE;
	this->debug_flag = TRUE;
	// 割込み禁止解除
	INTR_ENABLE;
}

static void gysfdmaxb_cmd_debug_off(void)
{
	GYSFDMAXB_CTL *this = &gysfdmaxb_ctl;
	
	// 割込み禁止
	INTR_DISABLE;
	this->debug_flag = FALSE;
	// 割込み禁止解除
	INTR_ENABLE;
}

// コマンド設定関数
void gysfdmaxb_set_cmd(void)
{
	COMMAND_INFO cmd;
	
	// コマンドの設定
	cmd.input = "gysfdmaxb_change_baudrate 115200";
	cmd.func = gysfdmaxb_cmd_change_baudrate_115200;
	console_set_command(&cmd);
	cmd.input = "gysfdmaxb_change_baudrate 9600";
	cmd.func = gysfdmaxb_cmd_change_baudrate_9600;
	console_set_command(&cmd);
	cmd.input = "gysfdmaxb_cmd_debug on";
	cmd.func = gysfdmaxb_cmd_debug_on;
	console_set_command(&cmd);
	cmd.input = "gysfdmaxb_cmd_debug off";
	cmd.func = gysfdmaxb_cmd_debug_off;
	console_set_command(&cmd);
}
