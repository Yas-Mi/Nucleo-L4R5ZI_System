
#ifndef CYC_H_
#define CYC_H_

#include "kozos.h"

// 関数ポインタ
typedef void (*CYC_MSG)(void);

// 周期メッセージ登録用の関数
uint32_t set_cyclic_message(CYC_MSG cyclic_message, uint32_t period);
uint32_t del_cyclic_message(CYC_MSG cyclic_message);

#endif /* CYC_H_ */