/*
 * ctl_main.c
 *
 *  Created on: 2022/09/22
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "cyc.h"

// ���
#define ST_NOT_INITIALIZED	(0)		// ������
#define ST_INITIALIZED		(1)		// �������ς�

// �}�N����`
#define	CYC_TASK_PERIOD			(5U)	//!< �������b�Z�[�W�𑗐M����^�X�N�̎���[ms]
#define	QUE_NUM					(1U)	//!< �L���[�̐�

// ���X�g
typedef struct lst {
	struct lst *next;
	CYC_MSG cyc_msg;
	uint32_t period;
	uint32_t remain_period;
} LIST;

// �������b�Z�[�W�Ǘ��p�̃L���[
static struct {
  LIST *head;
  LIST *tail;
} cycmsg_que[QUE_NUM];

// ����p�u���b�N
typedef struct {
	uint32_t state;				// ���
	kz_thread_id_t tsk_id;      // ���C���^�X�N��ID
} CYC_CTL;
static CYC_CTL cyc_ctl;		// ����u���b�N�̎���

/*!
 @brief �������b�Z�[�W�𑗐M����^�X�N
 @param [in] argc
 @param [in] *argv[]
 */
int cycmsg_main(int argc, char *argv[])
{
	LIST *node;
	LIST *prev_node;
	
	while(1){
		// TASK_PERIOD�̊��ԃX���[�v
		kz_tsleep(CYC_TASK_PERIOD);

		node = cycmsg_que[0].head;
		prev_node = cycmsg_que[0].head;

		// �R�[������^�C�~���O�̎������b�Z�[�W������
		while (node != NULL) {
			// �w�肳�ꂽ�����ȉ��ɂȂ��Ă���ꍇ
			if (node->remain_period <= CYC_TASK_PERIOD) {
				// �w�肳�ꂽ�������b�Z�[�W���R�[��
				node->cyc_msg();
				// ���X�g����폜
				//prev_node->next = node->next;
				// �m�[�h���폜
				//kz_kmfree(node);
				node->remain_period = node->period;
			// �w�肳�ꂽ�����ɂȂ��Ă��Ȃ��ꍇ
			} else {
				// �w�肳�ꂽ��������^�X�N�̎���������
				node->remain_period -= CYC_TASK_PERIOD;
			}
			// �X�V
			prev_node = node;
			node = node->next;
		}
	}
	
	return 0;
}

/*! 
 @brief �������b�Z�[�W��o�^����
 @param [in] cyclic_message �������b�Z�[�W
 @param [in] period         ����
 @return     -1             �o�^�ł��Ȃ�����
 @return      0             �o�^�ł���
*/
uint32_t set_cyclic_message(CYC_MSG cyclic_message, uint32_t period)
{
	LIST *new_node;
	LIST *node;
	
	// �p�����[�^�`�F�b�N
	// �������b�Z�[�W��NULL�̏ꍇ�A�G���[��Ԃ��ďI��
	if (cyclic_message == NULL) {
		return -1;
	}
	
	// �V�����m�[�h���쐬
	new_node = kz_kmalloc(sizeof(LIST));
	
	// �������b�Z�[�W�Ǝ�����ݒ�
	new_node->cyc_msg       = cyclic_message;
	new_node->period        = period;
	new_node->remain_period = period;
	new_node->next          = NULL;
	
	if (cycmsg_que[0].tail) {
		cycmsg_que[0].tail->next = new_node;
	} else {
		cycmsg_que[0].head = new_node;
	}
	cycmsg_que[0].tail = new_node;
	
	return 0;
}

/*! 
 @brief �������b�Z�[�W���폜
 @param [in] cyclic_message �������b�Z�[�W
 @param [in] period         ����
 @return     -1             �o�^�ł��Ȃ�����
 @return      0             �o�^�ł���
*/
uint32_t del_cyclic_message(CYC_MSG cyclic_message)
{
	LIST **prev_node;
	LIST **node;

	// �p�����[�^�`�F�b�N
	// �������b�Z�[�W��NULL�̏ꍇ�A�G���[��Ԃ��ďI��
	if (cyclic_message == NULL) {
		return -1;
	}

	// ����
	node = &(cycmsg_que[0].head);
	prev_node = &(cycmsg_que[0].head);
	while (*node != NULL) {
		if ((*node)->cyc_msg == cyclic_message) {
			break;
		}
		prev_node = node;
		node = &(cycmsg_que[0].head->next);
	}

	if (*node == NULL) {
		return -1;
	}

	// ���X�g����폜
	*prev_node = (*node)->next;

	return 0;
}

/*!
 @brief �������֐�
 @param [in] �Ȃ�
 @return     �Ȃ�
*/
void cyc_init(void)
{
	CYC_CTL *this = &cyc_ctl;
	int32_t ret;
	
	// ����u���b�N�̏�����
	memset(this, 0, sizeof(CYC_CTL));
	
	// �^�X�N�̐���
	this->tsk_id = kz_run(cycmsg_main, "cyc_main",  CYC_PRI, CYC_STACK, 0, NULL);
	
	// ��Ԃ̍X�V
	this->state = ST_INITIALIZED;
}
