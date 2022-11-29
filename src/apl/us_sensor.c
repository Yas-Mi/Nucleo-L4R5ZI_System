/*
 * us_sensor.c
 *
 *  Created on: 2022/08/29
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "us_sensor.h"
//#include "ctl_main.h"
#include "consdrv.h"

// マクロ定義
#define US_SENSOR_NUM  (1U)
#define BUF_SIZE       (1U)
#define SPEED_OF_SOUND (34U)   // 34cm/ms
#define US_TSK_PERIOD  (1000U) // 1000ms

// 状態
enum State_Type {
	ST_UNINITIALIZED = 0U,
	ST_INITIALIZED,
	ST_RUN,
	ST_STOP,
	ST_UNDEIFNED,
	ST_MAX,
};
// イベント
enum EVENT_Type {
	EVENT_INIT = 0U,
	EVENT_RUN,
	EVENT_STOP,
	EVENT_MAX,
};
// 関数ポインタ
typedef int (*FUNC)(void);

// 状態遷移用定義
typedef struct {
	FUNC func;
	uint8_t nxt_state;
}FSM;

// 制御用ブロック
typedef struct {
	kz_thread_id_t tsk_id;      // メインタスクのID
	kz_msgbox_id_t msg_id;      // メッセージID
	uint8_t state;              // 状態
	US_CALLBACK callback;       // コールバック
	uint8_t data[BUF_SIZE];     // データ格納用バッファ(未使用)
} US_SENSOR_CTL;
US_SENSOR_CTL us_sensor_ctl[US_SENSOR_NUM];

// プロトタイプ宣言
static void us_init(void);
static void us_get_data_start(void);
static void us_get_data_stop(void);
static void us_get_data(void);
static uint8_t read_distance(void);

// 状態遷移テーブル
static const FSM fsm[ST_MAX][EVENT_MAX] = {
		// EVENT_INIT               EVENT_RUN                    EVENT_STOP                   EVENT_MAX
		{{us_init, ST_INITIALIZED}, {NULL, ST_UNDEIFNED},        {NULL, ST_UNDEIFNED},        {NULL, ST_UNDEIFNED},}, // ST_UNINITIALIZED
		{{NULL, ST_UNDEIFNED},      {us_get_data_start, ST_RUN}, {NULL, ST_UNDEIFNED},        {NULL, ST_UNDEIFNED},}, // ST_INITIALIZED
		{{NULL, ST_UNDEIFNED},      {us_get_data, ST_RUN},       {us_get_data_stop, ST_STOP}, {NULL, ST_UNDEIFNED},}, // ST_RUN
		{{NULL, ST_UNDEIFNED},      {us_get_data_start, ST_RUN}, {NULL, ST_UNDEIFNED},        {NULL, ST_UNDEIFNED},}, // ST_STOP
};

// メインのタスク
// 状態遷移テーブルに従い、処理を実行し、状態を更新する
int US_main(int argc, char *argv[])
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	uint8_t cur_state = ST_UNINITIALIZED;
	uint8_t nxt_state = cur_state;
	US_MSG *msg;
	uint32_t size;
	FUNC func;

	// 自タスクのIDを制御ブロックに設定
	this->tsk_id = kz_getid();
	// メッセージIDを制御ブロックに設定
	this->msg_id = MSGBOX_ID_US_MAIN;

	while(1){
		// メッセージ受信
		kz_recv(this->msg_id, &size, &msg);

		// 処理/次状態を取得
		func = fsm[cur_state][msg->msg_type].func;
		nxt_state = fsm[cur_state][msg->msg_type].nxt_state;

		// msgを解放
		kz_kmfree(msg);

		// 現状態を更新
		this->state = nxt_state;

		// 処理を実行
		if (func != NULL) {
			func();
		}

		// 状態を更新
		if (nxt_state != ST_UNDEIFNED) {
			cur_state = nxt_state;
		}
	}

	return 0;
}

// 外部公開関数
// 初期化要求メッセージを送信する関数
void US_MSG_init(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	US_MSG *msg;

	msg = kz_kmalloc(sizeof(US_MSG));

	msg->msg_type = EVENT_INIT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(US_MSG), msg);

	return;
}

// データ取得開始メッセージを送信する関数
void US_MSG_measure_start(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	US_MSG *msg;

	msg = kz_kmalloc(sizeof(US_MSG));

	msg->msg_type = EVENT_RUN;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(US_MSG), msg);

	return;
}

/* データ取得メッセージを送信する関数 */
void US_MSG_get_data(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	US_MSG *msg;

	msg = kz_kmalloc(sizeof(US_MSG));

	msg->msg_type = EVENT_RUN;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(US_MSG), msg);

	return;
}

/* データ取得停止メッセージを送信する関数 */
void US_MSG_measure_stop(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	US_MSG *msg;

	msg = kz_kmalloc(sizeof(US_MSG));

	msg->msg_type = EVENT_STOP;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(US_MSG), msg);

	return;
}

/* 超音波センサのデータ取得時にコールするコールバックを設定する関数 */
uint32_t US_set_callback(US_CALLBACK callback)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	
	// パラメータチェック
	// callbackがNULLの場合、-1を返して終了
	if (callback == NULL) {
		return -1;
	}

	// コールバックがまだ登録されていない場合
	if (this->callback == NULL) {
		// コールバックを登録
		this->callback = callback;
	}
	
	return 0;
}

// 内部関数
// 初期化要求メッセージ受信時に実行する関数
static void us_init(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* GPIOの設定 */
	__HAL_RCC_GPIOF_CLK_ENABLE();
	HAL_PWREx_EnableVddIO2();

	/* PF14(GPIO入力) -> HC-SR04 Echo */
	/* PF15(GPIO出力) -> HC-SR04 Trig */
	/*Configure GPIO pin : PF14 */
	GPIO_InitStruct.Pin = GPIO_PIN_14;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	/*Configure GPIO pin : PF15 */
	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_15, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	return;
}

// データ取得開始メッセージ受信時に実行する関数
static void us_get_data_start(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	uint32_t ret;
	
	// システム制御機能に、1s周期でデータ取得メッセージを送信してもらうように要求
	ret = set_cyclic_message(US_MSG_get_data, US_TSK_PERIOD);
	
	return;
}

// データ取得メッセージ受信時に実行する関数
static void us_get_data(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	uint8_t data = 0;
	
	// 距離を取得
	data = read_distance();
	
	// コールバックを呼ぶ
	if (this->callback != NULL) {
		this->callback(data);
	}
	
	return;
}

// 超音波センサからデータを取得する関数
static uint8_t read_distance(void)
{
	uint8_t echo;
	uint32_t start_time_s;
	uint32_t end_time_s;
	uint32_t start_time_ms;
	uint32_t end_time_ms;
	uint32_t elapsed_time_ms;
	uint8_t distance;

	/* TrigをHigh */
	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_15, GPIO_PIN_SET);
	/* 1msのwait */
	kz_tsleep(1);
	/* TrigをLow */
	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_15, GPIO_PIN_RESET);

	/* Highの開始時間を取得 */
	do{
		/* echoを取得 */
		echo = HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_14);
		/* 開始時刻を取得 */
		start_time_ms = kz_get_time_ms();
		start_time_s = kz_get_time_s();
	}while(!echo);

	/* Highの終了時間を取得 */
	do{
		/* echoを取得 */
		echo = HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_14);
		/* 開始時刻を取得 */
		end_time_ms = kz_get_time_ms();
		end_time_s = kz_get_time_s();
	}while(echo);

	/* 開始時刻と終了時刻を計算 */
	start_time_ms = start_time_ms + start_time_s * 1000;
	end_time_ms = end_time_ms + end_time_s * 1000;

	/* 経過時間を算出 */
	elapsed_time_ms = end_time_ms - start_time_ms;

	/* 距離を算出 */
	distance = (uint8_t)((elapsed_time_ms * SPEED_OF_SOUND)/2);

	return distance;
}

// データ取得停止メッセージ受信時に実行する関数
static void us_get_data_stop(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	uint32_t ret;

	// システム制御機能に、1s周期のデータ取得メッセージの送信を停止してもらうように要求
	ret = del_cyclic_message(US_MSG_get_data);

	return;
}






