/*
 * bt_dev.c
 *
 *  Created on: 2022/09/18
 *      Author: User
 */
#include <usart.h>
#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "lib.h"
#include "consdrv.h"
#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_pwr_ex.h"

#define CONS_BUFFER_SIZE 24
#define CONSDRV_CMD_USE   'u' /* コンソール・ドライバの使用開始 */
#define CONSDRV_CMD_WRITE 'w' /* コンソールへの文字列出力 */
#define STATUS_CHECK_TASK_PERIOD (1000U)  // 1000ms毎に接続/非接続チェックを行う

static struct bt_reg {
  kz_thread_id_t id; /* コンソールを利用するスレッド */
  int index;         /* 利用するシリアルの番号 */

  char *send_buf;    /* 送信バッファ */
  char *recv_buf;    /* 受信バッファ */
  int send_len;      /* 送信バッファ中のデータサイズ */
  int recv_len;      /* 受信バッファ中のデータサイズ */

  /* kozos.c の kz_msgbox と同様の理由で，ダミー・メンバでサイズ調整する */
  long dummy[3];
} bt_reg[1];

/*
 * 以下の２つの関数(send_char(), send_string())は割込み処理とスレッドから
 * 呼ばれるが送信バッファを操作しており再入不可のため，スレッドから呼び出す
 * 場合は排他のため割込み禁止状態で呼ぶこと．
 */

/* 送信バッファの先頭１文字を送信する */
static void send_char(struct bt_reg *cons)
{
  int i;
  usart_send_byte(cons->index, cons->send_buf[0]);
  cons->send_len--;
  /* 先頭文字を送信したので，１文字ぶんずらす */
  for (i = 0; i < cons->send_len; i++)
    cons->send_buf[i] = cons->send_buf[i + 1];
}

/* 文字列を送信バッファに書き込み送信開始する */
static void send_string(struct bt_reg *cons, char *str, int len)
{
  int i;
  for (i = 0; i < len; i++) { /* 文字列を送信バッファにコピー */
    if (str[i] == '\n') /* \n→\r\nに変換 */
      cons->send_buf[cons->send_len++] = '\r';
    cons->send_buf[cons->send_len++] = str[i];
  }
  /*
   * 送信割込み無効ならば，送信開始されていないので送信開始する．
   * 送信割込み有効ならば送信開始されており，送信割込みの延長で
   * 送信バッファ内のデータが順次送信されるので，何もしなくてよい．
   */
  if (cons->send_len && !usart_intr_is_send_enable(cons->index)) {
    usart_intr_send_enable(cons->index); /* 送信割込み有効化 */
    send_char(cons); /* 送信開始 */
  }
}

/*
 * 以下は割込みハンドラから呼ばれる割込み処理であり，非同期で
 * 呼ばれるので，ライブラリ関数などを呼び出す場合には注意が必要．
 * 基本として，以下のいずれかに当てはまる関数しか呼び出してはいけない．
 * ・再入可能である．
 * ・スレッドから呼ばれることは無い関数である．
 * ・スレッドから呼ばれることがあるが，割込み禁止で呼び出している．
 * また非コンテキスト状態で呼ばれるため，システム・コールは利用してはいけない．
 * (サービス・コールを利用すること)
 */
static int consdrv_intrproc(struct bt_reg *cons)
{
  unsigned char c;
  char *p;

  if (usart_is_rcv_enable(cons->index)) { /* 受信割込み */
    c = usart_recv_byte(cons->index);
    if (cons->id) {
		/*
		* Enterが押されたら，バッファの内容を
		* コマンド処理スレッドに通知する．
		* (割込みハンドラなので，サービス・コールを利用する)
		*/
    	BT_resp_callback(c);
		cons->recv_len = 0;
    }
  }

  if (usart_is_send_enable(USART2)) { /* 送信割込み */
    if (!cons->id || !cons->send_len) {
      /* 送信データが無いならば，送信処理終了 */
      usart_intr_send_disable(cons->index);
    } else {
      /* 送信データがあるならば，引続き送信する */
      send_char(cons);
    }
  }

  return 0;
}

/* 割込みハンドラ */
static void consdrv_intr(void)
{
  int i;
  struct bt_reg *cons;

  for (i = 0; i < CONSDRV_DEVICE_NUM; i++) {
    cons = &bt_reg[i];
    if (cons->id) {
      if (usart_is_send_enable(cons->index) ||
	  usart_is_rcv_enable(cons->index))
	/* 割込みがあるならば，割込み処理を呼び出す */
	consdrv_intrproc(cons);
    }
  }
}

static int bt_dev_init(void)
{
  memset(bt_reg, 0, sizeof(bt_reg));
  return 0;
}

/* コンソール・ドライバの使用開始をコンソール・ドライバに依頼する */
void bt_dev_send_use(int index)
{
  char *p;
  p = kz_kmalloc(3);
  p[0] = '0';
  p[1] = CONSDRV_CMD_USE;
  p[2] = '0' + index;
  kz_send(MSGBOX_ID_BTOUTPUT, 3, p);
}

/* コンソールへの文字列出力をコンソール・ドライバに依頼する */
void bt_dev_send_write(char *str)
{
  char *p;
  int len;
  len = strlen(str);
  p = kz_kmalloc(len + 2);
  p[0] = '0';
  p[1] = CONSDRV_CMD_WRITE;
  memcpy(&p[2], str, len);
  kz_send(MSGBOX_ID_BTOUTPUT, len + 2, p);
}

static void set_state_pin(void) 
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	
	// クロックイネーブル
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();
	HAL_PWREx_EnableVddIO2();
#if 0
	/*Configure GPIO pin : PG3 */
	GPIO_InitStruct.Pin = GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
	HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI3_IRQn);
#endif
#if 1
	/*Configure GPIO pin : PG2 */
	GPIO_InitStruct.Pin = GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
#endif
}

/* スレッドからの要求を処理する */
static int consdrv_command(struct bt_reg *cons, kz_thread_id_t id,
			   int index, int size, char *command)
{
  switch (command[0]) {
  case CONSDRV_CMD_USE: /* コンソール・ドライバの使用開始 */
    cons->id = id;
    cons->index = command[1] - '0';
    cons->send_buf = kz_kmalloc(CONS_BUFFER_SIZE);
    cons->recv_buf = kz_kmalloc(CONS_BUFFER_SIZE);
    cons->send_len = 0;
    cons->recv_len = 0;
    usart_open(cons->index);
    usart_init(cons->index, consdrv_intr);
    usart_intr_recv_enable(cons->index); /* 受信割込み有効化(受信開始) */
	// state pinの設定
  	set_state_pin();
    break;

  case CONSDRV_CMD_WRITE: /* コンソールへの文字列出力 */
    /*
     * send_string()では送信バッファを操作しており再入不可なので，
     * 排他のために割込み禁止にして呼び出す．
     */
    INTR_DISABLE;
    send_string(cons, command + 1, size - 1); /* 文字列の送信 */
    INTR_ENABLE;
    break;

  default:
    break;
  }

  return 0;
}

int BT_dev_main(int argc, char *argv[])
{
  int size, index;
  kz_thread_id_t id;
  char *p;

  bt_dev_init();
  //kz_setintr(SOFTVEC_TYPE_SERINTR, consdrv_intr); /* 割込みハンドラ設定 */

  while (1) {
    id = kz_recv(MSGBOX_ID_BTOUTPUT, &size, &p);
    index = p[0] - '0';
    consdrv_command(&bt_reg[index], id, index, size - 1, p + 1);
    kz_kmfree(p);
  }

  return 0;
}

int BT_mng_connect_sts(int argc, char *argv[])
{
	GPIO_PinState status = 0U;
	GPIO_PinState pre_status = 0U;

	while(1){
		// TASK_PERIODの期間スリープ
		kz_tsleep(STATUS_CHECK_TASK_PERIOD);

		// 接続状態を確認
		status = HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_2);

		// 前回値と一致した場合
		if (status == pre_status) {
			// 接続状態(1)の場合
			if (status == 1U) {
				// 接続したことをアプリに通知
				BT_MSG_connect();
			// 接続が切れた状態(0)の場合
			} else {
				// 接続が切れたことをアプリに通知
				BT_MSG_disconnect();
			}
		}

		pre_status = status;
	}

	return 0;
}


