
#ifndef CTL_MAIN_H_
#define CTL_MAIN_H_

#include "kozos.h"

// 関数ポインタ
typedef void (*CYC_MSG)(void);
typedef void (*INIT_TSK)(void);

typedef struct {
	uint32_t msg_type;
	void    *msg_data;
}CTL_MSG;

// 周期メッセージ登録用の関数
uint32_t set_cyclic_message(CYC_MSG cyclic_message, uint32_t period);
uint32_t del_cyclic_message(CYC_MSG cyclic_message);

// タスクにメッセージを送信するための関数
void CTL_MSG_init(void);
void CTL_MSG_event(void);
void CTL_MSG_btn(BTN_TYPE btn, BTN_STATUS sts);

#endif /* CTL_MAIN_H_ */