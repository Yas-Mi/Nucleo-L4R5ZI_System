#include <usart.h>
#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "lib.h"
#include "consdrv.h"

#define CONS_BUFFER_SIZE 40
#define CONSDRV_CMD_USE   'u' /* �R���\�[���E�h���C�o�̎g�p�J�n */
#define CONSDRV_CMD_WRITE 'w' /* �R���\�[���ւ̕�����o�� */
#define STATIC_SEND_CONS_BURREF_SIZE 4096
#define STATIC_RECV_CONS_BURREF_SIZE 128
#define TEMP_CONS_BURREF_SIZE 128

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

#if USE_STATIC_BUFFER
typedef struct {
	uint8_t buf[STATIC_SEND_CONS_BURREF_SIZE];
	uint32_t wr_idx;
	uint32_t rd_idx;
}RING_BUF;
typedef struct {
	RING_BUF snd_consle_buffer;
	uint8_t snd_len;
	uint8_t rcv_consle_buffer[STATIC_RECV_CONS_BURREF_SIZE];
	uint32_t rcv_len;
	int8_t temporary_buffer[TEMP_CONS_BURREF_SIZE];
}STATIC_CONSOLE_BUFFER;
static STATIC_CONSOLE_BUFFER s_buf;
#define get_sbuf() &s_buf;
#endif

static initialized = 0;

static uint8_t get_buf(void)
{
	STATIC_CONSOLE_BUFFER *buf = get_sbuf();
	RING_BUF *ring_buf;
	uint8_t data;

	ring_buf = &(buf->snd_consle_buffer);

	data = ring_buf->buf[ring_buf->rd_idx++];

	ring_buf->rd_idx = ring_buf->rd_idx & (STATIC_SEND_CONS_BURREF_SIZE-1);

	return data;
}

static void set_buf(uint8_t data)
{
	STATIC_CONSOLE_BUFFER *buf = get_sbuf();
	RING_BUF *ring_buf;

	ring_buf = &(buf->snd_consle_buffer);

	ring_buf->buf[ring_buf->wr_idx++] = data;

	ring_buf->wr_idx = ring_buf->wr_idx & (STATIC_SEND_CONS_BURREF_SIZE-1);

	return;
}

static int get_difference_idx(void)
{
	STATIC_CONSOLE_BUFFER *buf = get_sbuf();
	RING_BUF *ring_buf;
	ring_buf = &(buf->snd_consle_buffer);
	return ring_buf->wr_idx - ring_buf->rd_idx;
}

/*
 * �ȉ��̂Q�̊֐�(send_char(), send_string())�͊����ݏ����ƃX���b�h����
 * �Ă΂�邪���M�o�b�t�@�𑀍삵�Ă���ē��s�̂��߁C�X���b�h����Ăяo��
 * �ꍇ�͔r���̂��ߊ����݋֎~��ԂŌĂԂ��ƁD
 */

/* ���M�o�b�t�@�̐擪�P�����𑗐M���� */
static void send_char(struct consreg *cons)
{
	int i;
	STATIC_CONSOLE_BUFFER *buf = get_sbuf();
#if USE_STATIC_BUFFER
	usart_send_byte(cons->index, get_buf());
#else
	usart_send_byte(cons->index, cons->send_buf[0]);
	cons->send_len--;
	/* �擪�����𑗐M�����̂ŁC�P�����Ԃ񂸂炷 */
	for (i = 0; i < cons->send_len; i++)
		cons->send_buf[i] = cons->send_buf[i + 1];
#endif
}

/* ������𑗐M�o�b�t�@�ɏ������ݑ��M�J�n���� */
static void send_string(struct consreg *cons, char *str, int len)
{
	int i;
	STATIC_CONSOLE_BUFFER *buf = get_sbuf();
	for (i = 0; i < len; i++) { /* ������𑗐M�o�b�t�@�ɃR�s�[ */
		if (str[i] == '\n') /* \n��\r\n�ɕϊ� */
#if USE_STATIC_BUFFER
			set_buf('\r') ;
		set_buf(str[i]);
#else
			cons->send_buf[cons->send_len++] = '\r';
		cons->send_buf[cons->send_len++] = str[i];
#endif
  }
  /*
   * ���M�����ݖ����Ȃ�΁C���M�J�n����Ă��Ȃ��̂ő��M�J�n����D
   * ���M�����ݗL���Ȃ�Α��M�J�n����Ă���C���M�����݂̉�����
   * ���M�o�b�t�@���̃f�[�^���������M�����̂ŁC�������Ȃ��Ă悢�D
   */
#if USE_STATIC_BUFFER
  if (get_difference_idx() && !usart_intr_is_send_enable(cons->index)) {
    usart_intr_send_enable(cons->index); /* ���M�����ݗL���� */
    send_char(cons); /* ���M�J�n */
  }
#else
  if (cons->send_len && !usart_intr_is_send_enable(cons->index)) {
    usart_intr_send_enable(cons->index); /* ���M�����ݗL���� */
    send_char(cons); /* ���M�J�n */
  }
#endif
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
	STATIC_CONSOLE_BUFFER *buf = get_sbuf();
	
	if (usart_is_rcv_enable(cons->index)) { /* ��M������ */
		c = usart_recv_byte(cons->index);
		if (c == '\r') /* ���s�R�[�h�ϊ�(\r��\n) */
			c = '\n';
		
		send_string(cons, &c, 1); /* �G�R�[�o�b�N���� */
		
		if (cons->id) {
			if (c != '\n') {
			/* ���s�łȂ��Ȃ�C��M�o�b�t�@�Ƀo�b�t�@�����O���� */
#if USE_STATIC_BUFFER
				buf->rcv_consle_buffer[buf->rcv_len++] = c;
#else
				cons->recv_buf[cons->recv_len++] = c;
#endif
			} else {
				/*
				 * Enter�������ꂽ��C�o�b�t�@�̓��e��
				 * �R�}���h�����X���b�h�ɒʒm����D
				 * (�����݃n���h���Ȃ̂ŁC�T�[�r�X�E�R�[���𗘗p����)
				 */
#if USE_STATIC_BUFFER
				kx_send(MSGBOX_ID_CONSINPUT, buf->rcv_len, &(buf->rcv_consle_buffer));
				buf->rcv_len = 0;
#else
				p = kx_kmalloc(CONS_BUFFER_SIZE);
				memcpy(p, cons->recv_buf, cons->recv_len);
				kx_send(MSGBOX_ID_CONSINPUT, cons->recv_len, p);
				cons->recv_len = 0;
#endif
			}
		}
	}
	
	if (usart_is_send_enable(USART1)) { /* ���M������ */
#if USE_STATIC_BUFFER
		if (!cons->id || (get_difference_idx() == 0)) {
#else
		if (!cons->id || !cons->send_len) {
#endif
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
#if USE_STATIC_BUFFER
	char *p;
	int len;
	len = strlen(str);
	STATIC_CONSOLE_BUFFER *buf = get_sbuf();
	if (initialized) {
		buf->temporary_buffer[0] = '0';
		buf->temporary_buffer[1] = CONSDRV_CMD_WRITE;
		memcpy(&(buf->temporary_buffer[2]), str, len);
	
		kz_send(MSGBOX_ID_CONSOUTPUT, len + 2, buf->temporary_buffer);
	}
#else
	char *p;
	int len;
	len = strlen(str);

	p = kz_kmalloc(len + 2);
	p[0] = '0';
	p[1] = CONSDRV_CMD_WRITE;
	memcpy(&p[2], str, len);
	kz_send(MSGBOX_ID_CONSOUTPUT, len + 2, p);
#endif
}

/* �X���b�h����̗v������������ */
static int consdrv_command(struct consreg *cons, kz_thread_id_t id,
			   int index, int size, char *command)
{
  switch (command[0]) {
  case CONSDRV_CMD_USE: /* �R���\�[���E�h���C�o�̎g�p�J�n */
    cons->id = id;
    cons->index = command[1] - '0';
#if USE_STATIC_BUFFER
  	// �ÓI�o�b�t�@�̏�����
    memset(&s_buf, 0, sizeof(s_buf));
#else
    cons->send_buf = kz_kmalloc(CONS_BUFFER_SIZE);
    cons->recv_buf = kz_kmalloc(CONS_BUFFER_SIZE);
    cons->send_len = 0;
    cons->recv_len = 0;
#endif
    usart_open(cons->index);
    usart_init(cons->index, consdrv_intr);
    usart_intr_recv_enable(cons->index); /* ��M�����ݗL����(��M�J�n) */
    initialized = 1;
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
#ifndef USE_STATIC_BUFFER
    kz_kmfree(p);
#endif
  }

  return 0;
}
