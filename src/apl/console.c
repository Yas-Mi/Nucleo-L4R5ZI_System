#include <console.h>
#include "usart.h"
#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "lib.h"

#define CONOLE_BUF_SIZE		(128U)		// �R�}���h���C���o�b�t�@�T�C�Y
#define CONOLE_CMD_NUM		(16U)		// �ݒ�ł���R�}���h�̐�
#define CONSOLE_USART_CH	USART_CH1	// �g�p����USART�̃`���l��
#define CONSOLE_BAUDRATE	(9600)		// �R���\�[���̃{�[���[�g

// ����u���b�N
typedef struct {
	kz_thread_id_t		tsk_id;						// �R���\�[���^�X�N��ID
	kz_thread_id_t		snd_tsk_id;					// ���M�v�����o�����^�X�N��ID
	kz_msgbox_id_t		msg_id;						// ���b�Z�[�WID
	char				buf[CONOLE_BUF_SIZE];		// �R�}���h���C���o�b�t�@
	uint8_t				buf_idx;					// �R�}���h���C���o�b�t�@�C���f�b�N�X
	COMMAND_INFO		cmd_info[CONOLE_CMD_NUM];	// �R�}���h�֐�
	uint8_t				cmd_idx;					// �R�}���h�֐��C���f�b�N�X
} CONSOLE_CTL;
static CONSOLE_CTL console_ctl;

// ��M�R�[���o�b�N
static void console_recv_callback(USART_CH ch, void *vp)
{
	CONSOLE_CTL *this;
	
	// ����u���b�N�̎擾
	this = &console_ctl;
	
	// �R���\�[���^�X�N���N����
	kx_wakeup(this->tsk_id);
}

// ���M�R�[���o�b�N
static void console_send_callback(USART_CH ch, void *vp)
{
	// ���ɉ������Ȃ�
}

static void console_init(void)
{
	CONSOLE_CTL *this;
	int32_t ret;
	
	// ����u���b�N�̎擾
	this = &console_ctl;
	
	// ����u���b�N�̏�����
	memset(this, 0, sizeof(CONSOLE_CTL));
	
	// ���^�X�NID�̎擾
	this->tsk_id = kz_getid();
	this->msg_id = MSGBOX_ID_BTMAIN;
	
	// �R�[���o�b�N�̐ݒ�
	usart_reg_recv_callback(CONSOLE_USART_CH, console_recv_callback, this);
	usart_reg_send_callback(CONSOLE_USART_CH, console_send_callback, this);
	
	// USART�̃I�[�v��
	ret = usart_open(CONSOLE_USART_CH, CONSOLE_BAUDRATE);
	
	return;
}

// �R���\�[������̓��͂���M����֐�
static uint8_t console_recv(void)
{
	uint8_t data;
	
	// ��M�ł���ő҂�
	while (usart_recv(CONSOLE_USART_CH, &data, 1)) {
		// ��M�f�[�^���Ȃ��ꍇ�͐Q��
		kz_sleep();
	}
	
	return data;
}

// �R���\�[������̓��͂���M����֐�
static void console_analysis(uint8_t data)
{
	CONSOLE_CTL *this;
	COMMAND_INFO *cmd_info;
	uint8_t i;
	
	// ����u���b�N�̎擾
	this = &console_ctl;
	
	switch (data) {
		case '\t':	// tab
			break;
		case '\b':	// back space
			break;
		case '\n':	// Enter
			// NULL������ݒ�
			this->buf[this->buf_idx++] = '\0';
			// �R�}���h�ɐݒ肳��Ă���H
			for (i = 0; i < this->cmd_idx; i++) {
				cmd_info = &(this->cmd_info[i]);
				// �R�}���h���s
				if (strcmp(this->buf, cmd_info->input)) {
					cmd_info->func();
				}
			}
			// ���̑�����΂����ŏ�������
			// �R�}���h���C���o�b�t�@�C���f�b�N�X���N���A
			this->buf_idx = 0;
			break;
		default:
			// �f�[�^���o�b�t�@���i�[
			this->buf[this->buf_idx++] = data;
			break;
	}
	
	return;
}

// �O�����J�֐�
// �R���\�[���^�X�N
int console_main(int argc, char *argv[])
{
	CONSOLE_CTL *this;
	uint8_t data[2];
	uint32_t ret;
	
	// ����u���b�N�̎擾
	this = &console_ctl;
	
	// �R���\�[���^�X�N�̏�����
	console_init();
	
	while (1) {
		// "command>"���o��
		if (this->buf_idx == 0) {
			console_str_send((uint8_t*)"command>");
		}
		// �R���\�[������̓��͂���M����
		data[0] = console_recv();
		data[1] = '\0';
		// ���s�R�[�h�ϊ�(\r��\n)
		if (data[0] == '\r') data[0] = '\n';
		// �G�R�[�o�b�N
		ret = console_str_send((uint8_t*)data);
		// ��M�f�[�^���
		console_analysis(data[0]);
	}
	
	return 0;
}

// �R���\�[���֕�����𑗐M����֐�
uint8_t console_str_send(uint8_t *data)
{
	int32_t ret;
	uint8_t len;
	
	// ������̒������擾
	len = strlen((char*)data);
	
	// ���M
	ret = usart_send(CONSOLE_USART_CH, data, len);
	
	return ret;
}

// �R�}���h��ݒ肷��֐�
int32_t console_set_command(COMMAND_INFO *cmd_info)
{
	CONSOLE_CTL *this;
	
	// �R�}���h�֐���NULL�̏ꍇ�G���[��Ԃ��ďI��
	if (cmd_info == NULL) {
		return -1;
	}
	
	// ����u���b�N�̎擾
	this = &console_ctl;
	
	// �����R�}���h�֐���o�^�ł��Ȃ��ꍇ�̓G���[��Ԃ��ďI��
	if (this->cmd_idx >= CONOLE_CMD_NUM) {
		return -1;
	}
	
	// �o�^
	this->cmd_info[this->cmd_idx].input = cmd_info->input;
	this->cmd_info[this->cmd_idx].func = cmd_info->func;
	this->cmd_idx++;
	
	return 0;
}
