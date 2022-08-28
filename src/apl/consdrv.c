#include <usart.h>
#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "lib.h"
#include "consdrv.h"

#define CONS_BUFFER_SIZE 24
#define CONSDRV_CMD_USE   'u' /* �R���\�[���E�h���C�o�̎g�p�J�n */
#define CONSDRV_CMD_WRITE 'w' /* �R���\�[���ւ̕�����o�� */

static struct consreg {
  kz_thread_id_t id; /* �R���\�[���𗘗p����X���b�h */
  int index;         /* ���p����V���A���̔ԍ� */

  char *send_buf;    /* ���M�o�b�t�@ */
  char *recv_buf;    /* ��M�o�b�t�@ */
  int send_len;      /* ���M�o�b�t�@���̃f�[�^�T�C�Y */
  int recv_len;      /* ��M�o�b�t�@���̃f�[�^�T�C�Y */

  /* kozos.c �� kz_msgbox �Ɠ��l�̗��R�ŁC�_�~�[�E�����o�ŃT�C�Y�������� */
  long dummy[3];
} consreg[CONSDRV_DEVICE_NUM];

/*
 * �ȉ��̂Q�̊֐�(send_char(), send_string())�͊����ݏ����ƃX���b�h����
 * �Ă΂�邪���M�o�b�t�@�𑀍삵�Ă���ē��s�̂��߁C�X���b�h����Ăяo��
 * �ꍇ�͔r���̂��ߊ����݋֎~��ԂŌĂԂ��ƁD
 */

/* ���M�o�b�t�@�̐擪�P�����𑗐M���� */
static void send_char(struct consreg *cons)
{
  int i;
  usart_send_byte(cons->index, cons->send_buf[0]);
  cons->send_len--;
  /* �擪�����𑗐M�����̂ŁC�P�����Ԃ񂸂炷 */
  for (i = 0; i < cons->send_len; i++)
    cons->send_buf[i] = cons->send_buf[i + 1];
}

/* ������𑗐M�o�b�t�@�ɏ������ݑ��M�J�n���� */
static void send_string(struct consreg *cons, char *str, int len)
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
static int consdrv_intrproc(struct consreg *cons)
{
  unsigned char c;
  char *p;

  if (usart_is_rcv_enable(cons->index)) { /* ��M������ */
    c = usart_recv_byte(cons->index);
    if (c == '\r') /* ���s�R�[�h�ϊ�(\r��\n) */
      c = '\n';

    send_string(cons, &c, 1); /* �G�R�[�o�b�N���� */

    if (cons->id) {
      if (c != '\n') {
	/* ���s�łȂ��Ȃ�C��M�o�b�t�@�Ƀo�b�t�@�����O���� */
	cons->recv_buf[cons->recv_len++] = c;
      } else {
	/*
	 * Enter�������ꂽ��C�o�b�t�@�̓��e��
	 * �R�}���h�����X���b�h�ɒʒm����D
	 * (�����݃n���h���Ȃ̂ŁC�T�[�r�X�E�R�[���𗘗p����)
	 */
	p = kx_kmalloc(CONS_BUFFER_SIZE);
	memcpy(p, cons->recv_buf, cons->recv_len);
	kx_send(MSGBOX_ID_CONSINPUT, cons->recv_len, p);
	cons->recv_len = 0;
      }
    }
  }

  if (usart_is_send_enable(USART1)) { /* ���M������ */
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
  struct consreg *cons;

  for (i = 0; i < CONSDRV_DEVICE_NUM; i++) {
    cons = &consreg[i];
    if (cons->id) {
      if (usart_is_send_enable(cons->index) ||
	  usart_is_rcv_enable(cons->index))
	/* �����݂�����Ȃ�΁C�����ݏ������Ăяo�� */
	consdrv_intrproc(cons);
    }
  }
}

static int consdrv_init(void)
{
  memset(consreg, 0, sizeof(consreg));
  return 0;
}

/* �R���\�[���E�h���C�o�̎g�p�J�n���R���\�[���E�h���C�o�Ɉ˗����� */
void consdrv_send_use(int index)
{
  char *p;
  p = kz_kmalloc(3);
  p[0] = '0';
  p[1] = CONSDRV_CMD_USE;
  p[2] = '0' + index;
  kz_send(MSGBOX_ID_CONSOUTPUT, 3, p);
}

/* �R���\�[���ւ̕�����o�͂��R���\�[���E�h���C�o�Ɉ˗����� */
void consdrv_send_write(char *str)
{
  char *p;
  int len;
  len = strlen(str);
  p = kz_kmalloc(len + 2);
  p[0] = '0';
  p[1] = CONSDRV_CMD_WRITE;
  memcpy(&p[2], str, len);
  kz_send(MSGBOX_ID_CONSOUTPUT, len + 2, p);
}

/* �X���b�h����̗v������������ */
static int consdrv_command(struct consreg *cons, kz_thread_id_t id,
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

int consdrv_main(int argc, char *argv[])
{
  int size, index;
  kz_thread_id_t id;
  char *p;

  consdrv_init();
  //kz_setintr(SOFTVEC_TYPE_SERINTR, consdrv_intr); /* �����݃n���h���ݒ� */

  while (1) {
    id = kz_recv(MSGBOX_ID_CONSOUTPUT, &size, &p);
    index = p[0] - '0';
    consdrv_command(&consreg[index], id, index, size - 1, p + 1);
    kz_kmfree(p);
  }

  return 0;
}
