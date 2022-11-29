#ifndef _KOZOS_H_INCLUDED_
#define _KOZOS_H_INCLUDED_

#include "defines.h"
#include "interrupt.h"
#include "syscall.h"

/* �V�X�e���E�R�[�� */
kz_thread_id_t kz_run(kz_func_t func, char *name, int priority, int stacksize,
		      int argc, char *argv[]);
void kz_exit(void);
int kz_wait(void);
int kz_sleep(void);
int kz_wakeup(kz_thread_id_t id);
kz_thread_id_t kz_getid(void);
int kz_chpri(int priority);
void *kz_kmalloc(int size);
int kz_kmfree(void *p);
int kz_send(kz_msgbox_id_t id, int size, char *p);
kz_thread_id_t kz_recv(kz_msgbox_id_t id, int *sizep, char **pp);
int kz_setintr(softvec_type_t type, kz_handler_t handler);
int kz_tsleep(int time);
/* �T�[�r�X�E�R�[�� */
int kx_wakeup(kz_thread_id_t id);
void *kx_kmalloc(int size);
int kx_kmfree(void *p);
int kx_send(kz_msgbox_id_t id, int size, char *p);
/* utility */
uint32 kz_get_time_ms(void);
uint32 kz_get_time_s(void);

/* ���C�u�����֐� */
void kz_start(kz_func_t func, char *name, int priority, int stacksize,
	      int argc, char *argv[]);
void kz_sysdown(void);
void kz_syscall(kz_syscall_type_t type, kz_syscall_param_t *param);
void kz_srvcall(kz_syscall_type_t type, kz_syscall_param_t *param);

/* �V�X�e���E�^�X�N */
int ctl_main(int argc, char *argv[]);
int ctl_cycmsg_main(int argc, char *argv[]);

/* �R���\�[���^�X�N */
int command_main(int argc, char *argv[]);
int consdrv_main(int argc, char *argv[]);

/* �����g�Z���T�f�[�^�擾�^�X�N */
int US_main(int argc, char *argv[]);

/* BlueTooth�^�X�N */
int BT_main(int argc, char *argv[]);
int BT_mng_connect_sts(int argc, char *argv[]);
int BT_dev_main(int argc, char *argv[]);

// �\���A�v���^�X�N
int LCD_app_main(int argc, char *argv[]);

// �{�^���f�o�C�X�^�X�N
int BTN_dev_main(int argc, char *argv[]);

#endif
