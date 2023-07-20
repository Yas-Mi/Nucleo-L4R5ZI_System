/*
 * sound_app.h
 *
 *  Created on: 2023/5/8
 *      Author: User
 */

#ifndef APL_SOUND_APP_H_
#define APL_SOUND_APP_H_
#include "defines.h"
#include "kozos.h"

#define SAMPLING_FREQUENCY	(16000)		// サンプリング周波数 16kHz
#define SOUND_DATA_WIDTH	(16)		// 16bit

// タスクにメッセージを送信するための関数
extern void sound_app_init(void);
extern int32_t sound_app_play(void);
extern void sound_app_set_cmd(void);

#endif /* APL_SOUND_APP_H_ */
