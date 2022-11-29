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
#define CONSDRV_CMD_USE   'u' /* �R���\�[���E�h���C�o�̎g�p�J�n */
#define CONSDRV_CMD_WRITE 'w' /* �R���\�[���ւ̕�����o�� */
#define STATUS_CHECK_TASK_PERIOD (1000U)  // 1000ms���ɐڑ�/��ڑ��`�F�b�N���s��

static struct bt_reg {
  kz_thread_id_t id; /* �R���\�[���𗘗p����X���b�h */
  int index;         /* ���p����V���A���̔ԍ� */

  char *send_buf;    /* ���M�o�b�t�@ */
  char *recv_buf;    /* ��M�o�b�t�@ */
  int send_len;      /* ���M�o�b�t�@���̃f�[�^�T�C�Y */
  int recv_len;      /* ��M�o�b�t�@���̃f�[�^�T�C�Y */

  /* kozos.c �� kz_msgbox �Ɠ��l�̗��R�ŁC�_�~�[�E�����o�ŃT�C�Y�������� */
  long dummy[3];
} bt_reg[1];

/*
 * �ȉ��̂Q�̊֐�(send_char(), send_string())�͊����ݏ����ƃX���b�h����
 * �Ă΂�邪���M�o�b�t�@�𑀍삵�Ă���ē��s�̂��߁C�X���b�h����Ăяo��
 * �ꍇ�͔r���̂��ߊ����݋֎~��ԂŌĂԂ��ƁD
 */

/* ���M�o�b�t�@�̐擪�P�����𑗐M���� */
static void send_char(struct bt_reg *cons)
{
  int i;
  usart_send_byte(cons->index, cons->send_buf[0]);
  cons->send_len--;
  /* �擪�����𑗐M�����̂ŁC�P�����Ԃ񂸂炷 */
  for (i = 0; i < cons->send_len; i++)
    cons->send_buf[i] = cons->send_buf[i + 1];
}

/* ������𑗐M�o�b�t�@�ɏ������ݑ��M�J�n���� */
static void send_string(struct bt_reg *cons, char *str, int len)
{
  int i;
  for (i = 0; i < len; i++) { /* ������𑗐M�o�b�t�@�ɃR�s�[ */
    if (str[i] == '\n') /* \n��\r\n�ɕϊ� */
      cons->send_buf[cons->send_len++] = '\r';
    cons->send_buf[cons->send_len++] = str[i];
  }
  /*
   * ���M�����ݖ����Ȃ�΁C���M�J�n����Ă��Ȃ��̂ő��M�J�n����D
   * ���M�����ݗL���Ȃ�Α��M�J�n����Ă���C���M�����݂̉�����
   * ���M�o�b�t�@���̃f�[�^���������M�����̂ŁC�������Ȃ��Ă悢�D
   */
  if (cons->send_len && !usart_intr_is_send_enable(cons->index)) {
    usart_intr_send_enable(cons->index); /* ���M�����ݗL���� */
    send_char(cons); /* ���M�J�n */
  }
}

/*
 * �ȉ��͊����݃n���h������Ă΂�銄���ݏ����ł���C�񓯊���
 * �Ă΂��̂ŁC���C�u�����֐��Ȃǂ��Ăяo���ꍇ�ɂ͒��ӂ��K�v�D
 * ��{�Ƃ��āC�ȉ��̂����ꂩ�ɓ��Ă͂܂�֐������Ăяo���Ă͂����Ȃ��D
 * �E�ē��\�ł���D
 * �E�X���b�h����Ă΂�邱�Ƃ͖����֐��ł���D
 * �E�X���b�h����Ă΂�邱�Ƃ����邪�C�����݋֎~�ŌĂяo���Ă���D
 * �܂���R���e�L�X�g��ԂŌĂ΂�邽�߁C�V�X�e���E�R�[���͗��p���Ă͂����Ȃ��D
 * (�T�[�r�X�E�R�[���𗘗p���邱��)
 */
static int consdrv_intrproc(struct bt_reg *cons)
{
  unsigned char c;
  char *p;

  if (usart_is_rcv_enable(cons->index)) { /* ��M������ */
    c = usart_recv_byte(cons->index);
    if (cons->id) {
		/*
		* Enter�������ꂽ��C�o�b�t�@�̓��e��
		* �R�}���h�����X���b�h�ɒʒm����D
		* (�����݃n���h���Ȃ̂ŁC�T�[�r�X�E�R�[���𗘗p����)
		*/
    	BT_resp_callback(c);
		cons->recv_len = 0;
    }
  }

  if (usart_is_send_enable(USART2)) { /* ���M������ */
    if (!cons->id || !cons->send_len) {
      /* ���M�f�[�^�������Ȃ�΁C���M�����I�� */
      usart_intr_send_disable(cons->index);
    } else {
      /* ���M�f�[�^������Ȃ�΁C���������M���� */
      send_char(cons);
    }
  }

  return 0;
}

/* �����݃n���h�� */
static void consdrv_intr(void)
{
  int i;
  struct bt_reg *cons;

  for (i = 0; i < CONSDRV_DEVICE_NUM; i++) {
    cons = &bt_reg[i];
    if (cons->id) {
      if (usart_is_send_enable(cons->index) ||
	  usart_is_rcv_enable(cons->index))
	/* �����݂�����Ȃ�΁C�����ݏ������Ăяo�� */
	consdrv_intrproc(cons);
    }
  }
}

static int bt_dev_init(void)
{
  memset(bt_reg, 0, sizeof(bt_reg));
  return 0;
}

/* �R���\�[���E�h���C�o�̎g�p�J�n���R���\�[���E�h���C�o�Ɉ˗����� */
void bt_dev_send_use(int index)
{
  char *p;
  p = kz_kmalloc(3);
  p[0] = '0';
  p[1] = CONSDRV_CMD_USE;
  p[2] = '0' + index;
  kz_send(MSGBOX_ID_BTOUTPUT, 3, p);
}

/* �R���\�[���ւ̕�����o�͂��R���\�[���E�h���C�o�Ɉ˗����� */
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
	
	// �N���b�N�C�l�[�u��
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

/* �X���b�h����̗v������������ */
static int consdrv_command(struct bt_reg *cons, kz_thread_id_t id,
			   int index, int size, char *command)
{
  switch (command[0]) {
  case CONSDRV_CMD_USE: /* �R���\�[���E�h���C�o�̎g�p�J�n */
    cons->id = id;
    cons->index = command[1] - '0';
    cons->send_buf = kz_kmalloc(CONS_BUFFER_SIZE);
    cons->recv_buf = kz_kmalloc(CONS_BUFFER_SIZE);
    cons->send_len = 0;
    cons->recv_len = 0;
    usart_open(cons->index);
    usart_init(cons->index, consdrv_intr);
    usart_intr_recv_enable(cons->index); /* ��M�����ݗL����(��M�J�n) */
	// state pin�̐ݒ�
  	set_state_pin();
    break;

  case CONSDRV_CMD_WRITE: /* �R���\�[���ւ̕�����o�� */
    /*
     * send_string()�ł͑��M�o�b�t�@�𑀍삵�Ă���ē��s�Ȃ̂ŁC
     * �r���̂��߂Ɋ����݋֎~�ɂ��ČĂяo���D
     */
    INTR_DISABLE;
    send_string(cons, command + 1, size - 1); /* ������̑��M */
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
  //kz_setintr(SOFTVEC_TYPE_SERINTR, consdrv_intr); /* �����݃n���h���ݒ� */

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
		// TASK_PERIOD�̊��ԃX���[�v
		kz_tsleep(STATUS_CHECK_TASK_PERIOD);

		// �ڑ���Ԃ��m�F
		status = HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_2);

		// �O��l�ƈ�v�����ꍇ
		if (status == pre_status) {
			// �ڑ����(1)�̏ꍇ
			if (status == 1U) {
				// �ڑ��������Ƃ��A�v���ɒʒm
				BT_MSG_connect();
			// �ڑ����؂ꂽ���(0)�̏ꍇ
			} else {
				// �ڑ����؂ꂽ���Ƃ��A�v���ɒʒm
				BT_MSG_disconnect();
			}
		}

		pre_status = status;
	}

	return 0;
}


