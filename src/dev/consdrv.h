#ifndef _CONSDRV_H_INCLUDED_
#define _CONSDRV_H_INCLUDED_

#define CONSDRV_DEVICE_NUM 1
#define USE_STATIC_BUFFER 1
//#define CONSDRV_CMD_USE   'u' /* �R���\�[���E�h���C�o�̎g�p�J�n */
//#define CONSDRV_CMD_WRITE 'w' /* �R���\�[���ւ̕�����o�� */

void consdrv_send_write(char *str);
void consdrv_send_use(int index);

#endif
