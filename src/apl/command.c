#include "defines.h"
#include "kozos.h"
#include "consdrv.h"
#include "lib.h"
#include "us_sensor.h"
#include "stm32l4xx_hal_gpio.h"

typedef struct{
	kz_thread_id_t tsk_id;
	uint8_t state;
}COMMAND_CTL;
COMMAND_CTL command_ctl;

int command_main(int argc, char *argv[])
{
  char *p;
  int size;
  COMMAND_CTL *this = &command_ctl;
	GPIO_PinState pin;

  /* 自身のタスクIDをコンテキストに設定 */
  this->tsk_id = kz_getid();

  consdrv_send_use(SERIAL_DEFAULT_DEVICE);

  while (1) {
	  consdrv_send_write("command> "); /* プロンプト表示 */

    /* コンソールからの受信文字列を受け取る */
    kz_recv(MSGBOX_ID_CONSINPUT, &size, &p);
    p[size] = '\0';

    if (!strncmp(p, "echo", 4)) { /* echoコマンド */
    	consdrv_send_write(p + 4); /* echoに続く文字列を出力する */
    	consdrv_send_write("\n");
    }else if(!strncmp(p, "measure start", 13)){
    	// 測定開始
    	US_MSG_measure_start();
    	//kz_send(MSGBOX_ID_BTINPUT, strlen(p)-3, &p[3]);
    }else if(!strncmp(p, "measure stop", 12)){
    	// 測定停止
    	US_MSG_measure_stop();
    	//kz_send(MSGBOX_ID_BTINPUT, strlen(p)-3, &p[3]);
    }else if(!strncmp(p, "BT status", 9)){
    	BT_MSG_get_status();
    }else{
    	consdrv_send_write("unknown.\n");
    }
#ifndef USE_STATIC_BUFFER
    kz_kmfree(p);
#endif
  }

  return 0;
}


