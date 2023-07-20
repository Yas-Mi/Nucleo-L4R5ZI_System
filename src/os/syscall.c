#include "defines.h"
#include "kozos.h"
#include "interrupt.h"
#include "syscall.h"

static void consdrv_send_write_wrapper(void* address)
{
	uint32_t adr;
	char ascii[8];

	adr = (uint32_t)address;

	ascii[0] = ((adr & 0xF0000000) >> 28) >= 10 ? ((adr & 0xF0000000) >> 28) + 55 : ((adr & 0xF0000000) >> 28) + 48;
	ascii[1] = ((adr & 0x0F000000) >> 24) >= 10 ? ((adr & 0x0F000000) >> 24) + 55 : ((adr & 0x0F000000) >> 24) + 48;
	ascii[2] = ((adr & 0x00F00000) >> 20) >= 10 ? ((adr & 0x00F00000) >> 20) + 55 : ((adr & 0x00F00000) >> 20) + 48;
	ascii[3] = ((adr & 0x000F0000) >> 16) >= 10 ? ((adr & 0x000F0000) >> 16) + 55 : ((adr & 0x000F0000) >> 16) + 48;
	ascii[4] = ((adr & 0x0000F000) >> 12) >= 10 ? ((adr & 0x0000F000) >> 12) + 55 : ((adr & 0x0000F000) >> 12) + 48;
	ascii[5] = ((adr & 0x00000F00) >> 8) >= 10 ? ((adr & 0x00000F00) >> 8) + 55 : ((adr & 0x00000F00) >> 8) + 48;
	ascii[6] = ((adr & 0x000000F0) >> 4) >= 10 ? ((adr & 0x000000F0) >> 4) + 55 : ((adr & 0x000000F0) >> 4) + 48;
	ascii[7] = ((adr & 0x0000000F) >> 0) >= 10 ? ((adr & 0x0000000F) >> 0) + 55 : ((adr & 0x0000000F) >> 0) + 48;
	consdrv_send_write(ascii);
	consdrv_send_write('\n');
}

/* システム・コール */

kz_thread_id_t kz_run(kz_func_t func, char *name, int priority, int stacksize,
		      int argc, char *argv[])
{
  kz_syscall_param_t param;
  param.un.run.func = func;
  param.un.run.name = name;
  param.un.run.priority = priority;
  param.un.run.stacksize = stacksize;
  param.un.run.argc = argc;
  param.un.run.argv = argv;
  kz_syscall(KZ_SYSCALL_TYPE_RUN, &param);
  return param.un.run.ret;
}

void kz_exit(void)
{
  kz_syscall(KZ_SYSCALL_TYPE_EXIT, NULL);
}

int kz_wait(void)
{
  kz_syscall_param_t param;
  kz_syscall(KZ_SYSCALL_TYPE_WAIT, &param);
  return param.un.wait.ret;
}

int kz_sleep(void)
{
  kz_syscall_param_t param;
  kz_syscall(KZ_SYSCALL_TYPE_SLEEP, &param);
  return param.un.sleep.ret;
}

int kz_wakeup(kz_thread_id_t id)
{
  kz_syscall_param_t param;
  param.un.wakeup.id = id;
  kz_syscall(KZ_SYSCALL_TYPE_WAKEUP, &param);
  return param.un.wakeup.ret;
}

kz_thread_id_t kz_getid(void)
{
  kz_syscall_param_t param;
  kz_syscall(KZ_SYSCALL_TYPE_GETID, &param);
  return param.un.getid.ret;
}

int kz_chpri(int priority)
{
  kz_syscall_param_t param;
  param.un.chpri.priority = priority;
  kz_syscall(KZ_SYSCALL_TYPE_CHPRI, &param);
  return param.un.chpri.ret;
}

void *kz_kmalloc(int size)
{
	kz_syscall_param_t param;
	uint32_t result;
	
	__asm volatile ("MRS %0, basepri" : "=r" (result) );
	param.un.kmalloc.size = size;
	param.un.kmalloc.ret = 0xAAAAAAAA;
	
	kz_syscall(KZ_SYSCALL_TYPE_KMALLOC, &param);
	return param.un.kmalloc.ret;
}

int kz_kmfree(void *p)
{
  kz_syscall_param_t param;
  param.un.kmfree.p = p;
  kz_syscall(KZ_SYSCALL_TYPE_KMFREE, &param);
  return param.un.kmfree.ret;
}

int kz_send(kz_msgbox_id_t id, int size, char *p)
{
	kz_syscall_param_t param;
	
	param.un.send.id = id;
	param.un.send.size = size;
	param.un.send.p = p;
	
	kz_syscall(KZ_SYSCALL_TYPE_SEND, &param);
	return param.un.send.ret;
}

kz_thread_id_t kz_recv(kz_msgbox_id_t id, int *sizep, char **pp)
{
  kz_syscall_param_t param;
  param.un.recv.id = id;
  param.un.recv.sizep = sizep;
  param.un.recv.pp = pp;
  kz_syscall(KZ_SYSCALL_TYPE_RECV, &param);
  return param.un.recv.ret;
}

int kz_setintr(softvec_type_t type, kz_handler_t handler)
{
  kz_syscall_param_t param;
  param.un.setintr.type = type;
  param.un.setintr.handler = handler;
  kz_syscall(KZ_SYSCALL_TYPE_SETINTR, &param);
  return param.un.setintr.ret;
}

int kz_tsleep(int time)
{
  kz_syscall_param_t param;
  param.un.tsleep.time = time;
  kz_syscall(KZ_SYSCALL_TYPE_TSLEEP, &param);
  return param.un.tsleep.ret;
}

/* サービス・コール */

int kx_wakeup(kz_thread_id_t id)
{
  kz_syscall_param_t param;
  param.un.wakeup.id = id;
  kz_srvcall(KZ_SYSCALL_TYPE_WAKEUP, &param);
  return param.un.wakeup.ret;
}

void *kx_kmalloc(int size)
{
  kz_syscall_param_t param;
  param.un.kmalloc.size = size;
  kz_srvcall(KZ_SYSCALL_TYPE_KMALLOC, &param);
  return param.un.kmalloc.ret;
}

int kx_kmfree(void *p)
{
  kz_syscall_param_t param;
  param.un.kmfree.p = p;
  kz_srvcall(KZ_SYSCALL_TYPE_KMFREE, &param);
  return param.un.kmfree.ret;
}

int kx_send(kz_msgbox_id_t id, int size, char *p)
{
  kz_syscall_param_t param;
  param.un.send.id = id;
  param.un.send.size = size;
  param.un.send.p = p;
  kz_srvcall(KZ_SYSCALL_TYPE_SEND, &param);
  return param.un.send.ret;
}
