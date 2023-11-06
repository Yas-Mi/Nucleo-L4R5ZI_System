/*
 * lcd_fw.c
 *
 *  Created on: 2022/12/12
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "lcd_fw.h"
#include "lcd_dev.h"
//#include "lib.h"

// ���
#define LCD_FW_ST_NOT_INTIALIZED	(0U) // ��������
#define LCD_FW_ST_INITIALIZED		(1U) // �������ς�

// �D��x
#define PRIORITY_NUM	(5U)

//  ��ʕۑ��p���X�g
typedef struct _DISPLAY_INFO_LIST{
	DISPLAY_ALL_INFO disp_all_info;
	struct _DISPLAY_INFO_LIST *next;
} DISPLAY_INFO_LIST;
typedef struct {
	DISPLAY_INFO_LIST *head;
	DISPLAY_INFO_LIST *tail;
}DISPLAY_INFO_QUEUE;

// ����p�u���b�N
typedef struct {
	uint8_t status;					// ���
	uint8_t cursor_pos;				// ���ݕ\������Ă���J�[�\���̈ʒu
}LCD_FW_CTL;
static LCD_FW_CTL lcd_fw_ctl;

// ��ʕۑ��p
static DISPLAY_INFO_QUEUE readyque[PRIORITY_NUM];			// ���f�B�[�L���[

// �����֐�
// ��ʂ����X�g�\���ɐݒ肷�鏈��
static int32_t push_disp(DISPLAY_ALL_INFO *disp_all_info)
{
	DISPLAY_INFO_LIST **head;
	DISPLAY_INFO_LIST **tail;
	
	// disp_info��NULL�̏ꍇ�A�G���[��Ԃ��ďI��
	if (disp_all_info == NULL) {
		return -1;
	}
	
	// ���X�g�̏����擾
	head = &(readyque[disp_all_info->priority].head);
	tail = &(readyque[disp_all_info->priority].tail);
	
	if (*head == NULL) {
		// �������m��
		*head = kz_kmalloc(sizeof(DISPLAY_INFO_LIST));
		if (head == NULL) {
			return -1;
		}
		*tail = *head;
	} else {
		// ���X�g�ɒǉ�
		(*tail)->next = kz_kmalloc(sizeof(DISPLAY_INFO_LIST));
		if ((*tail)->next == NULL) {
			return -1;
		}
		*tail = (*tail)->next;
	}
	// �f�[�^���R�s�[
	memcpy(&((*tail)->disp_all_info), disp_all_info, sizeof(DISPLAY_ALL_INFO));
	(*tail)->next = NULL;

	return 0;
}

// ���X�g�̗v�f�̐����擾
static uint8_t get_disp_num(uint32_t priority)
{
	DISPLAY_INFO_LIST *cur;
	uint8_t cnt = 0U;
	
	// �D��x���͈͊O�̏ꍇ�A�G���[��Ԃ��ďI��
	if (priority > PRIORITY_NUM) {
		return -1;
	}
	
	// ���X�g�̐擪���擾
	cur = readyque[priority].head;
	
	// ���X�g�̐����擾
	while (cur->next != NULL) {
		cur = cur->next;
		cnt++;
	}
	
	return cnt;
}

// ��ԐV������ʂ����X�g���珜�����鏈��
static int32_t del_top_disp(uint32_t priority)
{
	DISPLAY_INFO_LIST *cur;
	DISPLAY_INFO_LIST *prev;
	uint8_t num = get_disp_num(priority);
	
	// �v�f�̐���0�̏ꍇ�A�G���[��Ԃ��ďI��
	if (num == 0U) {
		return -1;
	}
	
	// ���X�g�̐擪���擾
	cur = readyque[priority].head;
	
	while (cur->next != NULL) {
		prev = cur;
		cur = cur->next;
	}
	// ���������X�g����폜
	prev->next = NULL;
	kz_kmfree(cur);
	readyque[priority].tail = prev;
	
	return 0;
}

// �O�����J�֐�
// ����������
void LCD_fw_init(void)
{
	LCD_FW_CTL *this = &lcd_fw_ctl;
	
	// ����u���b�N�̏�����
	memset(this, 0, sizeof(LCD_FW_CTL));
	
	// LCD�f�o�C�X�h���C�o�̏�����
	LCD_dev_init();
	
	// ���X�g��������
	memset(readyque, 0, sizeof(DISPLAY_INFO_QUEUE));
	
	// ��Ԃ��������ς݂ɍX�V
	this->status = LCD_FW_ST_INITIALIZED;
	
	return;
}

// �������݂��s���֐�
int8_t LCD_fw_disp(DISPLAY_ALL_INFO *disp_all_info)
{
	LCD_FW_CTL *this = &lcd_fw_ctl;
	int8_t ret;
	int8_t y;
	DISPLAY_INFO *disp_info;
	uint8_t *disp_char;
	uint8_t x_pos;
	uint8_t y_pos;
	
	// disp_all_info��NULL�̏ꍇ�G���[��Ԃ��ďI��
	if (disp_all_info == NULL) {
		return -1;
	}
	
	// �����������{�̏ꍇ�A�G���[��Ԃ��ďI��
	if (this->status != LCD_FW_ST_INITIALIZED) {
		return -1;
	}
	
	// �J�[�\���̈ʒu��ݒ�
	if (disp_all_info->cursor_flag == 1U) {
		switch (disp_all_info->cursor_pos){
			case CURSOR_POS_GO_NEXT:
				// �J�[�\���̈ʒu��i�߂�
				if (this->cursor_pos < (LCD_FW_LINE - 1U)) {
					this->cursor_pos++;
				} else {
					this->cursor_pos = 0U;
				}
				break;
			case CURSOR_POS_GO_BACK:
				// �J�[�\���̈ʒu�����ǂ�
				if (this->cursor_pos > 0) {
					this->cursor_pos--;
				} else {
					this->cursor_pos = LCD_FW_LINE - 1U;
				}
				break;
			default:
				break;
		}
	}
	
	// ��ʂ��o���Ă���
	push_disp(disp_all_info);
	
	// ��������lcd�C���[�W���N���A
	ret = LCD_dev_clear_disp();
	if (ret != 0) {
		return ret;
	}
	
	// �\�������擾
	disp_info = &(disp_all_info->disp_info);
	
	// ��������������
	for (y = 0; y < LCD_FW_LINE; y++) {
		// �����擾
		disp_char = disp_info->char_info[y].disp_char;
		x_pos = disp_info->char_info[y].x_pos;
		y_pos = disp_info->char_info[y].y_pos;
		// �����������
		ret = LCD_dev_write(x_pos, y_pos, disp_char);
		if (ret != 0) {
			return ret;
		}
		// �J�[�\��������
		if (disp_all_info->cursor_flag == 1U) {
			if (y == this->cursor_pos) {
				ret = LCD_dev_write(LCD_FW_CURSOR_XPOS, this->cursor_pos, "��");
				if (ret != 0) {
					return ret;
				}
			}
		}
	}
	
	// �\��
	LCD_dev_disp();
	
	return 0;
}

int8_t LCD_fw_disp_back(uint32_t priority)
{
	DISPLAY_INFO_LIST *cur;
	DISPLAY_ALL_INFO *disp_all_info;
	DISPLAY_INFO *disp_info;
	uint8_t *disp_char;
	uint8_t x_pos;
	uint8_t y_pos;
	int8_t ret;
	int8_t y;
	
	// ��ԐV������ʂ�����
	del_top_disp(priority);
	
	// ���X�g�̐擪���擾
	cur = readyque[priority].head;
	
	//�O�̉�ʂ��擾
	while (cur->next != NULL) {
		cur = cur->next;
	}
	
	// ��������lcd�C���[�W���N���A
	ret = LCD_dev_clear_disp();
	if (ret != 0) {
		return ret;
	}
	
	disp_all_info = &(cur->disp_all_info);
	disp_info = &(disp_all_info->disp_info);
	
	// ��������������
	for (y = 0; y < LCD_FW_LINE; y++) {
		// �����擾
		disp_char = disp_info->char_info[y].disp_char;
		x_pos = disp_info->char_info[y].x_pos;
		y_pos = disp_info->char_info[y].y_pos;
		// �����������
		ret = LCD_dev_write(x_pos, y_pos, disp_char);
		if (ret != 0) {
			return ret;
		}
		// �J�[�\��������
		if (disp_all_info->cursor_flag == 1U) {
			if (y == disp_all_info->cursor_pos) {
				ret = LCD_dev_write(LCD_FW_CURSOR_XPOS, disp_all_info->cursor_pos, "��");
				if (ret != 0) {
					return ret;
				}
			}
		}
	}
	
	// �\��
	LCD_dev_disp();
	
	return 0;
}

int8_t LCD_fw_get_cursor_pos(void)
{
	LCD_FW_CTL *this = &lcd_fw_ctl;
	
	return this->cursor_pos;
}

