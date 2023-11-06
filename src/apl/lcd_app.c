/*
 * lcd_app.c
 *
 *  Created on: 2022/11/03
 *      Author: User
 */
#include <string.h>
#include "defines.h"
#include "kozos.h"
#include "console.h"
//#include "lib.h"
#include "lcd_fw.h"
#include "gysfdmaxb.h"
#include "lcd_app.h"
#include "str_util.h"

// ���
#define ST_UNINITIALIZED	(0)		// ��������
#define ST_IDLE				(1)		// �A�C�h��
#define ST_UNDEFINED		(2)		// ����`
#define ST_MAX				(3)

// �C�x���g
#define EVENT_SEND			(0)		// ���M
#define EVENT_CYC_PROC		(1)		// �����C�x���g
#define EVENT_MAX			(2)

// �}�N��
#define LCD_APL_PROC_PERIOD	(1000)	// 1000ms����

// ���b�Z�[�W
typedef struct {
	uint32_t event;
	uint32_t data;
}LCD_APL_MSG;

// ����u���b�N
typedef struct {
	uint32_t			state;		// ���
	kz_msgbox_id_t		msg_id;		// ���b�Z�[�WID
	kz_thread_id_t		tsk_id;		// BlueToot��ԊǗ��^�X�NID
	GYSFDMAXB_DATA		gps_data;	// GPS�f�[�^
} LCD_APP_CTL;
static LCD_APP_CTL lcd_apl_ctl;

/* �j���̎Z�o�֐�(�c�F���[�̌�����) */
// https://bugs-c.info/uncategorized/6172/
char *zeller(int year, int mon, int day)
{
	static char *w[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
	if ((mon == 1) || (mon == 2)){
		year--;
		mon += 12;
	}
	
	int idx = (year + year/4 - year/100 + year/400 + (13 * mon + 8)/5 + day) % 7;
	
	return  w[idx];
}

// GPS�R�[���o�b�N
static void gysfdmaxb_callback(void *vp, GYSFDMAXB_DATA *data)
{
	LCD_APP_CTL *this = (LCD_APP_CTL*)vp;
	
	memcpy(&(this->gps_data), data, sizeof(GYSFDMAXB_DATA));
}

// ��������
static void lcd_app_cyc_proc(uint32_t par)
{
	LCD_APP_CTL *this = &lcd_apl_ctl;
	GYSFDMAXB_DATA gps_data;
	char text[DISP_STRING_MAX];
	char *week;
	uint8_t val[2] = {0};
	static uint32_t cnt = 0;
	uint8_t period_cnt = 0;
	uint8_t i;
	
	// GPS�f�[�^���擾
	// �����݋֎~
	INTR_DISABLE;
	memcpy(&gps_data, &(this->gps_data), sizeof(GYSFDMAXB_DATA));
	// �����݋֎~����
	INTR_ENABLE;
	
	// ���ʂ��Ă���H
	if (gps_data.status != 'A') {
		// �\���N���A
		LCD_dev_clear_disp();
		// ���ʂ���܂ł�"WAITING..."��\��
		memset(text, ' ', sizeof(text));
		text[16] = '\0';
		memcpy(text, "WAITING", strlen("WAITING"));
		period_cnt = cnt++%4;
		for (i = 0; i < period_cnt; i++) {
			text[7+i] = '.';
		}
		han2zen(text);
		LCD_dev_write(0, 0, text);
		// �\��
		LCD_dev_disp();
		return;
	}
	
	// �\���N���A
	LCD_dev_clear_disp();
	
	// ������
	memset(text, ' ', sizeof(text));
	text[16] = '\0';
	
	// 1�s��
	// �N
	text[0] = '2';
	text[1] = '0';
	text[2] = num2ascii(gps_data.year/10);
	text[3] = num2ascii(gps_data.year-((gps_data.year/10)*10));
	text[4] = '/';
	// ��
	if (gps_data.month >= 10) {
		text[5] = '1';
	}
	text[6] = num2ascii(gps_data.month-((gps_data.month/10)*10));
	text[7] = '/';
	// ��
	if (gps_data.day >= 10) {
		text[8] = num2ascii(gps_data.day/10);
	}
	text[9] = num2ascii(gps_data.day-((gps_data.day/10)*10));
	// �j��
	week = zeller(2000+gps_data.year, gps_data.month, gps_data.day);
	memcpy(&(text[11]), week, strlen(week));
	
	// �ϊ����ĕ\��
	han2zen(text);
	LCD_dev_write(0, 0, text);
	
	// ������
	memset(text, ' ', sizeof(text));
	text[16] = '\0';
	
	// 2�s��
	// ��
	if (gps_data.hour >= 10) {
		text[0] = num2ascii(gps_data.hour/10);
	}
	text[1] = num2ascii(gps_data.hour-((gps_data.hour/10)*10));
	text[2] = ':';
	// ��
	if (gps_data.minute >= 10) {
		text[3] = num2ascii(gps_data.minute/10);
	} else {
		text[3] = '0';
	}
	text[4] = num2ascii(gps_data.minute-((gps_data.minute/10)*10));
	// �o�x
	if (gps_data.longitude >= 100) {
		val[0] = 100;
		text[7] = '1';
	}
	if (gps_data.longitude >= 10) {
		val[1] = gps_data.longitude - val[0];
		text[8] = num2ascii(val[1]/10);
	}
	text[9] = num2ascii(gps_data.longitude-val[0]-(val[1]/10)*10);
	text[10] = ':';
	// �ܓx
	if (gps_data.latitude >= 10) {
		text[12] = num2ascii(gps_data.latitude/10);
	}
	text[13] = num2ascii(gps_data.latitude-((gps_data.latitude/10)*10));
	
	// �ϊ����ĕ\��
	han2zen(text);
	LCD_dev_write(0, 1, text);
	
	// �\��
	LCD_dev_disp();
}

// ��ԑJ�ڃe�[�u��
static const FSM fsm[ST_MAX][EVENT_MAX] = {
	// EVENT_SEND				EVENT_CYC_PROC
	{{NULL, ST_UNDEFINED},		{NULL, ST_UNDEFINED}, 			}, // ST_UNINITIALIZED
	{{NULL, ST_UNDEFINED},		{lcd_app_cyc_proc, ST_IDLE},	}, // ST_IDLE
	{{NULL, ST_UNDEFINED},		{NULL, ST_UNDEFINED}, 			}, // ST_UNDEFINED
};

// ���C���̃^�X�N
// ��ԑJ�ڃe�[�u���ɏ]���A���������s���A��Ԃ��X�V����
int lcd_app_main(int argc, char *argv[])
{
	LCD_APP_CTL *this = &lcd_apl_ctl;
	LCD_APL_MSG *msg;
	LCD_APL_MSG tmp_msg;
	int32_t size;
	
	while(1){
		// ���b�Z�[�W��M
		kz_recv(this->msg_id, &size, &msg);
		// �������񃁃b�Z�[�W�����[�J���ϐ��ɃR�s�[
		memcpy(&tmp_msg, msg, sizeof(LCD_APL_MSG));
		// ���b�Z�[�W�����
		kz_kmfree(msg);
		// ���������s
		if (fsm[this->state][tmp_msg.event].func != NULL) {
			fsm[this->state][tmp_msg.event].func(tmp_msg.data);
		}
		// ��ԑJ��
		if (fsm[this->state][tmp_msg.event].nxt_state != ST_UNDEFINED) {
			this->state = fsm[this->state][tmp_msg.event].nxt_state;
		}
	}
	
	return 0;
}

// �O�����J�֐�
// �������v�����b�Z�[�W�𑗐M����֐�
void lcd_apl_init(void)
{
	LCD_APP_CTL *this = &lcd_apl_ctl;
	
	// ����u���b�N�̏�����
	memset(this, 0, sizeof(LCD_APP_CTL));
	
	// ���b�Z�[�WID�̐ݒ�
	this->msg_id = MSGBOX_ID_LCD_APP_MAIN;
	
	// �������b�Z�[�W�̍쐬
	set_cyclic_message(lcd_apl_cyc_msg, LCD_APL_PROC_PERIOD);
	
	// �R�[���o�b�N�o�^
	gysfdmaxb_reg_callback(gysfdmaxb_callback, this);
	
	// �^�X�N�̐���
	this->tsk_id = kz_run(lcd_app_main, "lcd_app_main",  LCD_APL_PRI, LCD_APL_STACK, 0, NULL);
	
	// ��Ԃ̍X�V
	this->state = ST_IDLE;
	
	return;
}

// �������v�����b�Z�[�W�𑗐M����֐�
void lcd_apl_cyc_msg(void)
{
	LCD_APP_CTL *this = &lcd_apl_ctl;
	LCD_APL_MSG *msg;
	
	msg = kz_kmalloc(sizeof(LCD_APL_MSG));
	msg->event = EVENT_CYC_PROC;
	msg->data = NULL;
	kz_send(this->msg_id, sizeof(LCD_APL_MSG), msg);
}
