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
#include "lcd_fw.h"
#include "btn_dev.h"
#include "lcd_app.h"

// �}�N����`
#define VESION   "�u�d�q�r�h�n�m�P�D�O�D�O"

// ���
enum State_Type {
	ST_UNINITIALIZED = 0U,
	ST_INTIALIZED,
	ST_START_UP,
	ST_SELECT_MODE_DISP,			// �T�C�Z�C/�^�J�E�����[�h�I�����
	ST_SELECT_RESUME_MODE_DISP,		// �X�g���[�~���O�T�C�Z�C/�_�E�����[�h�T�C�Z�C�I�����
	ST_UNDEIFNED,
	ST_MAX,
};

typedef enum {
	DISPLAY_LIST_NO_DISP = 0,		//	�����\�����Ȃ�
	DISPLAY_LIST_START_UP,			//	�N�����
	DISPLAY_LIST_SELECT_MODE,		//	���[�h�I��
	DISPLAY_LIST_RESUME_MODE,		//	�T�C�Z�C���[�h�I��
	DISPLAY_LIST_MAX,				//	�ő�l
}DISPLAY_LIST;

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

// ����p�u���b�N
typedef struct {
	kz_thread_id_t tsk_id;		// ���C���^�X�N��ID
	kz_msgbox_id_t msg_id;		// ���b�Z�[�WID
	uint8_t state;				// ���
	// ST_SELECT_MODE_DISP�Ŏg�p������
	uint8_t select_ypos;			//"��"��y�ʒu
} LCD_APP_CTL;
LCD_APP_CTL lcd_ap_ctl;

static const DISPLAY_INFO disp_info[DISPLAY_LIST_MAX] = {
	// DISPLAY_LIST_NO_DISP
	{	
		{
			{	0, 0, ""	},
			{	0, 0, ""	},
		}
	},
	// DISPLAY_LIST_START_UP
	{	
		{
			{	1, 0, "�I���J�J�N�T�C�Z�C�V�X�e��"	},
			{	1, 1, "�u�d�q�r�h�n�m�P�D�O�D�O"	},
		}
	},
	// DISPLAY_LIST_SELECT_MODE
	{
		{
			{	1, 0, "�T�C�Z�C"			},
			{	1, 1, "�^�J�E�����[�g�J"	},
		}
	},
	// DISPLAY_LIST_RESUME_MODE
	{
		{
			{	1, 0, "�X�g���[�~���N�J�T�C�Z�C"		},
			{	1, 1, "�^�J�E�����[�g�J�T�C�Z�C"		},
		}
	},
};

// �e��Ԃŕ\��������
static const DISPLAY_LIST status_disp_tbl[ST_MAX] = {
	DISPLAY_LIST_NO_DISP,			// �\���Ȃ�
	DISPLAY_LIST_NO_DISP,			// �\���Ȃ�
	DISPLAY_LIST_START_UP,			// �N�����
	DISPLAY_LIST_SELECT_MODE,		// ���[�h�Z���N�g���
	DISPLAY_LIST_RESUME_MODE,		// �Đ����[�h�Z���N�g���
	DISPLAY_LIST_NO_DISP,			// �\���Ȃ�
};

// �����֐�
// �N����ʕ\���֐�
static void lcd_app_start_up(void)
{
	DISPLAY_ALL_INFO info;
	
	// �\����������R�s�[
	memcpy(&(info.disp_info), &(disp_info[DISPLAY_LIST_START_UP]), sizeof(DISPLAY_INFO));
	// �J�[�\���Ȃ�
	info.cursor_flag = 0U;
	// �D��x�ݒ�
	info.priority = 0U;
	
	// �N�����ʕ\��
	LCD_fw_disp(&info);
}

// ���[�h�Z���N�g�\���֐�
static void lcd_app_select_mode(void)
{
	DISPLAY_ALL_INFO info;
	
	// �\����������R�s�[
	memcpy(&(info.disp_info), &(disp_info[DISPLAY_LIST_SELECT_MODE]), sizeof(DISPLAY_INFO));
	// �J�[�\������
	info.cursor_flag = 1U;
	// �J�[�\���ʒu�͕ς��Ȃ�
	info.cursor_pos = CURSOR_POS_NO_CHANGE;
	// �D��x�ݒ�
	info.priority = 0U;
	
	// �T�C�Z�C/�_�E�����[�h
	LCD_fw_disp(&info);
}

// �T�C�Z�C���[�h�Z���N�g�\���֐�
static void lcd_app_select_resume_mode(void)
{
	DISPLAY_ALL_INFO info;
	uint8_t cursor_pos;
	
	// ���݂̃J�[�\���ʒu���擾
	cursor_pos = LCD_fw_get_cursor_pos();
	
	switch (cursor_pos) {
		// �T�C�Z�C�I��
		case 0 :
			// �\����������R�s�[
			memcpy(&(info.disp_info), &(disp_info[DISPLAY_LIST_RESUME_MODE]), sizeof(DISPLAY_INFO));
			// �J�[�\������
			info.cursor_flag = 1U;
			// �J�[�\���ʒu�͕ς��Ȃ�
			info.cursor_pos = CURSOR_POS_NO_CHANGE;
			// �D��x�ݒ�
			info.priority = 0U;
			// �T�C�Z�C/�_�E�����[�h
			LCD_fw_disp(&info);
			break;
		// �_�E�����[�h�I��
		case 1 :
			break;
		default :
			break;
	}
	

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
	
	// LCDFW�f�o�C�X�h���C�o������
	LCD_fw_init();
	
	//�{�^���̃R�[���o�b�N��ݒ�
	BTN_dev_reg_callback_up(lcd_app_btn_up);
	BTN_dev_reg_callback_down(lcd_app_btn_down);
	BTN_dev_reg_callback_back(lcd_app_btn_back);
	BTN_dev_reg_callback_select(lcd_app_btn_select);
	
	// "��"��y�ʒu��0�Ƃ��Ă���
	this->select_ypos = 0;
}

// �J�[�\���ړ��֐�
static void lcd_app_move_cursor_up(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	DISPLAY_ALL_INFO info;
	
	// �\����������R�s�[
	memcpy(&(info.disp_info), &(disp_info[status_disp_tbl[this->state]]), sizeof(DISPLAY_INFO));
	// �J�[�\������
	info.cursor_flag = 1U;
	// �J�[�\���ʒu��i�߂�
	info.cursor_pos = CURSOR_POS_GO_NEXT;
	
	// �T�C�Z�C/�_�E�����[�h
	LCD_fw_disp(&info);
}

// �J�[�\���ړ��֐�
static void lcd_app_move_cursor_down(void)
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	DISPLAY_ALL_INFO info;
	
	// �\����������R�s�[
	memcpy(&(info.disp_info), &(disp_info[status_disp_tbl[this->state]]), sizeof(DISPLAY_INFO));
	// �J�[�\������
	info.cursor_flag = 1U;
	// �J�[�\���ʒu��i�߂�
	info.cursor_pos = CURSOR_POS_GO_BACK;
	
	// �T�C�Z�C/�_�E�����[�h
	LCD_fw_disp(&info);
}

// ��ԑJ�ڃe�[�u��
static const FSM fsm[ST_MAX][MSG_MAX] = {
	// MSG_INIT					      MSG_START_UP						 MSG_SELECT_MODE   							 MSG_BTN_UP_LONG	   MSG_BTN_UP_SHORT	   						MSG_BTN_DOWN_LONG		MSG_BTN_DOWN_SHORT    						MSG_BTN_BACK_LONG	  MSG_BTN_BACK_SHORT					MSG_BTN_SELECT_LONG   MSG_BTN_SELECT_SHORT  						MSG_CONNECT MSG_DISCONNECT
	{{lcd_app_init, ST_INTIALIZED},  {NULL, ST_UNDEIFNED}, 				{NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},						{NULL, ST_UNDEIFNED},	{NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},					{NULL, ST_UNDEIFNED}, {NULL,						ST_UNDEIFNED},}, // ST_UNINITIALIZED
	{{NULL, ST_UNDEIFNED},			 {lcd_app_start_up, ST_START_UP},   {NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},						{NULL, ST_UNDEIFNED},	{NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},					{NULL, ST_UNDEIFNED}, {NULL,						ST_UNDEIFNED},}, // ST_INTIALIZED
	{{NULL, ST_UNDEIFNED},			 {NULL, ST_UNDEIFNED}, 				{lcd_app_select_mode, ST_SELECT_MODE_DISP}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},						{NULL, ST_UNDEIFNED},	{NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},					{NULL, ST_UNDEIFNED}, {NULL,						ST_UNDEIFNED},}, // ST_START_UP
	{{NULL, ST_UNDEIFNED},			 {NULL, ST_UNDEIFNED}, 				{NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {lcd_app_move_cursor_up, ST_UNDEIFNED},	{NULL, ST_UNDEIFNED},	{lcd_app_move_cursor_down, ST_UNDEIFNED},	{NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},					{NULL, ST_UNDEIFNED}, {lcd_app_select_resume_mode,	ST_SELECT_RESUME_MODE_DISP},}, // ST_SELECT_MODE_DISP
	{{NULL, ST_UNDEIFNED},			 {NULL, ST_UNDEIFNED}, 				{NULL, ST_UNDEIFNED}, 						{NULL, ST_UNDEIFNED}, {lcd_app_move_cursor_up, ST_UNDEIFNED},	{NULL, ST_UNDEIFNED},	{lcd_app_move_cursor_down, ST_UNDEIFNED},	{NULL, ST_UNDEIFNED}, {LCD_fw_disp_back, ST_UNDEIFNED},		{NULL, ST_UNDEIFNED}, {NULL,						ST_UNDEIFNED},}, // ST_SELECT_RESUME_MODE_DISP
};

// ���C���̃^�X�N
// ��ԑJ�ڃe�[�u���ɏ]���A���������s���A��Ԃ��X�V����
int LCD_app_main(int argc, char *argv[])
{
	LCD_APP_CTL *this = &lcd_ap_ctl;
	this->state = ST_UNINITIALIZED;
	uint8_t nxt_state = this->state;
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
		func = fsm[this->state][msg->msg_type].func;
		nxt_state = fsm[this->state][msg->msg_type].nxt_state;
		
		// ���b�Z�[�W�����
		kz_kmfree(msg);
		
		// ���������s
		if (func != NULL) {
			func();
		}
		
		// ��ԑJ��
		if (nxt_state != ST_UNDEIFNED) {
			this->state = nxt_state;
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

