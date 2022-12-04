/*
 * lcd_app.c
 *
 *  Created on: 2022/11/03
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "lib.h"
#include "lcd_dev.h"
#include "btn_dev.h"
#include "lcd_app.h"

// �}�N����`
#define VESION   "�u�d�q�r�h�n�m�P�D�O�D�O"

// ���
enum State_Type {
	ST_UNINITIALIZED = 0U,
	ST_INTIALIZED,
	ST_START_UP,
	ST_SELECT_MODE_DISP,		// �T�C�Z�C/�^�J�E�����[�h�I�����
	ST_UNDEIFNED,
	ST_MAX,
};

// ���b�Z�[�W
enum MESSSAGE_Type {
	MSG_INIT = 0U,
	MSG_START_UP,
	MSG_SELECT_MODE,
	MSG_BTN_UP_LONG,
	MSG_BTN_UP_SHORT,
	MSG_BTN_DOWN_LONG,
	MSG_BTN_DOWN_SHORT,
	MSG_BTN_BACK_LONG,
	MSG_BTN_BACK_SHORT,
	MSG_BTN_SELECT_LONG,
	MSG_BTN_SELECT_SHORT,
	MSG_CONNECT,
	MSG_DISCONNECT,
	MSG_MAX,
};

// �֐��|�C���^
typedef int (*FUNC)(void);

// ��ԑJ�ڗp��`
typedef struct {
	FUNC func;
	uint8_t nxt_state;
}FSM;

// ����p�u���b�N
typedef struct {
	kz_thread_id_t tsk_id;		// ���C���^�X�N��ID
	kz_msgbox_id_t msg_id;		// ���b�Z�[�WID
	uint8_t state;				// ���
	// ST_SELECT_MODE_DISP�Ŏg�p������
	uint8_t select_ypos;			//"��"��y�ʒu
	
} LCD_APP_CTL;
LCD_APP_CTL lcd_ap_ctl;

// �v���g�^�C�v�錾
static void lcd_app_init(void);
static void lcd_app_start_up(void);
static void lcd_app_select_mode(void);
static void lcd_app_move_cursor(void);

// ��ԑJ�ڃe�[�u��
static const FSM fsm[ST_MAX][MSG_MAX] = {
	// MSG_INIT					      MSG_START_UP						 MSG_SELECT_MODE   							 MSG_BTN_UP_LONG	   MSG_BTN_UP_SHORT	   				   MSG_BTN_DOWN_LONG	 MSG_BTN_DOWN_SHORT    				  MSG_BTN_BACK_LONG     MSG_BTN_BACK_SHORT    MSG_BTN_SELECT_LONG   MSG_BTN_SELECT_SHORT  MSG_CONNECT MSG_DISCONNECT
	{{lcd_app_init, ST_INTIALIZED},  {NULL, ST_UNDEIFNED}, 				{NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},				   {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, 				  {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},}, // ST_UNINITIALIZED
	{{NULL, ST_UNDEIFNED},			 {lcd_app_start_up, ST_START_UP},   {NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},				   {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, 				  {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},}, // ST_INTIALIZED
	{{NULL, ST_UNDEIFNED},			 {NULL, ST_UNDEIFNED}, 				{lcd_app_select_mode, ST_SELECT_MODE_DISP}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},				   {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, 				  {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},}, // ST_START_UP
	{{NULL, ST_UNDEIFNED},			 {NULL, ST_UNDEIFNED}, 				{NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {lcd_app_move_cursor, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {lcd_app_move_cursor, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},}, // ST_SELECT_MODE_DISP
};

// �����֐�
// �N����ʕ\���֐�
static void lcd_app_start_up(void)
{
	// �N����ʕ\��
	LCD_dev_clear_disp();
	LCD_dev_write(1, 0, "�I���J�J�N�T�C�Z�C�V�X�e��");
	LCD_dev_write(2, 1, VESION);
	LCD_dev_disp();
}

// ���[�h�Z���N�g�\���֐�
static void lcd_app_select_mode(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	
	// �T�C�Z�C/�_�E�����[�h
	LCD_dev_clear_disp();
	LCD_dev_write(1,0,"�T�C�Z�C");
	LCD_dev_write(1,1,"�^�J�E�����[�g�J");
	LCD_dev_write(0,this->select_ypos,"��");
	LCD_dev_disp();
}

// ��{�^���R�[���o�b�N
static void lcd_app_btn_up(BTN_STATUS sts)
{
	// �Z�����̏ꍇ
	if (sts == BTN_SHORT_PUSHED) {
		// ���b�Z�[�W�̑��M
		LCD_APP_MSG_btn_up_short();
	// �������̏ꍇ
	} else {
		// ���b�Z�[�W�̑��M
		LCD_APP_MSG_btn_up_long();
	}
}

// ���{�^���R�[���o�b�N
static void lcd_app_btn_down(BTN_STATUS sts)
{
	// �Z�����̏ꍇ
	if (sts == BTN_SHORT_PUSHED) {
		// ���b�Z�[�W�̑��M
		LCD_APP_MSG_btn_down_short();
	// �������̏ꍇ
	} else {
		// ���b�Z�[�W�̑��M
		LCD_APP_MSG_btn_down_long();
	}
}

// �߂�{�^���R�[���o�b�N
static void lcd_app_btn_back(BTN_STATUS sts)
{
	// �Z�����̏ꍇ
	if (sts == BTN_SHORT_PUSHED) {
		// ���b�Z�[�W�̑��M
		LCD_APP_MSG_btn_back_short();
	// �������̏ꍇ
	} else {
		// ���b�Z�[�W�̑��M
		LCD_APP_MSG_btn_back_long();
	}
}

// ����{�^���R�[���o�b�N
static void lcd_app_btn_select(BTN_STATUS sts)
{
	// �Z�����̏ꍇ
	if (sts == BTN_SHORT_PUSHED) {
		// ���b�Z�[�W�̑��M
		LCD_APP_MSG_btn_select_short();
	// �������̏ꍇ
	} else {
		// ���b�Z�[�W�̑��M
		LCD_APP_MSG_btn_select_long();
	}
}

// �������֐�
static void lcd_app_init(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	
	// LCD�f�o�C�X�h���C�o������
	LCD_dev_init();
	
	//�{�^���̃R�[���o�b�N��ݒ�
	BTN_dev_reg_callback_up(lcd_app_btn_up);
	BTN_dev_reg_callback_down(lcd_app_btn_down);
	BTN_dev_reg_callback_back(lcd_app_btn_back);
	BTN_dev_reg_callback_select(lcd_app_btn_select);
	
	// "��"��y�ʒu��0�Ƃ��Ă���
	this->select_ypos = 0;
}

// �J�[�\���ړ��֐�
static void lcd_app_move_cursor(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	
	// ���ݕ\������Ă���"��"���폜
	LCD_dev_clear(0, this->select_ypos, 1);
	
	// �J�[�\���ʒu�𔽓]������
	this->select_ypos = ~this->select_ypos & 0x01;
	
	// �J�[�\��("��")���ړ� (*)x�ʒu��0�̏ꏊ�ɂ͕������Ȃ��O��
	//LCD_dev_clear_disp();
	LCD_dev_write(0, this->select_ypos, "��");
	LCD_dev_disp();
}

// ���C���̃^�X�N
// ��ԑJ�ڃe�[�u���ɏ]���A���������s���A��Ԃ��X�V����
int LCD_app_main(int argc, char *argv[])
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	uint8_t cur_state = ST_UNINITIALIZED;
	uint8_t nxt_state = cur_state;
	LCD_APP_MSG *msg;
	int32_t size;
	FUNC func;
	
	// ����u���b�N�̏�����
	memset(this, 0, sizeof(LCD_APP_CTL));
	
	// ���^�X�N��ID�𐧌�u���b�N�ɐݒ�
	this->tsk_id = kz_getid();
	// ���b�Z�[�WID�𐧌�u���b�N�ɐݒ�
	this->msg_id = MSGBOX_ID_LCD_APP_MAIN;
	
	while(1){
		// ���b�Z�[�W��M
		kz_recv(this->msg_id, &size, &msg);
		
		// ����/����Ԃ��擾
		func = fsm[cur_state][msg->msg_type].func;
		nxt_state = fsm[cur_state][msg->msg_type].nxt_state;
		
		// ���b�Z�[�W�����
		kz_kmfree(msg);
		
		this->state = nxt_state;
		
		// ���������s
		if (func != NULL) {
			func();
		}
		
		// ��ԑJ��
		if (nxt_state != ST_UNDEIFNED) {
			cur_state = nxt_state;
		}
	}

	return 0;
}

// �O�����J�֐�
// �������v�����b�Z�[�W�𑗐M����֐�
void LCD_APP_MSG_init(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_INIT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// �N����ʕ\�����b�Z�[�W�𑗐M����֐�
void LCD_APP_MSG_start_up(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_START_UP;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// ���[�h�Z���N�g�\�����b�Z�[�W�𑗐M����֐�
void LCD_APP_MSG_select_mode(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_SELECT_MODE;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// ��{�^�����������b�Z�[�W�𑗐M����֐�
void LCD_APP_MSG_btn_up_long(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_UP_LONG;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// ��{�^���Z�������b�Z�[�W�𑗐M����֐�
void LCD_APP_MSG_btn_up_short(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_UP_SHORT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// ���{�^�����������b�Z�[�W�𑗐M����֐�
void LCD_APP_MSG_btn_down_long(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_DOWN_LONG;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// ���{�^���Z�������b�Z�[�W�𑗐M����֐�
void LCD_APP_MSG_btn_down_short(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_DOWN_SHORT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// �߂�{�^�����������b�Z�[�W�𑗐M����֐�
void LCD_APP_MSG_btn_back_long(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_BACK_LONG;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// �߂�{�^���Z�������b�Z�[�W�𑗐M����֐�
void LCD_APP_MSG_btn_back_short(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_BACK_SHORT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// ����{�^�����������b�Z�[�W�𑗐M����֐�
void LCD_APP_MSG_btn_select_long(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_SELECT_LONG;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

// ����{�^���Z�������b�Z�[�W�𑗐M����֐�
void LCD_APP_MSG_btn_select_short(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	LCD_APP_MSG *msg;

	msg = kz_kmalloc(sizeof(LCD_APP_MSG));

	msg->msg_type = MSG_BTN_SELECT_SHORT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(LCD_APP_MSG), msg);

	return;
}

