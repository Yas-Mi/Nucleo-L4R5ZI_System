#ifndef _DEFINES_H_INCLUDED_
#define _DEFINES_H_INCLUDED_

#include "usart.h"

#define NULL ((void *)0)
#define SERIAL_DEFAULT_DEVICE (USART1)
#define SERIAL_BLUETOOTH_DEVICE (USART2)

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned long  uint32;

#define TRUE	(1U)
#define FALSE	(0U)

typedef uint32 kz_thread_id_t;
typedef int (*kz_func_t)(int argc, char *argv[]);
typedef void (*kz_handler_t)(void);

typedef enum {
	MSGBOX_ID_CONSOLE = 0,
	MSGBOX_ID_BTMAIN,
	MSGBOX_ID_SOUND_APP,
	MSGBOX_ID_US_MAIN,
	MSGBOX_ID_CTL_MAIN,
	MSGBOX_ID_BTN_MAIN,
	MSGBOX_ID_LCD_APP_MAIN,
	MSGBOX_ID_GYSFDMAXB,
//  MSGBOX_ID_US_DATA,
	MSGBOX_ID_SPI1,
	MSGBOX_ID_SPI2,
	MSGBOX_ID_SPI3,
	MSGBOX_ID_NUM
} kz_msgbox_id_t;

// タスクの優先度
#define BT_DEV_PRI		(8)
#define CONZOLE_PRI		(3)
#define SOUND_APP_PRI	(10)
#define CYC_PRI			(9)
#define GYSFDMAXB_PRI	(8)
#define LCD_APL_PRI		(10)

// タスクで使用するスタック
#define BT_DEV_STACK	(0x1000)
#define CONSOLE_STACK	(0x1000)
#define SOUND_APP_STACK	(0x2000)
#define CYC_STACK		(0x1000)
#define GYSFDMAXB_STACK	(0x1000)
#define LCD_APL_STACK	(0x1000)

// 状態遷移用定義
//typedef void (*FUNC)(void *par);
typedef void (*FUNC)(uint32_t par);
typedef struct {
	FUNC func;
	uint8_t  nxt_state;
}FSM;

// 共通戻り値
#define E_OK	(0)
#define E_PAR	(-1)
#define E_TMOUT	(-2)
#define E_STS	(-3)
#define E_OBJ	(-4)

#endif
