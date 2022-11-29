#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "syscall.h"
#include "memory.h"
#include "lib.h"
#include "stm32l4xx.h"
#include "usart.h"
#include "btn_dev.h"
#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_cortex.h"
#include "stm32l4xx_hal_i2c.h"

#define THREAD_NUM 15
#define PRIORITY_NUM 16
#define THREAD_NAME_SIZE 15
extern void connected_handler(void) ;
/* �X���b�h�E�R���e�L�X�g */
typedef struct _kz_context {
  uint32 sp; /* �X�^�b�N�E�|�C���^ */
} kz_context;

/* �^�X�N�E�R���g���[���E�u���b�N(TCB) */
typedef struct _kz_thread {
  struct _kz_thread *next;
  struct _kz_thread *t_next;
  int time;
  char name[THREAD_NAME_SIZE + 1]; /* �X���b�h�� */
  int priority;   /* �D��x */
  char *stack;    /* �X�^�b�N */
  uint32 flags;   /* �e��t���O */
#define KZ_THREAD_FLAG_READY (1 << 0)

  struct { /* �X���b�h�̃X�^�[�g�E�A�b�v(thread_init())�ɓn���p�����[�^ */
    kz_func_t func; /* �X���b�h�̃��C���֐� */
    int argc;       /* �X���b�h�̃��C���֐��ɓn�� argc */
    char **argv;    /* �X���b�h�̃��C���֐��ɓn�� argv */
  } init;

  struct { /* �V�X�e���E�R�[���p�o�b�t�@ */
    kz_syscall_type_t type;
    kz_syscall_param_t *param;
  } syscall;

  kz_context context; /* �R���e�L�X�g��� */
} kz_thread;

/* ���b�Z�[�W�E�o�b�t�@ */
typedef struct _kz_msgbuf {
  struct _kz_msgbuf *next;
  kz_thread *sender; /* ���b�Z�[�W�𑗐M�����X���b�h */
  struct { /* ���b�Z�[�W�̃p�����[�^�ۑ��̈� */
    int size;
    char *p;
  } param;
} kz_msgbuf;

/* ���b�Z�[�W�E�{�b�N�X */
typedef struct _kz_msgbox {
  kz_thread *receiver; /* ��M�҂���Ԃ̃X���b�h */
  kz_msgbuf *head;
  kz_msgbuf *tail;

  /*
   * H8��16�r�b�gCPU�Ȃ̂ŁC32�r�b�g�����ɑ΂��Ă̏�Z���߂������D�����
   * �\���̂̃T�C�Y���Q�̗ݏ�ɂȂ��Ă��Ȃ��ƁC�\���̂̔z��̃C���f�b�N�X
   * �v�Z�ŏ�Z���g���āu___mulsi3�������v�Ȃǂ̃����N�E�G���[�ɂȂ�ꍇ��
   * ����D(�Q�̗ݏ�Ȃ�΃V�t�g���Z�����p�����̂Ŗ��͏o�Ȃ�)
   * �΍�Ƃ��āC�T�C�Y���Q�̗ݏ�ɂȂ�悤�Ƀ_�~�[�E�����o�Œ�������D
   * ���\���̂œ��l�̃G���[���o���ꍇ�ɂ́C���l�̑Ώ������邱�ƁD
   */
  long dummy[1];
} kz_msgbox;

typedef struct{
	uint32 time_ms; /* ms */
	uint32 time_s;  /*  s */
}kz_time;

/* �X���b�h�̃��f�B�[�E�L���[ */
static struct {
  kz_thread *head;
  kz_thread *tail;
} readyque[PRIORITY_NUM];

/* �X���b�h�̃��f�B�[�E�L���[ */
static struct {
  kz_thread *head;
  kz_thread *tail;
} timque[1];

static kz_thread *current; /* �J�����g�E�X���b�h */
static kz_thread threads[THREAD_NUM]; /* �^�X�N�E�R���g���[���E�u���b�N */
static kz_handler_t handlers[SOFTVEC_TYPE_NUM]; /* �����݃n���h�� */
static kz_msgbox msgboxes[MSGBOX_ID_NUM]; /* ���b�Z�[�W�E�{�b�N�X */
static kz_time mng_time;

void dispatch(kz_context *context);
void SysTick_Handler(void);
void static SysTick_init(void);
static void mng_tsleep(void);

/* �J�����g�E�X���b�h�����f�B�[�E�L���[���甲���o�� */
static int getcurrent(void)
{
  if (current == NULL) {
    return -1;
  }
  if (!(current->flags & KZ_THREAD_FLAG_READY)) {
    /* ���łɖ����ꍇ�͖��� */
    return 1;
  }

  /* �J�����g�E�X���b�h�͕K���擪�ɂ���͂��Ȃ̂ŁC�擪���甲���o�� */
  readyque[current->priority].head = current->next;
  if (readyque[current->priority].head == NULL) {
    readyque[current->priority].tail = NULL;
  }
  current->flags &= ~KZ_THREAD_FLAG_READY;
  current->next = NULL;

  return 0;
}

/* �J�����g�E�X���b�h�����f�B�[�E�L���[�Ɍq���� */
static int putcurrent(void)
{
  if (current == NULL) {
    return -1;
  }
  if (current->flags & KZ_THREAD_FLAG_READY) {
    /* ���łɗL��ꍇ�͖��� */
    return 1;
  }

  /* ���f�B�[�E�L���[�̖����ɐڑ����� */
  if (readyque[current->priority].tail) {
    readyque[current->priority].tail->next = current;
  } else {
    readyque[current->priority].head = current;
  }
  readyque[current->priority].tail = current;
  current->flags |= KZ_THREAD_FLAG_READY;

  return 0;
}

static void thread_end(void)
{
  kz_exit();
}

/* �X���b�h�̃X�^�[�g�E�A�b�v */
static void thread_init(kz_thread *thp)
{

	/* �X���b�h�̃��C���֐����Ăяo�� */
	thp->init.func(thp->init.argc, thp->init.argv);
	thread_end();
}

/* �V�X�e���E�R�[���̏���(kz_run():�X���b�h�̋N��) */
static kz_thread_id_t thread_run(kz_func_t func, char *name, int priority,
				 int stacksize, int argc, char *argv[])
{
  int i;
  kz_thread *thp;
  uint32 *sp;
  extern char _task_stack_start; /* �����J�E�X�N���v�g�Œ�`�����X�^�b�N�̈� */
  static char *thread_stack = &_task_stack_start;
  static int init_dispatch = 0;
  /* �󂢂Ă���^�X�N�E�R���g���[���E�u���b�N������ */
  for (i = 0; i < THREAD_NUM; i++) {
    thp = &threads[i];
    if (!thp->init.func) /* �������� */
      break;
  }
  if (i == THREAD_NUM) /* ������Ȃ����� */
    return -1;

  memset(thp, 0, sizeof(*thp));

  /* �^�X�N�E�R���g���[���E�u���b�N(TCB)�̐ݒ� */
  strcpy(thp->name, name);
  thp->next     = NULL;
  thp->priority = priority;
  thp->flags    = 0;

  thp->init.func = func;
  thp->init.argc = argc;
  thp->init.argv = argv;

  /* �X�^�b�N�̈���l�� */
  memset(thread_stack, 0, stacksize);
  thread_stack += stacksize;

  thp->stack = thread_stack; /* �X�^�b�N��ݒ� */

  /* �X�^�b�N�̏����� */
  sp = (uint32 *)thp->stack;

  /*
   * �v���O�����E�J�E���^��ݒ肷��D
   * �X���b�h�̗D��x���[���̏ꍇ�ɂ́C�����݋֎~�X���b�h�Ƃ���D
   */
	if(init_dispatch==0){
		*(--sp) = (uint32_t)thread_end;
		*(--sp) = (uint32_t)thread_init;
		*(--sp) = 0; /* r12 */
		*(--sp) = 0; /* r11 */
		*(--sp) = 0; /* r10 */
		*(--sp) = 0; /* r9  */
		*(--sp) = 0; /* r8  */
		*(--sp) = 0; /* r7  */
		*(--sp) = 0; /* r6  */
		*(--sp) = 0; /* r5  */
		*(--sp) = 0; /* r4  */
		*(--sp) = 0; /* r3  */
		*(--sp) = 0; /* r2  */
		*(--sp) = 0; /* r1  */
		/* �X���b�h�̃X�^�[�g�A�b�v(thread_init())�ɓn�������@*/
		*(--sp) = (uint32_t)thp; /* r0  */
		init_dispatch = 1;
	}else{
		*(--sp) = (uint32_t)thread_end;
		*(--sp) = 0x1000000;             /* APSR */
		*(--sp) = (uint32_t)thread_init; /* pc   */
		*(--sp) = 0;                     /* lr(���ɐݒ肷��K�v�͂Ȃ�) */
		*(--sp) = 0;                     /* r12  */
		*(--sp) = 0;                     /* r3   */
		*(--sp) = 0;                     /* r2   */
		*(--sp) = 0;                     /* r1   */
		*(--sp) = (uint32_t)thp;         /* r0(�^�X�N�̈���) */
		*(--sp) = (uint32_t)0xFFFFFFF9;
		*(--sp) = 0;                     /* r11  */
		*(--sp) = 0;                     /* r10  */
		*(--sp) = 0;                     /* r9   */
		*(--sp) = 0;                     /* r8   */
		*(--sp) = 0;                     /* r7   */
		*(--sp) = 0;                     /* r6   */
		*(--sp) = 0;                     /* r5   */
		*(--sp) = 0;                     /* r4   */
	}
  /* �X���b�h�̃R���e�L�X�g��ݒ� */
  thp->context.sp = (uint32)sp;

  /* �V�X�e���E�R�[�����Ăяo�����X���b�h�����f�B�[�E�L���[�ɖ߂� */
  putcurrent();

  /* �V�K�쐬�����X���b�h���C���f�B�[�E�L���[�ɐڑ����� */
  current = thp;
  putcurrent();

  return (kz_thread_id_t)current;
}

/* �V�X�e���E�R�[���̏���(kz_exit():�X���b�h�̏I��) */
static int thread_exit(void)
{
  /*
   * �{���Ȃ�X�^�b�N��������čė��p�ł���悤�ɂ��ׂ������ȗ��D
   * ���̂��߁C�X���b�h��p�ɂɐ����E��������悤�Ȃ��Ƃ͌���łł��Ȃ��D
   */
  puts(current->name);
  puts(" EXIT.\n");
  memset(current, 0, sizeof(*current));
  return 0;
}

/* �V�X�e���E�R�[���̏���(kz_wait():�X���b�h�̎��s������) */
static int thread_wait(void)
{
  putcurrent();
  return 0;
}

/* �V�X�e���E�R�[���̏���(kz_sleep():�X���b�h�̃X���[�v) */
static int thread_sleep(void)
{
  return 0;
}

/* �V�X�e���E�R�[���̏���(kz_wakeup():�X���b�h�̃E�F�C�N�E�A�b�v) */
static int thread_wakeup(kz_thread_id_t id)
{
  /* �E�F�C�N�E�A�b�v���Ăяo�����X���b�h�����f�B�[�E�L���[�ɖ߂� */
  putcurrent();

  /* �w�肳�ꂽ�X���b�h�����f�B�[�E�L���[�ɐڑ����ăE�F�C�N�E�A�b�v���� */
  current = (kz_thread *)id;
  putcurrent();

  return 0;
}

/* �V�X�e���E�R�[���̏���(kz_getid():�X���b�hID�擾) */
static kz_thread_id_t thread_getid(void)
{
  putcurrent();
  return (kz_thread_id_t)current;
}

/* �V�X�e���E�R�[���̏���(kz_chpri():�X���b�h�̗D��x�ύX) */
static int thread_chpri(int priority)
{
  int old = current->priority;
  if (priority >= 0)
    current->priority = priority; /* �D��x�ύX */
  putcurrent(); /* �V�����D��x�̃��f�B�[�E�L���[�Ɍq������ */
  return old;
}

/* �V�X�e���E�R�[���̏���(kz_kmalloc():���I�������l��) */
static void *thread_kmalloc(int size)
{
  putcurrent();
  return kzmem_alloc(size);
}

/* �V�X�e���E�R�[���̏���(kz_kfree():���������) */
static int thread_kmfree(char *p)
{
  kzmem_free(p);
  putcurrent();
  return 0;
}

/* ���b�Z�[�W�̑��M���� */
static void sendmsg(kz_msgbox *mboxp, kz_thread *thp, int size, char *p)
{
  kz_msgbuf *mp;

  /* ���b�Z�[�W�E�o�b�t�@�̍쐬 */
  mp = (kz_msgbuf *)kzmem_alloc(sizeof(*mp));
  if (mp == NULL)
    kz_sysdown();
  mp->next       = NULL;
  mp->sender     = thp;
  mp->param.size = size;
  mp->param.p    = p;

  /* ���b�Z�[�W�E�{�b�N�X�̖����Ƀ��b�Z�[�W��ڑ����� */
  if (mboxp->tail) {
    mboxp->tail->next = mp;
  } else {
    mboxp->head = mp;
  }
  mboxp->tail = mp;
}

/* ���b�Z�[�W�̎�M���� */
static void recvmsg(kz_msgbox *mboxp)
{
  kz_msgbuf *mp;
  kz_syscall_param_t *p;

  /* ���b�Z�[�W�E�{�b�N�X�̐擪�ɂ��郁�b�Z�[�W�𔲂��o�� */
  mp = mboxp->head;
  mboxp->head = mp->next;
  if (mboxp->head == NULL)
    mboxp->tail = NULL;
  mp->next = NULL;

  /* ���b�Z�[�W����M����X���b�h�ɕԂ��l��ݒ肷�� */
  p = mboxp->receiver->syscall.param;
  p->un.recv.ret = (kz_thread_id_t)mp->sender;
  if (p->un.recv.sizep)
    *(p->un.recv.sizep) = mp->param.size;
  if (p->un.recv.pp)
    *(p->un.recv.pp) = mp->param.p;

  /* ��M�҂��X���b�h�͂��Ȃ��Ȃ����̂ŁCNULL�ɖ߂� */
  mboxp->receiver = NULL;

  /* ���b�Z�[�W�E�o�b�t�@�̉�� */
  kzmem_free(mp);
}

/* �V�X�e���E�R�[���̏���(kz_send():���b�Z�[�W���M) */
static int thread_send(kz_msgbox_id_t id, int size, char *p)
{
  kz_msgbox *mboxp = &msgboxes[id];

  putcurrent();
  sendmsg(mboxp, current, size, p); /* ���b�Z�[�W�̑��M���� */

  /* ��M�҂��X���b�h�����݂��Ă���ꍇ�ɂ͎�M�������s�� */
  if (mboxp->receiver) {
	current = mboxp->receiver; /* ��M�҂��X���b�h */
	recvmsg(mboxp); /* ���b�Z�[�W�̎�M���� */
	putcurrent(); /* ��M�ɂ�蓮��\�ɂȂ����̂ŁC�u���b�N�������� */
  }

  return size;
}

/* �V�X�e���E�R�[���̏���(kz_recv():���b�Z�[�W��M) */
static kz_thread_id_t thread_recv(kz_msgbox_id_t id, int *sizep, char **pp)
{
  kz_msgbox *mboxp = &msgboxes[id];

  if (mboxp->receiver) /* ���̃X���b�h�����łɎ�M�҂����Ă��� */
    kz_sysdown();

  mboxp->receiver = current; /* ��M�҂��X���b�h�ɐݒ� */

  if (mboxp->head == NULL) {
    /*
     * ���b�Z�[�W�E�{�b�N�X�Ƀ��b�Z�[�W�������̂ŁC�X���b�h��
     * �X���[�v������D(�V�X�e���E�R�[�����u���b�N����)
     */
    return -1;
  }

  recvmsg(mboxp); /* ���b�Z�[�W�̎�M���� */
  putcurrent(); /* ���b�Z�[�W����M�ł����̂ŁC���f�B�[��Ԃɂ��� */

  return current->syscall.param->un.recv.ret;
}

/* �V�X�e���E�R�[���̏���(kz_setintr():�����݃n���h���o�^) */
static int thread_setintr(softvec_type_t type, kz_handler_t handler)
{
  //static void thread_intr(softvec_type_t type, unsigned long sp);

  /*
   * �����݂��󂯕t���邽�߂ɁC�\�t�g�E�G�A�E�����݃x�N�^��
   * OS�̊����ݏ����̓����ƂȂ�֐���o�^����D
   */
  //softvec_setintr(type, thread_intr);

  handlers[type] = handler; /* OS������Ăяo�������݃n���h����o�^ */
  putcurrent();

  return 0;
}

static int thread_tsleep(int time)
{
	// time��ݒ�
	current->time = time;

    // timque�̖����ɐڑ�����
    if(timque[0].tail){
        // �����ɐڑ�����Ă���ꍇ�́A���̖����̎��𓮍쒆�̃^�X�N�ɐݒ肷��
        timque[0].tail->t_next = current;
    }else{
        // �����ɐڑ�����Ă��ȏꍇ�́A�擪�ɓ��쒆�̃^�X�N��ݒ肷��
	    timque[0].head = current;
    }
    // �����𓮍쒆�̃^�X�N�ɐݒ肷��
    timque[0].tail = current;

    return 0;
}

static void call_functions(kz_syscall_type_t type, kz_syscall_param_t *p)
{
  /* �V�X�e���E�R�[���̎��s����current�����������̂Œ��� */
  switch (type) {
  case KZ_SYSCALL_TYPE_RUN: /* kz_run() */
    p->un.run.ret = thread_run(p->un.run.func, p->un.run.name,
			       p->un.run.priority, p->un.run.stacksize,
			       p->un.run.argc, p->un.run.argv);
    break;
  case KZ_SYSCALL_TYPE_EXIT: /* kz_exit() */
    /* TCB�����������̂ŁC�߂�l����������ł͂����Ȃ� */
    thread_exit();
    break;
  case KZ_SYSCALL_TYPE_WAIT: /* kz_wait() */
    p->un.wait.ret = thread_wait();
    break;
  case KZ_SYSCALL_TYPE_SLEEP: /* kz_sleep() */
    p->un.sleep.ret = thread_sleep();
    break;
  case KZ_SYSCALL_TYPE_WAKEUP: /* kz_wakeup() */
    p->un.wakeup.ret = thread_wakeup(p->un.wakeup.id);
    break;
  case KZ_SYSCALL_TYPE_GETID: /* kz_getid() */
    p->un.getid.ret = thread_getid();
    break;
  case KZ_SYSCALL_TYPE_CHPRI: /* kz_chpri() */
    p->un.chpri.ret = thread_chpri(p->un.chpri.priority);
    break;
  case KZ_SYSCALL_TYPE_KMALLOC: /* kz_kmalloc() */
    p->un.kmalloc.ret = thread_kmalloc(p->un.kmalloc.size);
    break;
  case KZ_SYSCALL_TYPE_KMFREE: /* kz_kmfree() */
    p->un.kmfree.ret = thread_kmfree(p->un.kmfree.p);
    break;
  case KZ_SYSCALL_TYPE_SEND: /* kz_send() */
    p->un.send.ret = thread_send(p->un.send.id,
				 p->un.send.size, p->un.send.p);
    break;
  case KZ_SYSCALL_TYPE_RECV: /* kz_recv() */
    p->un.recv.ret = thread_recv(p->un.recv.id,
				 p->un.recv.sizep, p->un.recv.pp);
    break;
  case KZ_SYSCALL_TYPE_SETINTR: /* kz_setintr() */
    p->un.setintr.ret = thread_setintr(p->un.setintr.type,
				       p->un.setintr.handler);
  case KZ_SYSCALL_TYPE_TSLEEP:/* kz_tsleep() */
    p->un.tsleep.ret = thread_tsleep(p->un.tsleep.time);
    break;
  default:
    break;
  }
}

/* �V�X�e���E�R�[���̏��� */
static void syscall_proc(kz_syscall_type_t type, kz_syscall_param_t *p)
{
  /*
   * �V�X�e���E�R�[�����Ăяo�����X���b�h�����f�B�[�E�L���[����
   * �O������Ԃŏ����֐����Ăяo���D���̂��߃V�X�e���E�R�[����
   * �Ăяo�����X���b�h�����̂܂ܓ���p�����������ꍇ�ɂ́C
   * �����֐��̓����� putcurrent() ���s���K�v������D
   */
  getcurrent();
  call_functions(type, p);
}

/* �T�[�r�X�E�R�[���̏��� */
static void srvcall_proc(kz_syscall_type_t type, kz_syscall_param_t *p)
{
  /*
   * �V�X�e���E�R�[���ƃT�[�r�X�E�R�[���̏����֐��̓����ŁC
   * �V�X�e���E�R�[���̎��s�����X���b�hID�𓾂邽�߂� current ��
   * �Q�Ƃ��Ă��镔��������(���Ƃ��� thread_send() �Ȃ�)�C
   * current ���c���Ă���ƌ듮�삷�邽�� NULL �ɐݒ肷��D
   * �T�[�r�X�E�R�[���� thread_intrvec() �����̊����݃n���h���Ăяo����
   * �����ŌĂ΂�Ă���͂��łȂ̂ŁC�Ăяo����� thread_intrvec() ��
   * �X�P�W���[�����O�������s���Ccurrent �͍Đݒ肳���D
   */
  current = NULL;
  call_functions(type, p);
}

/* �X���b�h�̃X�P�W���[�����O */
static void schedule(void)
{
  int i;

  /*
   * �D�揇�ʂ̍�����(�D��x�̐��l�̏�������)�Ƀ��f�B�[�E�L���[�����āC
   * ����\�ȃX���b�h����������D
   */
  for (i = 0; i < PRIORITY_NUM; i++) {
    if (readyque[i].head) /* �������� */
      break;
  }
  if (i == PRIORITY_NUM) /* ������Ȃ����� */
    kz_sysdown();

  current = readyque[i].head; /* �J�����g�E�X���b�h�ɐݒ肷�� */
}

static void syscall_intr(void)
{
  syscall_proc(current->syscall.type, current->syscall.param);
}

static void softerr_intr(void)
{
  puts(current->name);
  puts(" DOWN.\n");
  getcurrent(); /* ���f�B�[�L���[����O�� */
  thread_exit(); /* �X���b�h�I������ */
}

/* �����ݏ����̓����֐� */
void thread_intr(softvec_type_t type, unsigned long sp)
{
  /* �J�����g�E�X���b�h�̃R���e�L�X�g��ۑ����� */
  current->context.sp = sp;

  /*
   * �����݂��Ƃ̏��������s����D
   * SOFTVEC_TYPE_SYSCALL, SOFTVEC_TYPE_SOFTERR �̏ꍇ��
   * syscall_intr(), softerr_intr() ���n���h���ɓo�^����Ă���̂ŁC
   * ����炪���s�����D
   * ����ȊO�̏ꍇ�́Ckz_setintr()�ɂ���ă��[�U�o�^���ꂽ�n���h����
   * ���s�����D
   */
  if (handlers[type])
    handlers[type]();

  schedule(); /* �X���b�h�̃X�P�W���[�����O */

  /*
   * �X���b�h�̃f�B�X�p�b�`
   * (dispatch()�֐��̖{�̂�startup.s�ɂ���C�A�Z���u���ŋL�q����Ă���)
   */
  _dispatch_int(current->context);
  /* �����ɂ͕Ԃ��Ă��Ȃ� */
}

void kz_start(kz_func_t func, char *name, int priority, int stacksize,
	      int argc, char *argv[])
{
	kzmem_init(); /* ���I�������̏����� */

	/*
	* �ȍ~�ŌĂяo���X���b�h�֘A�̃��C�u�����֐��̓����� current ��
	* ���Ă���ꍇ������̂ŁCcurrent �� NULL �ɏ��������Ă����D
	*/
	current = NULL;

	memset(readyque, 0, sizeof(readyque));
	memset(threads,  0, sizeof(threads));
	memset(handlers, 0, sizeof(handlers));
	memset(msgboxes, 0, sizeof(msgboxes));
	memset(&mng_time, 0, sizeof(mng_time));

	/* �����݃n���h���̓o�^ */
	thread_setintr(SVCall_INTERRUPT_NO, syscall_intr); /* �V�X�e���E�R�[�� */
	thread_setintr(SYSTICK_INTERRUPT_NO, SysTick_Handler);
	thread_setintr(USART1_GLOBAL_INTERRUPT_NO, usart1_handler); /* USART1 */
	thread_setintr(USART2_GLOBAL_INTERRUPT_NO, usart2_handler); /* USART2 */
	thread_setintr(USART3_GLOBAL_INTERRUPT_NO, usart3_handler); /* USART3 */
	thread_setintr(I2C1_EV_INTERRUPT_NO, I2C1_EV_IRQHandler);   /* I2C1 */
	thread_setintr(I2C1_ER_INTERRUPT_NO, I2C1_ER_IRQHandler);   /* I2C1 */
	thread_setintr(EXTI0_INTERRUPT_NO, btn_up_interrupt);   	/* EXTI0 */
	thread_setintr(EXTI1_INTERRUPT_NO, btn_down_interrupt);     /* EXTI1 */
	thread_setintr(EXTI2_INTERRUPT_NO, btn_back_interrupt);     /* EXTI2 */
	thread_setintr(EXTI3_INTERRUPT_NO, btn_select_interrupt);   /* EXTI3 */
  //thread_setintr(OCTOSPI1_GLOBAL_INTERRUPT_NO, opctspi1_handler); /* OCTSPI1 */
  //thread_setintr(OCTOSPI2_GLOBAL_INTERRUPT_NO, opctspi2_handler); /* OCTSPI2 */
  //thread_setintr(SOFTVEC_TYPE_SOFTERR, softerr_intr); /* �_�E���v������ */

	/* �V�X�e���E�R�[�����s�s�Ȃ̂Œ��ڊ֐����Ăяo���ăX���b�h�쐬���� */
	current = (kz_thread *)thread_run(func, name, priority, stacksize,
				    argc, argv);

	/* Systick�^�C�}�̏����� */
	SysTick_init();

	/* �ŏ��̃X���b�h���N�� */
	_dispatch(current->context);

	/* �����ɂ͕Ԃ��Ă��Ȃ� */
}

void kz_sysdown(void)
{
  puts("system error!\n");
  while (1)
    ;
}

/* �V�X�e���E�R�[���Ăяo���p���C�u�����֐� */
void kz_syscall(kz_syscall_type_t type, kz_syscall_param_t *param)
{
  current->syscall.type  = type;
  current->syscall.param = param;
  __asm volatile("    SVC %0 \n" : : "I" (0));
  __asm volatile("    NOP \n");
}

/* �T�[�r�X�E�R�[���Ăяo���p���C�u�����֐� */
void kz_srvcall(kz_syscall_type_t type, kz_syscall_param_t *param)
{
  srvcall_proc(type, param);
}

uint32 kz_get_time_ms(void)
{
	return mng_time.time_ms;
}

uint32 kz_get_time_s(void)
{
	return mng_time.time_s;
}

/* Systick���荞�� */
void static SysTick_init(void)
{
	/* 1us������Systick���荞�݂𔭐������� */
    SysTick_Config(SystemCoreClock / 1000);
}

/* Systick���荞�� */
void SysTick_Handler(void)
{

	/* ���Ԃ̊Ǘ� */
	mng_time.time_ms++;
	/* 1000ms�o�ߌ� */
	if(1000 == mng_time.time_ms){
		mng_time.time_s++;
		mng_time.time_ms = 0;
	}

	/* �҂���Ԃ̃^�X�N���`�F�b�N */
	mng_tsleep();
}

static void mng_tsleep(void){
    int i;
    kz_thread *current_task;
    kz_thread *prev_task;

    current_task = timque[0].head;
    prev_task = timque[0].head;

    while(NULL != current_task){
        if(0 == current_task->time){
            /* ���f�B�[�L���[�֐ڑ�*/

            if(readyque[current_task->priority].tail) {
            	readyque[current_task->priority].tail->next = current_task;
            }else{
            	readyque[current_task->priority].head = current_task;
            }
            readyque[current->priority].tail = current_task;
            current_task->flags |= KZ_THREAD_FLAG_READY;

            /* timque����O�� */
            /*   �O�^�X�N��t_next�����݂̃^�X�N��t_next�ɐݒ肷��(���f�B�[�L���[�ɐڑ������^�X�N��timque����O��) */
            prev_task->t_next = current_task->t_next;
            /*   timque����O�����^�X�N��timque�̐擪�̏ꍇ */
            if(current_task == timque[0].head){
            	/*   �O�����^�X�N����timque�ɐڑ�����Ă��Ȃ������ꍇ */
            	if(NULL == current_task->t_next){
            		/*    timque�̐擪�Ɩ�����NULL�ɐݒ� */
            		timque[0].head = NULL;
            		timque[0].tail = NULL;
            	/*   �O�����^�X�N�ȊO�ɂ��ڑ����ꂽ�^�X�N���������ꍇ */
            	}else{
            		/*   timque�̐擪�����^�X�N��t_next�ɐݒ肷�� */
            		timque[0].head = current_task->t_next;
            	}
            /*   �O�����^�X�N��timque�̖����̏ꍇ */
            }else if(current_task == timque[0].tail){
            	/*   timque�̖�����O�^�X�N�ɐݒ肷�� */
            	timque[0].tail = prev_task;
            }
            /* �O�̃^�X�N���X�V */
            prev_task = current_task;
            /* ���̃^�X�N���X�V */
            current_task = current_task->t_next;
            /* timque����O�����̂ŁA�f�B�X�p�b�`�Ώۂ̃^�X�N��next�|�C���^���N���A */
            prev_task->t_next = NULL;

        }else{
        	/* ���Ԃ�1���炷 */
        	current_task->time--;
            /* �O�̃^�X�N���X�V */
        	prev_task = current_task;
            /* ���̃^�X�N���X�V */
            current_task = current_task->t_next;
        }
    }
}
