/*
 * bt_main.h
 *
 *  Created on: 2022/09/18
 *      Author: User
 */

#ifndef APL_BT_MAIN_H_
#define APL_BT_MAIN_H_
#include "defines.h"
#include "kozos.h"

typedef struct {
	uint32_t msg_type;
	void    *msg_data;
}BT_MSG;

void BT_MSG_init(void);
void BT_MSG_connect(void);
void BT_MSG_send_data(uint8_t *data, uint8_t size);
void BT_MSG_receive_data(uint8_t *data, uint8_t size);
void BT_MSG_disconnect(void);
void BT_resp_callback(uint8_t data);
#endif /* APL_BT_MAIN_H_ */
