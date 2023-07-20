/*
 * ctl_main.c
 *
 *  Created on: 2022/09/22
 *      Author: User
 */
#include "defines.h"
#include "kozos.h"
#include "console.h"
#include "bt_dev.h"
#include "btn_dev.h"
#include "lcd_app.h"
#include "ctl_main.h"

// �}�N����`
#define	CYC_TASK_PERIOD			(10U)	//!< �������b�Z�[�W�𑗐M����^�X�N�̎���[ms]
#define	CTL_TASK_EVENT_PERIOD	(100U)	//!< ����^�X�N�̃C�x���g���M����[ms]
#define	QUE_NUM					(1U)	//!< �L���[�̐�
#define MODE_SELECT_TIME		(1000U)	//!< �N����ʂ��烂�[�h�Z���N�g��ʂ܂ł̎���

// ���
enum State_Type {
	ST_UNINITIALIZED = 0U,	//!< �������
	ST_START_UP,			//!< �N�����
	ST_SELECT_MODE,	        //!< ���[�h�I�����
	ST_RUN,					//!< ���쒆
	ST_UNDEIFNED,			//!< ����`
	ST_MAX,					//!< �ő�l
};

// ���b�Z�[�W
enum MSG_Type {
	MSG_INIT = 0U,		//!< �������v�����b�Z�[�W
	MSG_EVENT,			//!< �C�x���g�������b�Z�[�W
	MSG_MAX,		//!< �ő�l
};

// �֐��|�C���^
typedef void (*INIT_TSK)(void);

// ���X�g
typedef struct lst {
	struct lst *next;
	CYC_MSG cyc_msg;
	uint32_t period;
	uint32_t remain_period;
} LIST;

// ��ԑJ�ڗp��`
typedef struct {
	CTL_FUNC func;
	uint8_t  nxt_state;
}FSM;

// ����p�u���b�N
typedef struct {
	kz_thread_id_t tsk_id;      // ���C���^�X�N��ID
	kz_msgbox_id_t msg_id;      // ���b�Z�[�WID
	uint8_t state;              // ���
	uint8_t req_chg_sts;		// ��ԕύX�v���t���O
	uint8_t mode_select_timer;  // �N����ʂ��烂�[�h�Z���N�g��ʂ܂ł̎��Ԃ��J�E���g���邽�߂̃^�C�}�[
} CTL_CTL;

// �������b�Z�[�W�Ǘ��p�̃L���[
static struct {
  LIST *head;
  LIST *tail;
} cycmsg_que[QUE_NUM];

// �^�X�N�������֐��e�[�u��
static const INIT_TSK init_tsk_func[] =
{
	//US_MSG_init,    // �����g�f�[�^�擾�^�X�N�������֐�
	LCD_APP_MSG_init, // LCD�A�v���^�X�N�������֐�
};

// �v���g�^�C�v�錾
static void init_apl_tsk(void* info);
static void ctl_disp_start_up(void* info);
static void ctl_disp_mode_select(void* info);
static void ctl_btn(void* info);

// ��ԑJ�ڃe�[�u��
static const FSM fsm[ST_MAX][MSG_MAX] = {
	// MSG_INIT                    MSG_EVENT								 MSG_MAX
	{{init_apl_tsk, ST_START_UP}, {NULL, ST_UNDEIFNED},  					{NULL, ST_UNDEIFNED}, },	// ST_UNINITIALIZED
	{{NULL, ST_UNDEIFNED},        {ctl_disp_start_up, ST_SELECT_MODE}, 		{NULL, ST_UNDEIFNED}, },	// ST_START_UP
	{{NULL, ST_UNDEIFNED},        {ctl_disp_mode_select, ST_SELECT_MODE},	{NULL, ST_UNDEIFNED}, },	// ST_SELECT_MODE
	{{NULL, ST_UNDEIFNED},        {NULL, ST_UNDEIFNED},						{NULL, ST_UNDEIFNED}, },	// ST_RUN
};

// ����u���b�N�̎���
static CTL_CTL ctl_ctl;

// �����֐�
// ��ԕύX�֐�
static void ctl_main_chg_sts(uint8_t req_sts) 
{
	CTL_CTL *this = &ctl_ctl;
	
	// ��Ԃ��X�V����
	this->state = req_sts;
	this->req_chg_sts = 1U;
}

// �N����ʕ\���֐�
static void ctl_disp_start_up(void* info) 
{
	// LCD�A�v���ɋN����ʕ\�����b�Z�[�W�𑗂�
	LCD_APP_MSG_start_up();
}

// ���[�h�Z���N�g�\���֐�
static void ctl_disp_mode_select(void* info) 
{
	CTL_CTL *this = &ctl_ctl;
	
	// �N����ʂ��烂�[�h�Z���N�g��ʂ܂ł̎��Ԍo�߂����Ƃ��Ƀ��[�h�Z���N�g�\�����b�Z�[�W�𑗂�
	if (this->mode_select_timer > (MODE_SELECT_TIME/CTL_TASK_EVENT_PERIOD)) {
		// ��Ԃ��X�V����
		ctl_main_chg_sts(ST_RUN);
		// 100ms�̃C�x���g���폜
		del_cyclic_message(CTL_MSG_event);
		// ���b�Z�[�W�𑗐M
		LCD_APP_MSG_select_mode();
	} else {
		// �J�E���^��i�߂�
		this->mode_select_timer++;
	}
}

/*!
 @brief �S�^�X�N������������
 @param [in] argc
 @param [in] *argv[]
 */
static void init_apl_tsk(void* info)
{
	uint8_t i;
	uint8_t size;
	uint32_t ret;
	
	// �T�C�Y���v�Z
	size = sizeof(init_tsk_func)/sizeof(init_tsk_func[0]);
	
	// �^�X�N�̏������֐����R�[��
	for (i = 0; i < size; i++) {
		init_tsk_func[i]();
	}
	
	// ���g�̃^�X�N��100ms�����œ��삷��悤�ɐݒ�
	ret = set_cyclic_message(CTL_MSG_event, CTL_TASK_EVENT_PERIOD);
	
	return;
}

/*!
 @brief �V�X�e���𐧌䂷�邽�߂̃^�X�N
 @param [in] argc
 @param [in] *argv[]
 */
int ctl_main(int argc, char *argv[])
{
	CTL_CTL *this = &ctl_ctl;
	uint8_t nxt_state;
	uint32_t size;
	CTL_MSG *msg;
	CTL_FUNC func;
	
	// ����u���b�N�̏�����
	memset(this, 0, sizeof(CTL_CTL));
	
	// ����u���b�N�̐ݒ�
	this->tsk_id = kz_getid();
	this->msg_id = MSGBOX_ID_CTL_MAIN;
	this->state  = ST_UNINITIALIZED;
	
	while(1){
		// ���b�Z�[�W��M
		kz_recv(this->msg_id, &size, &msg);
		
		// ����/����Ԃ��擾
		func = fsm[this->state][msg->msg_type].func;
		nxt_state = fsm[this->state][msg->msg_type].nxt_state;
		
		// ����Ԃ��X�V
		this->state = nxt_state;

		// ���b�Z�[�W�����
	    kz_kmfree(msg);

		// ���������s
		if (func != NULL) {
			func(msg->msg_data);
		}
		
		// ��ԑJ��
		if ((nxt_state != ST_UNDEIFNED) && (this->req_chg_sts == 0U)) {
			this->state = nxt_state;
		} else if (this->req_chg_sts == 1U) {
			this->req_chg_sts = 0U;
		}
	}
}

/*!
 @brief �������b�Z�[�W�𑗐M����^�X�N
 @param [in] argc
 @param [in] *argv[]
 */
int ctl_cycmsg_main(int argc, char *argv[])
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
	
	// ���X�g�̍Ō������
	node = cycmsg_que[0].head;
	while (node != NULL) {
		node = cycmsg_que[0].head->next;
	}
	
	// ���X�g�̍Ō�ɁA�m�[�h��ǉ�
	node = new_node;
	cycmsg_que[0].head = new_node;
	
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
 @brief �V�X�e������^�X�N�ɏ��������b�Z�[�W�𑗐M����֐�
 @param [in] �Ȃ�
 @return     �Ȃ�
*/
void CTL_MSG_init(void)
{
	CTL_CTL *this = &ctl_ctl;
	CTL_MSG *msg;
	
	msg = kz_kmalloc(sizeof(CTL_MSG));
	
	msg->msg_type = MSG_INIT;
	msg->msg_data = NULL;
	
	kz_send(this->msg_id, sizeof(CTL_MSG), msg);
	
	return;
}

/*! 
 @brief �V�X�e������^�X�N�ɏ��������b�Z�[�W�𑗐M����֐�
 @param [in] �Ȃ�
 @return     �Ȃ�
*/
void CTL_MSG_event(void)
{
	CTL_CTL *this = &ctl_ctl;
	CTL_MSG *msg;

	msg = kz_kmalloc(sizeof(CTL_MSG));
	
	msg->msg_type = MSG_EVENT;
	msg->msg_data = NULL;
	
	kz_send(this->msg_id, sizeof(CTL_MSG), msg);
	
	return;
}
