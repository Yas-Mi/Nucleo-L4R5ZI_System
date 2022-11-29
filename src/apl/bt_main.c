/*
 * BT_main.c
 *
 *  Created on: 2022/09/18
 *      Author: User
 */
#include "bt_main.h"
#include "defines.h"
#include "kozos.h"
#include "consdrv.h"
#include "lib.h"
#include "bt_dev.h"

// �}�N����`
#define BT_NUM   (1U)
#define BUF_SIZE (32U)

// ���
enum State_Type {
	ST_UNINITIALIZED = 0U,
	ST_INITIALIZED,
	ST_CONNECTED,
	ST_DISCONNECTED,
	ST_UNDEIFNED,
	ST_MAX,
};

// �C�x���g
enum EVENT_Type {
	EVENT_INIT = 0U,
	EVENT_CONNECT,
	EVENT_RUN,
	EVENT_DISCONNECT,
	EVENT_GET_STATUS,
	EVENT_MAX,
};

// �֐��|�C���^
typedef int (*FUNC)(void);

// ��ԑJ�ڗp��`
typedef struct {
	FUNC func;
	uint8_t nxt_state;
}FSM;

// ��ԑJ�ڗp��`
typedef struct {
	uint8_t data[BUF_SIZE];
	uint8_t rd_idx;
	uint8_t wr_idx;
}BUF;

// ����p�u���b�N
typedef struct {
	BUF primary;   // �f�[�^�i�[�p�o�b�t�@
	BUF secondary; // �f�[�^�i�[�p�o�b�t�@
}TRANSFER_CTL;
typedef struct {
	kz_thread_id_t tsk_id;      // ���C���^�X�N��ID
	kz_thread_id_t tsk_data_id; // �f�[�^�擾�p�^�X�N��ID
	kz_msgbox_id_t msg_id;      // ���b�Z�[�WID
	uint8_t state;              // ���
	TRANSFER_CTL trans_ctl;
} BT_CTL;
BT_CTL bt_ctl[BT_NUM];

// �v���g�^�C�v�錾
static void bt_init();
static void bt_send_data();
static void bt_get_status();

// ��ԑJ�ڃe�[�u��
static const FSM fsm[ST_MAX][EVENT_MAX] = {
		// EVENT_INIT               EVENT_CONNECT         EVENT_RUN                      EVENT_DISCONNECT         EVENT_GET_STATUS
		{{bt_init, ST_INITIALIZED}, {NULL, ST_UNDEIFNED}, {NULL, ST_UNDEIFNED},          {NULL, ST_UNDEIFNED},    {bt_get_status, ST_UNINITIALIZED}, }, // ST_UNINITIALIZED
		{{NULL, ST_UNDEIFNED},      {NULL, ST_CONNECTED}, {bt_send_data, ST_UNDEIFNED},  {NULL, ST_DISCONNECTED}, {bt_get_status, ST_INITIALIZED}, },   // ST_INITIALIZED
		{{NULL, ST_UNDEIFNED},      {NULL, ST_CONNECTED}, {bt_send_data, ST_CONNECTED},  {NULL, ST_DISCONNECTED}, {bt_get_status, ST_CONNECTED}, },     // ST_CONNECTED
		{{NULL, ST_UNDEIFNED},      {NULL, ST_CONNECTED}, {NULL, ST_UNDEIFNED},          {NULL, ST_DISCONNECTED}, {bt_get_status, ST_DISCONNECTED}, },  // ST_DISCONNECTED
};

int BT_main(int argc, char *argv[])
{
	BT_CTL *this = &bt_ctl[0];
	uint8_t cur_state = ST_UNINITIALIZED;
	uint8_t nxt_state = cur_state;
	BT_MSG *msg;
	int32_t size;
	FUNC func;

	// ����u���b�N�̏�����
	memset(bt_ctl, 0, sizeof(bt_ctl));

	this->tsk_id = kz_getid();
	this->msg_id = MSGBOX_ID_BTMAIN;

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
/* ���������b�Z�[�W�𑗐M����֐� */
void BT_MSG_init(void)
{
	BT_CTL *this = &bt_ctl[0];
	BT_MSG *msg;

	msg = kz_kmalloc(sizeof(BT_MSG));

	msg->msg_type = EVENT_INIT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(BT_MSG), msg);

	return;
}

void BT_resp_callback(uint8_t data)
{

}

// �ڑ��ς݃��b�Z�[�W�𑗐M����֐�
void BT_MSG_connect(void)
{
	BT_CTL *this = &bt_ctl[0];
	BT_MSG *msg;

	// ����Ԃ�CONNECTED�̏ꍇ�A�C�x���g�𔭍s���Ȃ�
	if (this->state == ST_CONNECTED) {
		return;
	}

	msg = kz_kmalloc(sizeof(BT_MSG));

	msg->msg_type = EVENT_CONNECT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(BT_MSG), msg);

	return;
}

// �f�[�^���M�v�����b�Z�[�W�𑗐M����֐�
void BT_MSG_send_data(uint8_t *data, uint8_t size)
{
	BT_CTL *this = &bt_ctl[0];
	BT_MSG *msg;
	uint8_t i;

	msg = kz_kmalloc(sizeof(BT_MSG));

	// �����o�b�t�@�ɃR�s�[
	for (i = 0; i<size; i++) {
		this->trans_ctl.primary.data[this->trans_ctl.primary.wr_idx++] = data[i];
		this->trans_ctl.primary.wr_idx = this->trans_ctl.primary.wr_idx & (BUF_SIZE - 1);
	}

	msg->msg_type = EVENT_RUN;
	msg->msg_data = this->trans_ctl.primary.data;

	kz_send(this->msg_id, sizeof(BT_MSG), msg);

	return;
}

/* �f�[�^��M���b�Z�[�W�𑗐M����֐� */
void BT_MSG_receive_data(uint8_t *data, uint8_t size)
{
#if 0
	BT_CTL *this = &bt_ctl[0];
	uint8_t i;
	BT_MSG *msg;

	msg = kz_kmalloc(sizeof(BT_MSG));

	// �����o�b�t�@�ɃR�s�[
	for (i = 0; i<size; i++) {
		this->trans_ctl.rcv_data[i] = data[i];
	}

	msg->msg_type = EVENT_RUN;
	msg->msg_data = data;

	kz_send(this->msg_id, sizeof(BT_MSG), msg);
#endif
	return;
}

// ��ڑ����b�Z�[�W�𑗐M����֐�
void BT_MSG_disconnect(void)
{
	BT_CTL *this = &bt_ctl[0];
	BT_MSG *msg;

	// ����Ԃ�DISCONNECTED�̏ꍇ�A�C�x���g�𔭍s���Ȃ�
	if (this->state == ST_DISCONNECTED) {
		return;
	}

	msg = kz_kmalloc(sizeof(BT_MSG));

	msg->msg_type = EVENT_DISCONNECT;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(BT_MSG), msg);

	return;
}

// ��Ԏ擾���b�Z�[�W�𑗐M����֐�
void BT_MSG_get_status(void)
{
	BT_CTL *this = &bt_ctl[0];
	BT_MSG *msg;

	msg = kz_kmalloc(sizeof(BT_MSG));

	msg->msg_type = EVENT_GET_STATUS;
	msg->msg_data = NULL;

	kz_send(this->msg_id, sizeof(BT_MSG), msg);

	return;
}

// �����֐�
// �������v�����b�Z�[�W��M���Ɏ��s����֐�
static void bt_init(void)
{
	// BlueTooth�̏�����
	bt_dev_send_use(SERIAL_BLUETOOTH_DEVICE);

	return;
}

//�@�f�[�^���M�v�����b�Z�[�W��M���Ɏ��s����֐�
static void bt_send_data(void)
{
	BT_CTL *this = &bt_ctl[0];
	uint8_t val[3];
	char data[3];

	// 3���ڂ̐������擾
	val[2] = this->trans_ctl.primary.data[this->trans_ctl.primary.rd_idx] / 100;
	// 2���ڂ̐������擾
	val[1] = (this->trans_ctl.primary.data[this->trans_ctl.primary.rd_idx] - val[2]*100) / 10 ;
	// 1���ڂ̐������擾
	val[0] = (this->trans_ctl.primary.data[this->trans_ctl.primary.rd_idx] - val[2]*100 - val[1]*10);

	// �o�b�t�@�̃��[�h�C���f�b�N�X���X�V
	this->trans_ctl.primary.rd_idx++;
	this->trans_ctl.primary.rd_idx = this->trans_ctl.primary.rd_idx & (BUF_SIZE - 1);

	// �A�X�L�[�R�[�h�ɕϊ�
	data[0] = val[2] + 0x30;
	data[1] = val[1] + 0x30;
	data[2] = val[0] + 0x30;

	// BlueTooth�o�R��PC�֑��M
	bt_dev_send_write(data);

	return;
}

//�@��Ԏ擾���b�Z�[�W��M���Ɏ��s����֐�
void bt_get_status(void)
{
	BT_CTL *this = &bt_ctl[0];

	switch(this->state){
	case ST_UNINITIALIZED:
		consdrv_send_write("ST_UNINITIALIZED\n");
		break;
	case ST_INITIALIZED:
		consdrv_send_write("ST_INITIALIZED\n");
		break;
	case ST_CONNECTED:
		consdrv_send_write("ST_CONNECTED\n");
		break;
	case ST_DISCONNECTED:
		consdrv_send_write("ST_DISCONNECTED\n");
		break;
	case ST_UNDEIFNED:
		consdrv_send_write("ST_UNDEIFNED\n");
		break;
	}

	return;
}
