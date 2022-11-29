/*
 * us_sensor.c
 *
 *  Created on: 2022/08/29
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "us_sensor.h"
//#include "ctl_main.h"
#include "consdrv.h"

// �}�N����`
#define US_SENSOR_NUM  (1U)
#define BUF_SIZE       (1U)
#define SPEED_OF_SOUND (34U)   // 34cm/ms
#define US_TSK_PERIOD  (1000U) // 1000ms

// ���
enum State_Type {
	ST_UNINITIALIZED = 0U,
	ST_INITIALIZED,
	ST_RUN,
	ST_STOP,
	ST_UNDEIFNED,
	ST_MAX,
};
// �C�x���g
enum EVENT_Type {
	EVENT_INIT = 0U,
	EVENT_RUN,
	EVENT_STOP,
	EVENT_MAX,
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
	kz_thread_id_t tsk_id;      // ���C���^�X�N��ID
	kz_msgbox_id_t msg_id;      // ���b�Z�[�WID
	uint8_t state;              // ���
	US_CALLBACK callback;       // �R�[���o�b�N
	uint8_t data[BUF_SIZE];     // �f�[�^�i�[�p�o�b�t�@(���g�p)
} US_SENSOR_CTL;
US_SENSOR_CTL us_sensor_ctl[US_SENSOR_NUM];

// �v���g�^�C�v�錾
static void us_init(void);
static void us_get_data_start(void);
static void us_get_data_stop(void);
static void us_get_data(void);
static uint8_t read_distance(void);

// ��ԑJ�ڃe�[�u��
static const FSM fsm[ST_MAX][EVENT_MAX] = {
		// EVENT_INIT               EVENT_RUN                    EVENT_STOP                   EVENT_MAX
		{{us_init, ST_INITIALIZED}, {NULL, ST_UNDEIFNED},        {NULL, ST_UNDEIFNED},        {NULL, ST_UNDEIFNED},}, // ST_UNINITIALIZED
		{{NULL, ST_UNDEIFNED},      {us_get_data_start, ST_RUN}, {NULL, ST_UNDEIFNED},        {NULL, ST_UNDEIFNED},}, // ST_INITIALIZED
		{{NULL, ST_UNDEIFNED},      {us_get_data, ST_RUN},       {us_get_data_stop, ST_STOP}, {NULL, ST_UNDEIFNED},}, // ST_RUN
		{{NULL, ST_UNDEIFNED},      {us_get_data_start, ST_RUN}, {NULL, ST_UNDEIFNED},        {NULL, ST_UNDEIFNED},}, // ST_STOP
};

// ���C���̃^�X�N
// ��ԑJ�ڃe�[�u���ɏ]���A���������s���A��Ԃ��X�V����
int US_main(int argc, char *argv[])
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	uint8_t cur_state = ST_UNINITIALIZED;
	uint8_t nxt_state = cur_state;
	US_MSG *msg;
	uint32_t size;
	FUNC func;

	// ���^�X�N��ID�𐧌�u���b�N�ɐݒ�
	this->tsk_id = kz_getid();
	// ���b�Z�[�WID�𐧌�u���b�N�ɐݒ�
	this->msg_id = MSGBOX_ID_US_MAIN;

	while(1){
		// ���b�Z�[�W��M
		kz_recv(this->msg_id, &size, &msg);

		// ����/����Ԃ��擾
		func = fsm[cur_state][msg->msg_type].func;
		nxt_state = fsm[cur_state][msg->msg_type].nxt_state;

		// msg�����
		kz_kmfree(msg);

		// ����Ԃ��X�V
		this->state = nxt_state;

		// ���������s
		if (func != NULL) {
			func();
		}

		// ��Ԃ��X�V
		if (nxt_state != ST_UNDEIFNED) {
			cur_state = nxt_state;
		}
	}

	return 0;
}

// �O�����J�֐�
// �������v�����b�Z�[�W�𑗐M����֐�
void US_MSG_init(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	US_MSG *msg;

	msg = kz_kmalloc(sizeof(US_MSG));

	msg->msg_type = EVENT_INIT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(US_MSG), msg);

	return;
}

// �f�[�^�擾�J�n���b�Z�[�W�𑗐M����֐�
void US_MSG_measure_start(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	US_MSG *msg;

	msg = kz_kmalloc(sizeof(US_MSG));

	msg->msg_type = EVENT_RUN;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(US_MSG), msg);

	return;
}

/* �f�[�^�擾���b�Z�[�W�𑗐M����֐� */
void US_MSG_get_data(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	US_MSG *msg;

	msg = kz_kmalloc(sizeof(US_MSG));

	msg->msg_type = EVENT_RUN;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(US_MSG), msg);

	return;
}

/* �f�[�^�擾��~���b�Z�[�W�𑗐M����֐� */
void US_MSG_measure_stop(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	US_MSG *msg;

	msg = kz_kmalloc(sizeof(US_MSG));

	msg->msg_type = EVENT_STOP;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(US_MSG), msg);

	return;
}

/* �����g�Z���T�̃f�[�^�擾���ɃR�[������R�[���o�b�N��ݒ肷��֐� */
uint32_t US_set_callback(US_CALLBACK callback)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	
	// �p�����[�^�`�F�b�N
	// callback��NULL�̏ꍇ�A-1��Ԃ��ďI��
	if (callback == NULL) {
		return -1;
	}

	// �R�[���o�b�N���܂��o�^����Ă��Ȃ��ꍇ
	if (this->callback == NULL) {
		// �R�[���o�b�N��o�^
		this->callback = callback;
	}
	
	return 0;
}

// �����֐�
// �������v�����b�Z�[�W��M���Ɏ��s����֐�
static void us_init(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* GPIO�̐ݒ� */
	__HAL_RCC_GPIOF_CLK_ENABLE();
	HAL_PWREx_EnableVddIO2();

	/* PF14(GPIO����) -> HC-SR04 Echo */
	/* PF15(GPIO�o��) -> HC-SR04 Trig */
	/*Configure GPIO pin : PF14 */
	GPIO_InitStruct.Pin = GPIO_PIN_14;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	/*Configure GPIO pin : PF15 */
	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_15, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	return;
}

// �f�[�^�擾�J�n���b�Z�[�W��M���Ɏ��s����֐�
static void us_get_data_start(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	uint32_t ret;
	
	// �V�X�e������@�\�ɁA1s�����Ńf�[�^�擾���b�Z�[�W�𑗐M���Ă��炤�悤�ɗv��
	ret = set_cyclic_message(US_MSG_get_data, US_TSK_PERIOD);
	
	return;
}

// �f�[�^�擾���b�Z�[�W��M���Ɏ��s����֐�
static void us_get_data(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	uint8_t data = 0;
	
	// �������擾
	data = read_distance();
	
	// �R�[���o�b�N���Ă�
	if (this->callback != NULL) {
		this->callback(data);
	}
	
	return;
}

// �����g�Z���T����f�[�^���擾����֐�
static uint8_t read_distance(void)
{
	uint8_t echo;
	uint32_t start_time_s;
	uint32_t end_time_s;
	uint32_t start_time_ms;
	uint32_t end_time_ms;
	uint32_t elapsed_time_ms;
	uint8_t distance;

	/* Trig��High */
	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_15, GPIO_PIN_SET);
	/* 1ms��wait */
	kz_tsleep(1);
	/* Trig��Low */
	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_15, GPIO_PIN_RESET);

	/* High�̊J�n���Ԃ��擾 */
	do{
		/* echo���擾 */
		echo = HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_14);
		/* �J�n�������擾 */
		start_time_ms = kz_get_time_ms();
		start_time_s = kz_get_time_s();
	}while(!echo);

	/* High�̏I�����Ԃ��擾 */
	do{
		/* echo���擾 */
		echo = HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_14);
		/* �J�n�������擾 */
		end_time_ms = kz_get_time_ms();
		end_time_s = kz_get_time_s();
	}while(echo);

	/* �J�n�����ƏI���������v�Z */
	start_time_ms = start_time_ms + start_time_s * 1000;
	end_time_ms = end_time_ms + end_time_s * 1000;

	/* �o�ߎ��Ԃ��Z�o */
	elapsed_time_ms = end_time_ms - start_time_ms;

	/* �������Z�o */
	distance = (uint8_t)((elapsed_time_ms * SPEED_OF_SOUND)/2);

	return distance;
}

// �f�[�^�擾��~���b�Z�[�W��M���Ɏ��s����֐�
static void us_get_data_stop(void)
{
	US_SENSOR_CTL *this = &us_sensor_ctl[0];
	uint32_t ret;

	// �V�X�e������@�\�ɁA1s�����̃f�[�^�擾���b�Z�[�W�̑��M���~���Ă��炤�悤�ɗv��
	ret = del_cyclic_message(US_MSG_get_data);

	return;
}






