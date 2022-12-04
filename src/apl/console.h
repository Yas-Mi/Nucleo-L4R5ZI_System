#ifndef _CONSDRV_H_INCLUDED_
#define _CONSDRV_H_INCLUDED_
#include "defines.h"
#include "kozos.h"

// �R�}���h�̊֐���`
typedef void (*COMMAND)(void);

// �R�}���h�֐����
typedef struct {
	char* input;
	COMMAND func;
} COMMAND_INFO;

extern int console_main(int argc, char *argv[]);
extern uint8_t console_str_send(uint8_t *data);

#endif
