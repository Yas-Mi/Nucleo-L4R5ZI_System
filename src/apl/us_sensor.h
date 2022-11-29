/*
 * us_sensor.h
 *
 *  Created on: 2022/09/18
 *      Author: User
 */

#ifndef APL_US_SENSOR_H_
#define APL_US_SENSOR_H_
#include "defines.h"
#include "kozos.h"

// 関数ポインタ
typedef void (*US_CALLBACK)(uint32_t data);

// メッセージ定義
typedef struct {
	uint32_t msg_type;
	void     *msg_data;
}US_MSG;

// コールバック登録用の関数
uint32_t US_set_callback(US_CALLBACK callback);

// タスクにメッセージを送信するための関数
void US_MSG_init(void);
void US_MSG_measure_start(void);
void US_MSG_get_data(void);
void US_MSG_measure_stop(void);

#endif /* APL_US_SENSOR_H_ */
