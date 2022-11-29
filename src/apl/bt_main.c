/*
 * BT_main.c
 *
 *  Created on: 2022/09/18
 *      Author: User
 */
#include "bt_main.h"
#include "defines.h"
#include "kozos.h"
#include "consdrv.h"
#include "lib.h"
#include "bt_dev.h"

// マクロ定義
#define BT_NUM   (1U)
#define BUF_SIZE (32U)

// 状態
enum State_Type {
	ST_UNINITIALIZED = 0U,
	ST_INITIALIZED,
	ST_CONNECTED,
	ST_DISCONNECTED,
	ST_UNDEIFNED,
	ST_MAX,
};

// イベント
enum EVENT_Type {
	EVENT_INIT = 0U,
	EVENT_CONNECT,
	EVENT_RUN,
	EVENT_DISCONNECT,
	EVENT_GET_STATUS,
	EVENT_MAX,
};

// 関数ポインタ
typedef int (*FUNC)(void);

// 状態遷移用定義
typedef struct {
	FUNC func;
	uint8_t nxt_state;
}FSM;

// 状態遷移用定義
typedef struct {
	uint8_t data[BUF_SIZE];
	uint8_t rd_idx;
	uint8_t wr_idx;
}BUF;

// 制御用ブロック
typedef struct {
	BUF primary;   // データ格納用バッファ
	BUF secondary; // データ格納用バッファ
}TRANSFER_CTL;
typedef struct {
	kz_thread_id_t tsk_id;      // メインタスクのID
	kz_thread_id_t tsk_data_id; // データ取得用タスクのID
	kz_msgbox_id_t msg_id;      // メッセージID
	uint8_t state;              // 状態
	TRANSFER_CTL trans_ctl;
} BT_CTL;
BT_CTL bt_ctl[BT_NUM];

// プロトタイプ宣言
static void bt_init();
static void bt_send_data();
static void bt_get_status();

// 状態遷移テーブル
static const FSM fsm[ST_MAX][EVENT_MAX] = {
		// EVENT_INIT               EVENT_CONNECT         EVENT_RUN                      EVENT_DISCONNECT         EVENT_GET_STATUS
		{{bt_init, ST_INITIALIZED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},          {NULL, ST_UNDEIFNED},    {bt_get_status, ST_UNINITIALIZED}, }, // ST_UNINITIALIZED
		{{NULL, ST_UNDEIFNED},      {NULL, ST_CONNECTED}, {bt_send_data, ST_UNDEIFNED},  {NULL, ST_DISCONNECTED}, {bt_get_status, ST_INITIALIZED}, },   // ST_INITIALIZED
		{{NULL, ST_UNDEIFNED},      {NULL, ST_CONNECTED}, {bt_send_data, ST_CONNECTED},  {NULL, ST_DISCONNECTED}, {bt_get_status, ST_CONNECTED}, },     // ST_CONNECTED
		{{NULL, ST_UNDEIFNED},      {NULL, ST_CONNECTED}, {NULL, ST_UNDEIFNED},          {NULL, ST_DISCONNECTED}, {bt_get_status, ST_DISCONNECTED}, },  // ST_DISCONNECTED
};

int BT_main(int argc, char *argv[])
{
	BT_CTL *this = &bt_ctl[0];
	uint8_t cur_state = ST_UNINITIALIZED;
	uint8_t nxt_state = cur_state;
	BT_MSG *msg;
	int32_t size;
	FUNC func;

	// 制御ブロックの初期化
	memset(bt_ctl, 0, sizeof(bt_ctl));

	this->tsk_id = kz_getid();
	this->msg_id = MSGBOX_ID_BTMAIN;

	while(1){
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);

		// 処理/次状態を取得
		func = fsm[cur_state][msg->msg_type].func;
		nxt_state = fsm[cur_state][msg->msg_type].nxt_state;

		// メッセージを解放
		kz_kmfree(msg);

		this->state = nxt_state;

		// 処理を実行
		if (func != NULL) {
			func();
		}

		// 状態遷移
		if (nxt_state != ST_UNDEIFNED) {
			cur_state = nxt_state;
		}
	}

	return 0;
}

// 外部公開関数
/* 初期化メッセージを送信する関数 */
void BT_MSG_init(void)
{
	BT_CTL *this = &bt_ctl[0];
	BT_MSG *msg;

	msg = kz_kmalloc(sizeof(BT_MSG));

	msg->msg_type = EVENT_INIT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(BT_MSG), msg);

	return;
}

void BT_resp_callback(uint8_t data)
{

}

// 接続済みメッセージを送信する関数
void BT_MSG_connect(void)
{
	BT_CTL *this = &bt_ctl[0];
	BT_MSG *msg;

	// 現状態がCONNECTEDの場合、イベントを発行しない
	if (this->state == ST_CONNECTED) {
		return;
	}

	msg = kz_kmalloc(sizeof(BT_MSG));

	msg->msg_type = EVENT_CONNECT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(BT_MSG), msg);

	return;
}

// データ送信要求メッセージを送信する関数
void BT_MSG_send_data(uint8_t *data, uint8_t size)
{
	BT_CTL *this = &bt_ctl[0];
	BT_MSG *msg;
	uint8_t i;

	msg = kz_kmalloc(sizeof(BT_MSG));

	// 内部バッファにコピー
	for (i = 0; i<size; i++) {
		this->trans_ctl.primary.data[this->trans_ctl.primary.wr_idx++] = data[i];
		this->trans_ctl.primary.wr_idx = this->trans_ctl.primary.wr_idx & (BUF_SIZE - 1);
	}

	msg->msg_type = EVENT_RUN;
	msg->msg_data = this->trans_ctl.primary.data;

	kz_send(this->msg_id, sizeof(BT_MSG), msg);

	return;
}

/* データ受信メッセージを送信する関数 */
void BT_MSG_receive_data(uint8_t *data, uint8_t size)
{
#if 0
	BT_CTL *this = &bt_ctl[0];
	uint8_t i;
	BT_MSG *msg;

	msg = kz_kmalloc(sizeof(BT_MSG));

	// 内部バッファにコピー
	for (i = 0; i<size; i++) {
		this->trans_ctl.rcv_data[i] = data[i];
	}

	msg->msg_type = EVENT_RUN;
	msg->msg_data = data;

	kz_send(this->msg_id, sizeof(BT_MSG), msg);
#endif
	return;
}

// 非接続メッセージを送信する関数
void BT_MSG_disconnect(void)
{
	BT_CTL *this = &bt_ctl[0];
	BT_MSG *msg;

	// 現状態がDISCONNECTEDの場合、イベントを発行しない
	if (this->state == ST_DISCONNECTED) {
		return;
	}

	msg = kz_kmalloc(sizeof(BT_MSG));

	msg->msg_type = EVENT_DISCONNECT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(BT_MSG), msg);

	return;
}

// 状態取得メッセージを送信する関数
void BT_MSG_get_status(void)
{
	BT_CTL *this = &bt_ctl[0];
	BT_MSG *msg;

	msg = kz_kmalloc(sizeof(BT_MSG));

	msg->msg_type = EVENT_GET_STATUS;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(BT_MSG), msg);

	return;
}

// 内部関数
// 初期化要求メッセージ受信時に実行する関数
static void bt_init(void)
{
	// BlueToothの初期化
	bt_dev_send_use(SERIAL_BLUETOOTH_DEVICE);

	return;
}

//　データ送信要求メッセージ受信時に実行する関数
static void bt_send_data(void)
{
	BT_CTL *this = &bt_ctl[0];
	uint8_t val[3];
	char data[3];

	// 3桁目の数字を取得
	val[2] = this->trans_ctl.primary.data[this->trans_ctl.primary.rd_idx] / 100;
	// 2桁目の数字を取得
	val[1] = (this->trans_ctl.primary.data[this->trans_ctl.primary.rd_idx] - val[2]*100) / 10 ;
	// 1桁目の数字を取得
	val[0] = (this->trans_ctl.primary.data[this->trans_ctl.primary.rd_idx] - val[2]*100 - val[1]*10);

	// バッファのリードインデックスを更新
	this->trans_ctl.primary.rd_idx++;
	this->trans_ctl.primary.rd_idx = this->trans_ctl.primary.rd_idx & (BUF_SIZE - 1);

	// アスキーコードに変換
	data[0] = val[2] + 0x30;
	data[1] = val[1] + 0x30;
	data[2] = val[0] + 0x30;

	// BlueTooth経由でPCへ送信
	bt_dev_send_write(data);

	return;
}

//　状態取得メッセージ受信時に実行する関数
void bt_get_status(void)
{
	BT_CTL *this = &bt_ctl[0];

	switch(this->state){
	case ST_UNINITIALIZED:
		consdrv_send_write("ST_UNINITIALIZED\n");
		break;
	case ST_INITIALIZED:
		consdrv_send_write("ST_INITIALIZED\n");
		break;
	case ST_CONNECTED:
		consdrv_send_write("ST_CONNECTED\n");
		break;
	case ST_DISCONNECTED:
		consdrv_send_write("ST_DISCONNECTED\n");
		break;
	case ST_UNDEIFNED:
		consdrv_send_write("ST_UNDEIFNED\n");
		break;
	}

	return;
}
