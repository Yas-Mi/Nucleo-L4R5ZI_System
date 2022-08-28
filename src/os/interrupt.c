#include "defines.h"
#include "intr.h"
#include "interrupt.h"

/* �\�t�g�E�G�A�E�����݃x�N�^�̏����� */
int softvec_init(void)
{
  int type;
  for (type = 0; type < SOFTVEC_TYPE_NUM; type++)
    softvec_setintr(type, NULL);
  return 0;
}

/* �\�t�g�E�G�A�E�����݃x�N�^�̐ݒ� */
int softvec_setintr(softvec_type_t type, softvec_handler_t handler)
{
  SOFTVECS[type] = handler;
  return 0;
}

/*
 * ���ʊ����݃n���h���D
 * �\�t�g�E�G�A�E�����݃x�N�^�����āC�e�n���h���ɕ��򂷂�D
 */
void interrupt(softvec_type_t type, unsigned long sp)
{
  softvec_handler_t handler = SOFTVECS[type];
  if (handler)
    handler(type, sp);
}
