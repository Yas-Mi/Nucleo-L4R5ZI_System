#include "defines.h"
#include "kozos.h"
#include "lib.h"
#include "memory.h"
#include "consdrv.h"
/*
 * �������E�u���b�N�\����
 * (�l�����ꂽ�e�̈�́C�擪�Ɉȉ��̍\���̂������Ă���)
 */
typedef struct _kzmem_block {
  struct _kzmem_block *next;
  int size;
} kzmem_block;

/* �������E�v�[�� */
typedef struct _kzmem_pool {
  int size;
  int num;
  kzmem_block *free;
} kzmem_pool;

/* �������E�v�[���̒�`(�X�̃T�C�Y�ƌ�) */
// �������v�[���̈��0x30000(196,608byte)
// 16*32 = 512byte, 32*64 = 2,048byte, 64*64 = 4,096byte, 128*16 = 2,048byte, 256*16 = 4,096byte
// 512 + 2,048 + 4,096 + 2,048 + 4,096 = 10,752byte
static kzmem_pool pool[] = {
  { 16, 32, NULL }, { 32, 64, NULL }, { 64, 64, NULL }, { 128, 16, NULL }, { 256, 16, NULL },
};

#define MEMORY_AREA_NUM (sizeof(pool) / sizeof(*pool))

/* �������E�v�[���̏����� */
static int kzmem_init_pool(kzmem_pool *p)
{
  int i;
  kzmem_block *mp;
  kzmem_block **mpp;
  extern char _system_pool_start; /* �����J�E�X�N���v�g�Œ�`�����󂫗̈� */
  static char *area = &_system_pool_start;

  mp = (kzmem_block *)area;

  /* �X�̗̈�����ׂĉ���ς݃����N���X�g�Ɍq�� */
  mpp = &p->free;
  for (i = 0; i < p->num; i++) {
    *mpp = mp;
    memset(mp, 0, sizeof(*mp));
    mp->size = p->size;
    mpp = &(mp->next);
    mp = (kzmem_block *)((char *)mp + p->size);
    area += p->size;
  }

  return 0;
}

/* ���I�������̏����� */
int kzmem_init(void)
{
  int i;
  for (i = 0; i < MEMORY_AREA_NUM; i++) {
    kzmem_init_pool(&pool[i]); /* �e�������E�v�[�������������� */
  }
  return 0;
}

/* ���I�������̊l�� */
void *kzmem_alloc(int size)
{
  int i;
  kzmem_block *mp;
  kzmem_pool *p;

  for (i = 0; i < MEMORY_AREA_NUM; i++) {
    p = &pool[i];
    if (size <= p->size - sizeof(kzmem_block)) {
      if (p->free == NULL) { /* ����ςݗ̈悪����(�������E�u���b�N�s��) */
		kz_sysdown();
		return NULL;
      }
      /* ����ς݃����N���X�g����̈���擾���� */
      mp = p->free;
      p->free = p->free->next;
      mp->next = NULL;
      /*
       * ���ۂɗ��p�\�ȗ̈�́C�������E�u���b�N�\���̂̒���̗̈��
       * �Ȃ�̂ŁC����̃A�h���X��Ԃ��D
       */
      return mp + 1;
    }
  }

  /* �w�肳�ꂽ�T�C�Y�̗̈���i�[�ł��郁�����E�v�[�������� */
  kz_sysdown();
  return NULL;
}

/* �������̉�� */
void kzmem_free(void *mem)
{
  int i;
  kzmem_block *mp;
  kzmem_pool *p;

  /* �̈�̒��O�ɂ���(�͂���)�������E�u���b�N�\���̂��擾 */
  mp = ((kzmem_block *)mem - 1);

  for (i = 0; i < MEMORY_AREA_NUM; i++) {
    p = &pool[i];
    if (mp->size == p->size) {
      /* �̈������ς݃����N���X�g�ɖ߂� */
      mp->next = p->free;
      p->free = mp;
      return;
    }
  }

  kz_sysdown();
}


