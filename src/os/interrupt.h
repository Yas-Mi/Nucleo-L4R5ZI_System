#ifndef _INTERRUPT_H_INCLUDED_
#define _INTERRUPT_H_INCLUDED_

/* �ȉ��̓����J�E�X�N���v�g�Œ�`���Ă���V���{�� */
extern char softvec;
#define SOFTVEC_ADDR (&softvec)

typedef short softvec_type_t;

typedef void (*softvec_handler_t)(softvec_type_t type, unsigned long sp);

#define SOFTVECS ((softvec_handler_t *)SOFTVEC_ADDR)

#define INTR_ENABLE  asm volatile ("cpsie f");
#define INTR_DISABLE asm volatile ("cpsid f");
#define TASK_IDLE    asm volatile ("WFI \n");
/* �\�t�g�E�G�A�E�����݃x�N�^�̏����� */
int softvec_init(void);

/* �\�t�g�E�G�A�E�����݃x�N�^�̐ݒ� */
int softvec_setintr(softvec_type_t type, softvec_handler_t handler);

/* ���ʊ����݃n���h�� */
void interrupt(softvec_type_t type, unsigned long sp);

#endif
