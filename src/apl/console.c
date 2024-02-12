#include <console.h>
#include <string.h>
#include <stdio.h>
#include "str_util.h"
#include "usart.h"
#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
//#include "lib.h"

#define CONOLE_BUF_SIZE		(128U)		// �R�}���h���C���o�b�t�@�T�C�Y
#define CONOLE_CMD_NUM		(32U)		// �ݒ�ł���R�}���h�̐�
#define CONSOLE_USART_CH	USART_CH1	// �g�p����USART�̃`���l��
#define CONSOLE_BAUDRATE	(115200)	// �R���\�[���̃{�[���[�g
#define CONSOLE_ARG_MAX		(10)		// �����̌��̍ő�l

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
	uint8_t i, j;
	uint8_t argc = 0;
	char *argv[CONSOLE_ARG_MAX];
	uint8_t base_pos = 0;
	uint8_t sp_pos = 0;
	
	switch (data) {
		case '\t':	// tab
			// �R�}���h�̈ꗗ��\��
			console_str_send("\n");
			for (i = 0; i < this->cmd_idx; i++) {
				cmd_info = &(this->cmd_info[i]);
				console_str_send((uint8_t*)cmd_info->input);
				console_str_send("\n");
			}
			console_str_send("\n");
			break;
		case '\b':	// back space
			break;
		case '\n':	// Enter
			// NULL������ݒ�
			this->buf[this->buf_idx++] = '\0';
			// �R�}���h�ɐݒ肳��Ă���H
			for (i = 0; i < this->cmd_idx; i++) {
				cmd_info = &(this->cmd_info[i]);
				// �R�}���h������v����
				if (memcmp(this->buf, cmd_info->input, strlen(cmd_info->input)) == 0) {
					// ������� (*) �擪�ɋ󔒂͐�΂ɓ���Ȃ����ƁI�I
					for (j = 0; j < CONSOLE_ARG_MAX; j++) {
						// �󔒂�����
						sp_pos = find_str(' ', &(this->buf[base_pos]));
						// �����ݒ�
						argv[argc++] = &(this->buf[base_pos]);
						// �󔒂��Ȃ�����
						if (sp_pos == 0) {
							break;
						}
						// �󔒂�NULL�����ɐݒ�
						this->buf[base_pos+sp_pos] = '\0';
						// �����J�n�ʒu���X�V 
						base_pos += sp_pos + 1;
					}
					// �R�}���h���s
					cmd_info->func(argc, argv);
					break;
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
			console_str_send("command>");
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
	
	// ����u���b�N�̎擾
	this = &console_ctl;
	
	// ����u���b�N�̏�����
	memset(this, 0, sizeof(CONSOLE_CTL));
	
	// �^�X�N�̐���
	this->tsk_id = kz_run(console_main, "console",  CONZOLE_PRI, CONSOLE_STACK, 0, NULL);
	
	// ���b�Z�[�WID�̐ݒ�
	this->msg_id = MSGBOX_ID_CONSOLE;

	// USART�̃I�[�v��
	usart_open(CONSOLE_USART_CH, CONSOLE_BAUDRATE);
	
	return;
}

// �R���\�[���֕�����𑗐M����֐�
uint8_t console_str_send(char *data)
{
	int32_t ret;
	uint8_t len;
	
	// ������̒������擾
	len = strlen((char*)data);
	
	// ���M
	ret = usart_send(CONSOLE_USART_CH, data, len);
	
	return ret;
}

// �R���\�[���֐��l�𑗐M����֐�(8bit��)
uint8_t console_val_send(uint8_t data)
{
	int32_t ret;
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
		snd_data[0] = val2ascii(hundreds);
		snd_data[1] = val2ascii(tens_place);
		snd_data[2] = val2ascii(ones_place);
	// 2��?
	} else if (tens_place != 0) {
		snd_data[0] = val2ascii(tens_place);
		snd_data[1] = val2ascii(ones_place);
	// 1���H
	} else {
		snd_data[0] = val2ascii(ones_place);
	}
	
	// ���M
	ret = console_str_send(snd_data);
	
	return ret;
}

// �R���\�[���֐��l�𑗐M����֐�(16bit��)
uint8_t console_val_send_u16(uint16_t data)
{
	const uint8_t digit_max = 5;		// �����̍ő�l (*) 16bit�̍ő�l��66535��5��
	uint8_t snd_data[6];
	uint8_t digit = 0;
	uint16_t mul = 1;
	uint16_t val = 1;
	uint8_t idx = 0;
	int8_t i;
	int32_t ret;
	
	// ������
	memset(snd_data, '\0', sizeof(snd_data));
	
	// �����擾�̂��߂̒l���v�Z
	i = digit_max - 1;
	while (i != 0) {
		mul = mul * 10;
		i--;
	}
	
	// �e���l���A�X�L�[�ɂ���
	for (i = (digit_max - 1); i >= 0; i--) {
		// �e���̐��l���擾
		val = data / mul;
		// ���m��
		if ((val != 0) && (digit == 0)) {
			digit = i + 1;
		}
		// ���m�肵���H
		if ((digit != 0) || (i == 0)) {
			snd_data[idx] = val2ascii(val);
			data -= val * mul;
			idx++;
		}
		// 10�Ŋ���
		mul /= 10;
	}
	// ���M
	ret = console_str_send(snd_data);
	
	return ret;
}

// �R���\�[���֐��l(16�i��)�𑗐M����֐�(8bit��)
// (*) https://qiita.com/kaiyou/items/93af0fe0d49ff21fe20a
int32_t console_val_send_hex(uint8_t data, uint8_t digit)
{
	int32_t i, j;
	uint8_t temp;
	int32_t num[10];
	char answer[10];
	char snd_data[10];
	int32_t ret;
	
	if (digit == 0) {
		return -1;
	}
	
	//�e���̒l�����߂�inum[0]��1�̈ʁj
	i = 0;
	temp = data;
	do {
		num[i] = temp % 16;
		temp = temp /16;
		i++;
	} while (temp != 0);
	
	//�l��10�ȏ�ɂȂ錅��A~F�ɕϊ�����ianswer[i-1]��1�̈ʁj
	for (j = 0; i > j; j++) {
		if (num[i - j - 1] > 9) {
			answer[j] = num[i - j - 1] + 'A' - 10;
		} else {
			answer[j] = '0' + num[i - j - 1];
		}
		answer[j + 1] = '\0';
	}
	
	// ��������
	memset(snd_data, '0', sizeof(snd_data));
	// (*) i��2�ȏ�ɂȂ�Ȃ����ߔ͈͂𒴂��ăR�s�[���邱�Ƃ͂Ȃ�
	memcpy(&(snd_data[digit - i]), &(answer[0]), (i+1));
	
	// ���M
	ret = console_str_send(snd_data);
	
	return ret;
}

// �R���\�[���֐��l(16�i��)�𑗐M����֐�(32bit��)
int32_t console_val_send_hex_ex(uint32_t data, uint8_t digit)
{
	char tmp_data[16];
	char snd_data[8+1];
	int len;
	int8_t sft_size;
	int32_t ret;
	
	// 8���傫�����͈Ӗ��킩���
	if (digit > 8) {
		return -1;
	}
	
	// ������
	memset(snd_data, '0', sizeof(snd_data));
	
	// 16�i���ɕϊ�
	len = sprintf(tmp_data, "%x", data);
	
	// �V�t�g���鐔���v�Z
	sft_size = digit - len;
	
	// ���M�f�[�^���쐬
	if (sft_size >= 0) {
		memcpy(&(snd_data[sft_size]), tmp_data, len);
		snd_data[digit] = '\0';
	} else {
		memcpy(snd_data, tmp_data, len);
		snd_data[len] = '\0';
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

// �����݂ŕ�������o���֐�
void console_output_for_int(char *str)
{
	uint8_t len;
	
	// ������̒������擾
	len = strlen((char*)str);
	
	// ���M
	usart_send_for_int(CONSOLE_USART_CH, str, len);
}
