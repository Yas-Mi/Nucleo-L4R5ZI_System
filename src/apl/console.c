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
	kz_thread_id_t		tsk_id;						// �^�X�NID
	kz_msgbox_id_t		msg_id;						// ���b�Z�[�WID
	char				buf[CONOLE_BUF_SIZE];		// �R�}���h���C���o�b�t�@
	uint8_t				buf_idx;					// �R�}���h���C���o�b�t�@�C���f�b�N�X
	COMMAND_INFO		cmd_info[CONOLE_CMD_NUM];	// �R�}���h�֐�
	uint8_t				cmd_idx;					// �R�}���h�֐��C���f�b�N�X
} CONSOLE_CTL;
static CONSOLE_CTL console_ctl;


// ���l����ASCII�ɕϊ����鏈��
static uint8_t val2ascii(uint8_t val)
{
	if (val > 9) {
		return 0xFF;
	}
	
	return 0x30+val;
}

// �R���\�[������̓��͂���M����֐�
static uint8_t console_recv(void)
{
	uint8_t data;
	int32_t size;
	
	// ��M�ł���ő҂�
	size = usart_recv(CONSOLE_USART_CH, &data, 1);
	// ���҂����T�C�Y�ǂ߂��H
	if (size != 1) {
		; // ���ɉ������Ȃ� ���Ԃ���҂����T�C�Y�ǂ߂�ł��傤
	}
	
	return data;
}

// �R���\�[������̓��͂���M����֐�
static void console_analysis(uint8_t data)
{
	CONSOLE_CTL *this = &console_ctl;
	COMMAND_INFO *cmd_info;
	uint8_t i;
	
	switch (data) {
		case '\t':	// tab
			// �R�}���h�̈ꗗ��\��
			console_str_send((uint8_t*)"\n");
			for (i = 0; i < this->cmd_idx; i++) {
				cmd_info = &(this->cmd_info[i]);
				console_str_send((uint8_t*)cmd_info->input);
				console_str_send((uint8_t*)"\n");
			}
			console_str_send((uint8_t*)"\n");
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
				if (!strcmp(this->buf, cmd_info->input)) {
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

// �R���\�[���^�X�N
static int console_main(int argc, char *argv[])
{
	CONSOLE_CTL *this = &console_ctl;
	uint8_t data[2];
	uint32_t ret;
	
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

// �O�����J�֐�
void console_init(void)
{
	CONSOLE_CTL *this;
	int32_t ret;
	
	// ����u���b�N�̎擾
	this = &console_ctl;
	
	// ����u���b�N�̏�����
	memset(this, 0, sizeof(CONSOLE_CTL));
	
	// �^�X�N�̐���
	this->tsk_id = kz_run(console_main, "console",  CONZOLE_PRI, CONSOLE_STACK, 0, NULL);
	
	// ���b�Z�[�WID�̐ݒ�
	this->msg_id = MSGBOX_ID_CONSOLE;

	// USART�̃I�[�v��
	ret = usart_open(CONSOLE_USART_CH, CONSOLE_BAUDRATE);
	
	return;
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

// �R���\�[���֐��l�𑗐M����֐�
uint8_t console_val_send(uint8_t data)
{
	int32_t ret;
	uint8_t len;
	uint8_t hundreds, tens_place, ones_place;
	uint8_t snd_data[4];
	
	// ������
	memset(snd_data, '\0', sizeof(snd_data));
	
	// ���������߂�
	hundreds = data/100;
	tens_place = (data - hundreds * 100)/10;
	ones_place = (data - hundreds * 100 - tens_place * 10);
	
	// 3���H
	if (hundreds != 0) {
		len = 4;
		snd_data[0] = val2ascii(hundreds);
		snd_data[1] = val2ascii(tens_place);
		snd_data[2] = val2ascii(ones_place);
	// 2��?
	} else if (tens_place != 0) {
		len = 3;
		snd_data[0] = val2ascii(tens_place);
		snd_data[1] = val2ascii(ones_place);
	// 1���H
	} else {
		len = 2;
		snd_data[0] = val2ascii(ones_place);
	}
	
	// ���M
	ret = console_str_send(snd_data);
	
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
