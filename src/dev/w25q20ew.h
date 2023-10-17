/*
 * bt_dev.h
 *
 *  Created on: 2022/12/22
 *      Author: User
 */

#ifndef DEV_BT_DEV_H_
#define DEV_BT_DEV_H_

#include "defines.h"
#include "kozos.h"

typedef enum {
	SEND_TYPE_DATA = 0,			// データを送信
	SEND_TYPE_CMD_CHECK,		// AT 接続の確認
	SEND_TYPE_CMD_VERSION,		// AT+VERSION バージョンの確認
	SEND_TYPE_CMD_NAME,			// AT+NAME(名称) デバイス名の変更
	SEND_TYPE_CMD_BAUD,			// AT+BAUD(数値) ボーレートの変更
	SEND_TYPE_CMD_PIN,			// AT+PIN(数値) PINコードの変更
	SEND_TYPE_MAX,
} BT_SEND_TYPE;

typedef enum {
	BT_BAUDRATE_TYPE_NOT_USED = 0,
	BT_BAUDRATE_TYPE_1200,
	BT_BAUDRATE_TYPE_2400,
	BT_BAUDRATE_TYPE_4800,
	BT_BAUDRATE_TYPE_9600,
	BT_BAUDRATE_TYPE_19200,
	BT_BAUDRATE_TYPE_38400,
	BT_BAUDRATE_TYPE_57600,
	BT_BAUDRATE_TYPE_115200,
	BT_BAUDRATE_TYPE_MAX,
} BT_BAUDRATE_TYPE;

typedef void (*BT_RCV_CALLBACK)(uint8_t *data, void *vp);	// 受信コールバック

extern int32_t bt_dev_init(void);
extern int32_t bt_dev_reg_callback(BT_RCV_CALLBACK callback, void* callback_vp);
extern int32_t bt_dev_send(BT_SEND_TYPE type, uint8_t *data, uint8_t size);
extern int32_t bt_dev_check_sts(void);
extern int32_t bt_dev_set_baudrate(BT_BAUDRATE_TYPE baudrate);
extern void bt_dev_set_cmd(void);

#endif /* DEV_BT_DEV_H_ */