#ifndef _DEFINES_H_INCLUDED_
#define _DEFINES_H_INCLUDED_

#include "usart.h"

#define NULL ((void *)0)
#define SERIAL_DEFAULT_DEVICE (USART1)
#define SERIAL_BLUETOOTH_DEVICE (USART2)

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned long  uint32;

typedef uint32 kz_thread_id_t;
typedef int (*kz_func_t)(int argc, char *argv[]);
typedef void (*kz_handler_t)(void);

typedef enum {
  MSGBOX_ID_CONSINPUT = 0,
  MSGBOX_ID_CONSOUTPUT,
  MSGBOX_ID_BTMAIN,
  MSGBOX_ID_BTINPUT,
  MSGBOX_ID_BTOUTPUT,
  MSGBOX_ID_US_MAIN,
  MSGBOX_ID_CTL_MAIN,
  MSGBOX_ID_BTN_MAIN,
  MSGBOX_ID_LCD_APP_MAIN,
//  MSGBOX_ID_US_DATA,
  MSGBOX_ID_NUM
} kz_msgbox_id_t;

#endif
