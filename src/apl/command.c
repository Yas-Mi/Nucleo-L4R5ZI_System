#include "defines.h"
#include "kozos.h"
#include "consdrv.h"
#include "lib.h"

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
    }else if(!strncmp(p, "BT ", 3)){
    	kz_send(MSGBOX_ID_BTINPUT, strlen(p)-3, &p[3]);


    }else{
    	consdrv_send_write("unknown.\n");
    }

    kz_kmfree(p);
  }

  return 0;
}


