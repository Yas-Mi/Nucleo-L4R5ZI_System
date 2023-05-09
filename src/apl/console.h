#ifndef _CONSDRV_H_INCLUDED_
#define _CONSDRV_H_INCLUDED_
#include "defines.h"
#include "kozos.h"

// コマンドの関数定義
typedef void (*COMMAND)(void);

// コマンド関数情報
typedef struct {
	char* input;
	COMMAND func;
} COMMAND_INFO;

extern void console_init(void);
extern uint8_t console_str_send(uint8_t *data);
extern int32_t console_set_command(COMMAND_INFO *cmd_info);

#endif
