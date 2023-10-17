/*
 * btn_dev.c
 *
 *  Created on: 2022/10/29
 *      Author: User
 */
/*
	今回は、Bluetooth通信のデバイスとしてHC-06を使用する。
	
	HC-06との接続状態は、HC-06のSTATEpinで確認する。
	接続した場合は、常にhigh(3.3V)、接続していない場合は、100ms間隔でhigh、lowを繰り返す。
	接続していない場合は、100ms周期でhigh、lowを繰り返すため、150ms間同じ値になることはない。
	このため、50ms周期で3回連続同じ値であった場合、接続されたとみなす。
*/
#include "defines.h"
#include "kozos.h"
#include "bt_dev.h"
#include "console.h"
#include "ctl_main.h"
#include "intr.h"
#include "tim.h"
#include <string.h>

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
#define BT_BAUDRATE_9600			(9600)										// 通信速度 9600bps
#define BT_BAUDRATE_115200			(115200)									// 通信速度 115200bps
#define BT_USART_CH					USART_CH2									// 使用するUSARTのチャネル
#define BT_BUF_NUM					(512)										// バッファサイズ[byte]
#define BT_STATUS_CHECK_PERIOD		(10)										// デバイスの接続状態をチェックする周期 (ms)
#define BT_STATUS_CHECK_CHATTER_NUM	(16)										// チャタリング
#define BT_CONNECTED				((1 << BT_STATUS_CHECK_CHATTER_NUM) - 1)	// 接続された
#define BT_TIM_CH					TIM_CH2										// 使用するTIMのチャネル

// バッファ情報
typedef struct {
	uint8_t data[BT_BUF_NUM];
	uint8_t size;
} BUF_INFO;

// 制御用ブロック
typedef struct {
	uint32_t			state;					// 状態
	uint32_t			baudrate;				// ボーレート
	kz_thread_id_t		tsk_rcv_id;				// BlueToot受信タスクID
	kz_thread_id_t		tsk_sts_id;				// BlueToot状態管理タスクID
	kz_thread_id_t		tsk_con_id;				// 接続状態状態管理タスクID
	kz_msgbox_id_t		msg_id;					// メッセージID
	//- BUF_INFO			rcv_buf;				// 受信バッファ
	BUF_INFO			snd_buf;				// 送信バッファ
	uint32_t			con_sts_bmp;			// 接続状態
	uint8_t				check_sts_cnt;			// 接続状態確認カウント
	BT_RCV_CALLBACK		callback;				// コールバック
	void				*callback_vp;			// コールバックパラメータ
} BT_CTL;
static BT_CTL bt_ctl;

// 送信情報
typedef struct {
	BT_SEND_TYPE type;
	uint8_t      *data;
	uint8_t      size;
}BT_SEND_INFO;

// メッセージ
typedef struct {
	uint32_t     msg_type;
	BT_SEND_INFO msg_data;
}BT_MSG;

// 送信関数プロトタイプ
static int32_t send_data(uint8_t *data, uint8_t size);
static int32_t cmd_check(uint8_t *data, uint8_t size);
static int32_t cmd_version(uint8_t *data, uint8_t size);
static int32_t cmd_name(uint8_t *data, uint8_t size);
static int32_t cmd_baudrate(uint8_t *data, uint8_t size);
static int32_t cmd_pin(uint8_t *data, uint8_t size);

// 送信関数テーブル
typedef int32_t (*SEND_FUNC)(uint8_t *data, uint8_t size);
static SEND_FUNC snd_func[SEND_TYPE_MAX] = 
{
	send_data,
	cmd_check,
	cmd_version,
	cmd_name,
	cmd_baudrate,
	cmd_pin,
};

// TIM設定テーブル
static const TIM_OPEN tim_open_par = {
	TIM_MODE_INPUT_CAPTURE,
};

// デバッグ用
typedef struct {
	uint32_t	baudrate;	// ボーレート
	char*		str;		// 表示文字列
}BAUDRATE_STR;
static const BAUDRATE_STR baudrate_str[BT_BAUDRATE_TYPE_MAX] =
{
	{BT_BAUDRATE_9600,   "baudrate = 9600\n"},
	{BT_BAUDRATE_115200, "baudrate = 11520\n"},
};

// 接続通知ハンドラ
static void bt_dev_msg_connected(void* par) 
{
	console_str_send((uint8_t*)"bluetooth device is connected\n");
}

// 非接続通知ハンドラ
static void bt_dev_msg_unconnected(void* par) 
{
	BT_CTL *this = &bt_ctl;
	
	console_str_send((uint8_t*)"bluetooth device is unconnected\n");
	
	// 接続が切れた場合は、送信バッファをクリアする
	this->snd_buf.size = 0;
}

// 接続通知関数
static void bt_dev_connected(void) 
{
	BT_CTL *this = &bt_ctl;
	BT_MSG *msg;
	
	msg = kz_kmalloc(sizeof(BT_MSG));
	msg->msg_type = EVENT_CONNECT;
	memset(&(msg->msg_data), 0, sizeof(BT_SEND_INFO));
	
	kz_send(this->msg_id, sizeof(BT_MSG), msg);
}

// 非接続通知関数
static void bt_dev_unconnected(void) 
{
	BT_CTL *this = &bt_ctl;
	BT_MSG *msg;
	
	msg = kz_kmalloc(sizeof(BT_MSG));
	msg->msg_type = EVENT_DISCONNECT;
	memset(&(msg->msg_data), 0, sizeof(BT_SEND_INFO));
	
	kz_send(this->msg_id, sizeof(BT_MSG), msg);
}

// 状態チェック関数
static void bt_dev_msg_check_sts(int argc, char *argv[])
{
	BT_CTL *this = &bt_ctl;
	GPIO_PinState status = 0U;
	
	// 接続状態を確認
	status = HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_3);
	
	// 接続状態を覚えておく
	this->con_sts_bmp |= (status << this->check_sts_cnt);
	
	// 接続状態の取得回数をインクリメント
	this->check_sts_cnt++;
	
	if (this->check_sts_cnt < BT_STATUS_CHECK_CHATTER_NUM) {
		return;
	}
	
	// 接続状態の取得回数をクリア
	this->check_sts_cnt = 0;
	
	// 接続されている？
	if (this->con_sts_bmp == BT_CONNECTED) {
		// 接続状態でない？
		if (this->state != ST_CONNECTED) {
			// 接続したことを通知
			bt_dev_connected();
		}
	// 接続されていない？
	} else {
		// 非接続状態でない？
		if (this->state != ST_DISCONNECTED) {
			// 接続が切れたことを通知
			bt_dev_unconnected();
		}
	}
	
	// 接続状態をクリア
	this->con_sts_bmp = 0U;

}

// データを送信
static int32_t send_data(uint8_t *data, uint8_t size)
{
	int32_t ret;
	
	// 送信
	ret = usart_send(BT_USART_CH, data, size);
	
	return ret;
}

// 接続確認
static int32_t cmd_check(uint8_t *data, uint8_t size)
{
	int32_t ret;
	char *cmd = "AT";
	
	// 送信
	ret = usart_send(BT_USART_CH, (uint8_t*)cmd, 2);
	
	return ret;
}

// バージョン確認
static int32_t cmd_version(uint8_t *data, uint8_t size)
{
	int32_t ret;
	char *cmd = "AT+VERSION";
	
	// 送信
	ret = usart_send(BT_USART_CH, (uint8_t*)cmd, 10);
	
	return ret;
}

// 名前変更
static int32_t cmd_name(uint8_t *data, uint8_t size)
{
	int32_t ret;
	char *cmd = "AT+NAME";
	
	// 送信
	ret = usart_send(BT_USART_CH, (uint8_t*)cmd, 7);
	if (ret != -1) {
		ret = usart_send(BT_USART_CH, data, size);
	}
	
	return ret;
}

// ボーレート変更
static int32_t cmd_baudrate(uint8_t *data, uint8_t size)
{
	int32_t ret;
	char cmd[8];
	
	// siziが1でない場合はエラーを返して終了
	if (size != 1) {
		return -1;
	}
	
	// ボーレートが範囲外の場合はエラーを返して終了
	if ((*data >= BT_BAUDRATE_TYPE_MAX)||(*data <= BT_BAUDRATE_TYPE_NOT_USED)) {
		return -1;
	}
	
	// コマンドをコピー
	memcpy(cmd, "AT+BAUD", 7);
	
	// ボーレートをコピー
	cmd[7] = *data + 0x30;
	
	// 送信
	ret = usart_send(BT_USART_CH, (uint8_t*)cmd, 8);
	
	return ret;
}

// PIN変更
static int32_t cmd_pin(uint8_t *data, uint8_t size)
{
	int32_t ret;
	char *cmd = "AT+PIN";
	
	// siziが4でない場合はエラーを返して終了
	if (size != 4) {
		return -1;
	}
	
	// 送信
	ret = usart_send(BT_USART_CH, (uint8_t*)cmd, 6);
	if (ret != -1) {
		ret = usart_send(BT_USART_CH, data, size);
	}
	
	return ret;
}

// 送信通知ハンドラ
static void bt_dev_msg_send(void *par) 
{
	BT_SEND_INFO *send_info = (BT_SEND_INFO*)par;
	BT_CTL *this = &bt_ctl;
	int32_t ret;
	
	// 送信
	ret = snd_func[send_info->type](send_info->data, send_info->size);
	if (ret != 0) {
		console_str_send((uint8_t*)"send failed\n");
	}
	
	// サイズを0に更新
	this->snd_buf.size = 0;
}

// BlueToothからのコマンドのレスポンスを解析する関数
static int32_t bt_dev_analysis_cmd_res(uint8_t data)
{
	/*
		HC-06メモ
		送信コマンド	受信データ
		AT				OK
		AT+VERSION?		HC-06-[Version]
		AT+NAME[名前]	OKsetname
		AT+PIN[XXXX]	OKsetPIN
		AT+BAUD[数字]	OK[数値]
	*/
	return 0;
}

// コマンド
// 接続確認
static void bt_dev_cmd_connect_check(void)
{
	uint8_t dmy_data;
	uint8_t dmy_size = 1;
	// 送信
	bt_dev_send(SEND_TYPE_CMD_CHECK, &dmy_data, dmy_size);
}

// バージョン確認
static void bt_dev_cmd_version(void)
{
	uint8_t dmy_data;
	uint8_t dmy_size = 1;
	// 送信
	bt_dev_send(SEND_TYPE_CMD_VERSION, &dmy_data, dmy_size);
}

// 名前変更
static void bt_dev_cmd_name(void)
{
	uint8_t *name = "mito";
	uint8_t size = 4;
	// 送信
	bt_dev_send(SEND_TYPE_CMD_NAME, name, size);
}

// ボーレート変更 115200
static void bt_dev_cmd_baudrate_115200(void)
{
	uint8_t baudrate = BT_BAUDRATE_TYPE_115200;
	// 送信
	bt_dev_send(SEND_TYPE_CMD_BAUD, &baudrate, 1);
}

// ボーレート変更 9600
static void bt_dev_cmd_baudrate_9600(void)
{
	uint8_t baudrate = BT_BAUDRATE_TYPE_9600;
	// 送信
	bt_dev_send(SEND_TYPE_CMD_BAUD, &baudrate, 1);
}

// マイコン側のボーレート変更 
static void bt_dev_cmd_change_baudrate(void)
{
	BT_CTL *this = &bt_ctl;
	int32_t ret;
	uint8_t i;
	
	// usartクローズ
	ret = usart_close(BT_USART_CH);
	// usartをクローズできなかったらエラーを返して終了
	if (ret != 0) {
		console_str_send((uint8_t*)"uart close failed!\n");
		return -1;
	}
	console_str_send((uint8_t*)"uart closed!\n");
	
	// ボーレート設定
	// 現在のボーレートが9600?
	if (this->baudrate == BT_BAUDRATE_9600) {
		// ボーレートを115200に設定
		this->baudrate = BT_BAUDRATE_115200;
	// 現在のボーレートが115200?
	} else {
		// ボーレートを9600に設定
		this->baudrate = BT_BAUDRATE_9600;
	}
	
	// usart再オープン
	ret = usart_open(BT_USART_CH, this->baudrate);
	// usartをオープンできなかったらエラーを返して終了
	if (ret != 0) {
		console_str_send((uint8_t*)"uart reopen failed!\n");
		return -1;
	}
	console_str_send((uint8_t*)"uart reopend!\n");
	for (i = 0; i < sizeof(baudrate_str)/sizeof(baudrate_str[0]); i++) {
		if (this->baudrate == baudrate_str[i].baudrate) {
			console_str_send((uint8_t*)baudrate_str[i].str);
			break;
		}
	}
}

// インプットキャプチャのコールバック
static void bt_dev_input_capture_callback(TIM_CH ch, void *par, uint32_t cnt)
{
	// 未使用
}

// 状態遷移テーブル
static const FSM fsm[ST_MAX][EVENT_MAX] = {
	// EVENT_CONNECT						EVENT_DISCONNECT								EVENT_SEND								EVENT_CHECK_STS
	{{NULL,					ST_UNDEIFNED},	{NULL,						ST_UNDEIFNED},		{NULL,				ST_UNDEIFNED},		{NULL,					ST_UNDEIFNED}},		// ST_UNINITIALIZED
	{{bt_dev_msg_connected,	ST_CONNECTED},	{bt_dev_msg_unconnected,	ST_DISCONNECTED},	{NULL,				ST_UNDEIFNED},		{bt_dev_msg_check_sts,	ST_UNDEIFNED}},		// ST_INITIALIZED
	{{NULL,					ST_UNDEIFNED},	{bt_dev_msg_unconnected,	ST_DISCONNECTED},	{bt_dev_msg_send,	ST_UNDEIFNED},		{bt_dev_msg_check_sts,	ST_UNDEIFNED}},		// ST_CONNECTED
	{{bt_dev_msg_connected,	ST_CONNECTED},	{NULL,						ST_UNDEIFNED},		{bt_dev_msg_send,	ST_UNDEIFNED},		{bt_dev_msg_check_sts,	ST_UNDEIFNED}},		// ST_DISCONNECTED
	{{NULL,					ST_UNDEIFNED},	{NULL,						ST_UNDEIFNED},		{NULL,				ST_UNDEIFNED},		{NULL,					ST_UNDEIFNED}},		// ST_UNDEIFNED
};

// BlueTooth受信タスク
static int bt_dev_rcv_main(int argc, char *argv[])
{
	BT_CTL *this = &bt_ctl;
	uint8_t data;
	int32_t size;
	
	while (1) {
		// 要求する受信サイズ分、受信するまで待つ
		size = usart_recv(BT_USART_CH, &data, 1);
		// 期待したサイズ読めた？
		if (size != 1) {
			; // 特に何もしない たぶん期待したサイズ読めるでしょう
		}
		// 接続されている場合
		if (this->state == ST_CONNECTED) {
#if 0
			// 受信データを格納
			this->rcv_buf.data[this->rcv_buf.size++] = data;;
			// 指定されたサイズ受信した場合
			if ((this->callback_notice_size <= this->rcv_buf.size) && (this->callback != NULL)) {
				// コールバック
				this->callback(this->rcv_buf.data, this->rcv_buf.size);
			}
#endif
			//console_str_send(&data);
			// コールバック
			if (this->callback != NULL) {
				this->callback(data, this->callback_vp);
			}
		// 接続されていない場合
		} else {
			// 接続されていない場合は、ATコマンドのレスポンスしか受信しない
			// このレスポンスはアプリに通知しても意味ないので、とりあえずコンソールに出しておく
			console_str_send(&data);
		}
	}
	
	return 0;
}

// BlueToothデバイス状態管理タスク
static int bt_dev_sts_main(int argc, char *argv[])
{
	BT_CTL *this = &bt_ctl;
	BT_MSG *msg;
	BT_MSG tmp_msg;
	int32_t size;
	int32_t addr;
	volatile uint32_t a = 0;
	
	while(1){
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);
		// いったんメッセージをローカル変数にコピー
		memcpy(&tmp_msg, msg, sizeof(BT_MSG));
		// メッセージを解放
		kz_kmfree(msg);
		// 処理を実行
		if (fsm[this->state][tmp_msg.msg_type].func != NULL) {
			fsm[this->state][tmp_msg.msg_type].func(&(tmp_msg.msg_data));
		}
		// 状態遷移
		if (fsm[this->state][tmp_msg.msg_type].nxt_state != ST_UNDEIFNED) {
			this->state = fsm[this->state][tmp_msg.msg_type].nxt_state;
		}
	}
	
	return 0;
}

#if 0
// 接続状態チェックタスク
int bt_dev_connect_main(int argc, char *argv[])
{
	BT_CTL *this = &bt_ctl;
	GPIO_PinState status = 0U;
	uint8_t cnt = 0U;
	
	while(1){
		// TASK_PERIODの期間スリープ
		kz_tsleep(BT_STATUS_CHECK_PERIOD);
		
		// 接続状態を確認
		status = HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_3);
		
		// 接続状態を覚えておく
		this->con_sts |= (status << cnt);
		
		// 接続状態の取得回数をインクリメント
		cnt++;
		
		if (cnt < BT_STATUS_CHECK_CHATTER_NUM) {
			continue;
		}
		
		// 接続状態の取得回数をクリア
		cnt = 0;
		
		// 接続されている？
		if (this->con_sts == BT_CONNECTED) {
			// 接続したことを通知
			bt_dev_connected();
		// 接続されていない？
		} else {
			// 接続が切れたことを通知
			bt_dev_unconnected();
		}
		
		// 接続状態をクリア
		this->con_sts = 0U;
	}
	
	return 0;
}
#endif

// BlueTooth初期化関数
int32_t bt_dev_init(void)
{
	BT_CTL *this = &bt_ctl;
	int32_t ret;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(BT_CTL));
	
	// 初期ボーレート設定
	this->baudrate = BT_BAUDRATE_115200;
	
	// usartオープン
	ret = usart_open(BT_USART_CH, this->baudrate);
	// usartをオープンできなかったらエラーを返して終了
	if (ret != 0) {
		return -1;
	}
	
	// メッセージIDの設定
	this->msg_id = MSGBOX_ID_BTMAIN;
	
	// 周期メッセージの作成
	set_cyclic_message(bt_dev_check_sts, BT_STATUS_CHECK_PERIOD);
	
	// タスクの生成
	this->tsk_rcv_id = kz_run(bt_dev_rcv_main, "bt_dev_rcv_main",  BT_DEV_PRI, BT_DEV_STACK, 0, NULL);
	this->tsk_sts_id = kz_run(bt_dev_sts_main, "bt_dev_sts_main",  BT_DEV_PRI, BT_DEV_STACK, 0, NULL);
	//this->tsk_con_id = kz_run(bt_dev_connect_main, "bt_dev_connect_main",  BT_DEV_PRI, BT_DEV_STACK, 0, NULL);
	
	// タイマの設定
	//tim_open(BT_TIM_CH, &tim_open_par, bt_dev_input_capture_callback, this);
	
	// インプットキャプチャの設定
	//tim_start_input_capture(BT_TIM_CH);
	
	// 状態の更新
	this->state = ST_INITIALIZED;
	
	return 0;
}

// コールバック登録関数
int32_t bt_dev_reg_callback(BT_RCV_CALLBACK callback, void *callback_vp)
{
	BT_CTL *this = &bt_ctl;
	
	// コールバックがすでに登録されている？
	if (this->callback != NULL) {
		return -1;
	}
	
	// コールバック登録
	this->callback = callback;
	this->callback_vp = callback_vp;
	
	return 0;
}

// BlueTooth送信通知関数
int32_t bt_dev_send(BT_SEND_TYPE type, uint8_t *data, uint8_t size)
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

// 状態チェック関数
int32_t bt_dev_check_sts(void)
{
	BT_CTL *this = &bt_ctl;
	BT_MSG *msg;
	BT_MSG *tmp_msg;
	uint32_t i;
	
	msg = kz_kmalloc(sizeof(BT_MSG));
	msg->msg_type = EVENT_CHECK_STS;
	msg->msg_data.data = NULL;
	kz_send(this->msg_id,18, msg);
}
	
// ボーレート設定関数
int32_t bt_dev_set_baudrate(BT_BAUDRATE_TYPE baudrate)
{
	BT_BAUDRATE_TYPE tmp_baudrate = baudrate;
	
	// ボーレートが範囲外の場合はエラーを返して終了
	if ((tmp_baudrate >= BT_BAUDRATE_TYPE_MAX)||(tmp_baudrate <= BT_BAUDRATE_TYPE_NOT_USED)) {
		return -1;
	}
	
	// 送信
	bt_dev_send(SEND_TYPE_CMD_BAUD, &tmp_baudrate, 1);
	
	return 0;
}

// コマンド設定関数
void bt_dev_set_cmd(void)
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
