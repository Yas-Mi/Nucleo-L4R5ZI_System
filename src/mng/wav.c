/*
 * wave.c
 *
 *  Created on: 2022/12/12
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "wav.h"
#include "lib.h"

// ���
#define ST_NOT_INTIALIZED	(0U) // ��������
#define ST_INITIALIZED		(1U) // �������ς�
#define ST_OPEND			(2U) // �I�[�v���ς�
#define ST_FMT_ANLYSING		(3U) // �t�H�[�}�b�g��͒�
#define ST_DATA_RECEIVING	(4U) // ���y�f�[�^��M��

// �}�N��
#define WAVE_FOMART_ERR			(-1)	// �t�H�[�}�b�g�G���[
#define WAVE_FOMART_OK			(0)		// �t�H�[�}�b�gOK
#define WAVE_FOMART_ANALYSING	(1)		// �t�H�[�}�b�g��͒�

// WAVE�t�@�C���w�b�_���
typedef union {
	uint8_t data[44];
	struct {
		uint8_t riff_str[4];		// RIFF
		uint8_t size1[4];			// ���t�@�C���T�C�Y-8[byte]
		uint8_t wave_str[4];		// WAVE
		uint8_t fmt_str[4];			// fmt
		uint8_t fmt_size[4];		// �t�H�[�}�b�g�T�C�Y
		uint8_t fmt_code[2];		// �t�H�[�}�b�g�R�[�h
		uint8_t ch_num[2];			// �`�����l���� ���m������1�A�X�e���I��2
		uint8_t sampling_rate[4];	// �T���v�����O���[�g
		uint8_t byte_par_sec[4];	// �o�C�g/�b
		uint8_t blk_align[2];		// �u���b�N���E
		uint8_t bit_par_sample[2];	// �r�b�g/�T���v��
		uint8_t data_str[4];		// �f�[�^
		uint8_t size2[4];			// ���t�@�C���T�C�Y-126[byte]
	} info;
} WAV_HEADER;

// ����p�u���b�N
typedef struct {
	uint8_t status;					// ���
	WAV_RCV_CALLBACK rcv_callback;	// ��M�R�[���o�b�N
	WAV_END_CALLBACK end_callback;	// �I���R�[���o�b�N
	void *callback_vp;				// �R�[���o�b�N�p�����[�^
	WAV_HEADER rcv_buf;				// ��M�o�b�t�@
	uint32_t rcv_cnt;				// ��M�����T�C�Y
	uint32_t wave_data_size;		// �S�f�[�^�T�C�Y[byte]
	uint16_t sample_data_size;		// 1�f�[�^�T�C�Y[byte]
	uint32_t sample_data_cnt;		// ���ݎ�M����wave�̃f�[�^�T�C�Y[byte]
	int32_t	sample_data;			// �T���v���f�[�^	(*) �Ƃ肠����4byte
} WAV_CTL;
static WAV_CTL wav_ctl;

// �f�[�^���
static int32_t data_analysis(uint8_t data)
{
	WAV_CTL *this = &wav_ctl;
	
	// �f�[�^�i�[
	this->rcv_buf.data[this->rcv_cnt++] = data;
	
	// 4byte��M��
	if (this->rcv_cnt == 4) {
		// RIFF?
		if (memcmp(this->rcv_buf.info.riff_str, "RIFF", 4)) {
			this->rcv_cnt = 0;
			return WAVE_FOMART_ERR;
		}
	// 8byte��M��
	} else if (this->rcv_cnt == 8) {
		// �S�f�[�^�T�C�Y���i�[
		this->wave_data_size = ((this->rcv_buf.info.size1[0] << 0) | 
								(this->rcv_buf.info.size1[1] << 8) |
								(this->rcv_buf.info.size1[2] << 16 ) |
								(this->rcv_buf.info.size1[3] << 24 )) + 8 - sizeof(WAV_HEADER);
	// 12byte��M��
	} else if (this->rcv_cnt == 12) {
		// WAVE?
		if (memcmp(this->rcv_buf.info.wave_str, "WAVE", 4)) {
			this->rcv_cnt = 0;
			return WAVE_FOMART_ERR;
		}
	// 16byte��M��
	} else if (this->rcv_cnt == 16) {
		// fmt ?
		if (memcmp(this->rcv_buf.info.fmt_str, "fmt ", 4)) {
			this->rcv_cnt = 0;
			return WAVE_FOMART_ERR;
		}
	// 20byte��M��
	} else if (this->rcv_cnt == 20) {
		// �t�H�[�}�b�g�T�C�Y
		// �f�t�H���g��16
	// 22byte��M��
	} else if (this->rcv_cnt == 22) {
		// �t�H�[�}�b�g�R�[�h
		// �񈳏k��PCM�t�H�[�}�b�g��1
	// 24byte��M��
	} else if (this->rcv_cnt == 24) {
		// �`���l����
		// ���m������1�A�X�e���I��2
	// 28byte��M��
	} else if (this->rcv_cnt == 28) {
		// �T���v�����O���[�g
		// 44.1kHz�̏ꍇ�Ȃ�44100
	// 32byte��M��
	} else if (this->rcv_cnt == 32) {
		// �o�C�g�^�b
		// �P�b�Ԃ̘^���ɕK�v�ȃo�C�g��
	// 34byte��M��
	} else if (this->rcv_cnt == 34) {
		// �u���b�N���E
		// �X�e���I16bit�Ȃ�A16bit*2 = 32bit = 4byte
	// 36byte��M��
	} else if (this->rcv_cnt == 36) {
		// �r�b�g�^�T���v��
		// �P�T���v���ɕK�v�ȃr�b�g��
		this->sample_data_size = ((this->rcv_buf.info.bit_par_sample[1] << 8)|(this->rcv_buf.info.bit_par_sample[0] << 0))/8;
	// 40byte��M��
	} else if (this->rcv_cnt == 40) {
		// data?
		if (memcmp(this->rcv_buf.info.data_str, "data ", 4)) {
			this->rcv_cnt = 0;
			return WAVE_FOMART_ERR;
		}
	} else if (this->rcv_cnt == 44) {
		// �f�[�^�T�C�Y���i�[
		// (*) -128���Ȃ��Ƃ����Ȃ��炵��
		// �ŏ��ɂ����f�[�^�T�C�Y�͊i�[�����̂ŉ��߂Ă����ł͉������Ȃ�
		return WAVE_FOMART_OK;
	} else {
		;
	}
	return WAVE_FOMART_ANALYSING;
}

// �R�[���o�b�N
static void bt_dev_callback(uint8_t data, void *vp)
{
	WAV_CTL *this = (WAV_CTL*)vp;
	uint8_t byte_pos;
	int32_t ret;
	
	// �I�[�v���ς݁A��͒��A�f�[�^��M���łȂ���Ή������Ȃ�
	if ((this->status != ST_OPEND) &&
		(this->status != ST_FMT_ANLYSING) &&
		(this->status != ST_DATA_RECEIVING)) {
		return;
	}
	
	// �f�[�^���
	if (this->status != ST_DATA_RECEIVING) {
		// ���
		ret = data_analysis(data);
		// �t�H�[�}�b�g��͒��H
		if (ret == WAVE_FOMART_ANALYSING) {
			// ��Ԃ��t�H�[�}�b�g��͒��ɍX�V
			this->status = ST_FMT_ANLYSING;
		// �t�H�[�}�b�g�G���[�H
		} else if (ret == WAVE_FOMART_ERR) {
			// ��Ԃ��I�[�v����ԂɍX�V
			this->status = ST_OPEND;
		// �t�H�[�}�b�g���������H
		} else if (ret == WAVE_FOMART_OK) {
			// ��Ԃ��f�[�^��M���ɍX�V
			this->status = ST_DATA_RECEIVING;
		} else {
			;
		}
		return;
	}
	
	// �f�[�^���i�[
	byte_pos = (this->sample_data_cnt++) % this->sample_data_size;
	this->sample_data |= (data << (byte_pos * 8));
	
	// 1�T���v�����܂���?
	if (byte_pos == (this->sample_data_size - 1)) {
		// �R�[���o�b�N
		if (this->rcv_callback != NULL) {
			this->rcv_callback(this->sample_data, this->callback_vp);
		}
		// �f�[�^�N���A
		this->sample_data = 0;
	}
	
	// �S�f�[�^��M���������H
	if (this->sample_data_cnt >= this->wave_data_size) {
		// �R�[���o�b�N
		if (this->end_callback != NULL) {
			this->end_callback(this->callback_vp);
		}
		// �������̃N���A
		memset(&(this->rcv_buf), 0, sizeof(WAV_HEADER));
		this->rcv_cnt = 0;
		this->wave_data_size = 0;
		this->sample_data_size = 0;
		this->sample_data_cnt = 0;
		// ��Ԃ��X�V
		this->status = ST_OPEND;
	}
	
	return;
}

// �O�����J�֐�
// ����������
void wav_init(void)
{
	WAV_CTL *this = &wav_ctl;
	
	// ����u���b�N�̏�����
	memset(this, 0, sizeof(WAV_CTL));
	
	// ��Ԃ��������ς݂ɍX�V
	this->status = ST_INITIALIZED;
	
	return;
}

// �I�[�v������
int32_t wav_open(WAV_RCV_CALLBACK rcv_callback, WAV_END_CALLBACK end_callback, void *callback_vp)
{
	WAV_CTL *this = &wav_ctl;
	int32_t ret;
	
	// �R�[���o�b�N�o�^
	ret = bt_dev_reg_callback(bt_dev_callback, this);
	if (ret != 0) {
		return -1;
	}
	
	// �R�[���o�b�N�o�^
	this->rcv_callback = rcv_callback;
	this->end_callback = end_callback;
	this->callback_vp = callback_vp;
	
	// ��Ԃ��I�[�v���ς݂ɍX�V
	this->status = ST_OPEND;
	
	return 0;
}
