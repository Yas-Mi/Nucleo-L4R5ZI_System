#ifndef _KOZOS_MEMORY_H_INCLUDED_
#define _KOZOS_MEMORY_H_INCLUDED_

int kzmem_init(void);        /* ���I�������̏����� */
void *kzmem_alloc(int size); /* ���I�������̊l�� */
void kzmem_free(void *mem);  /* �������̉�� */

#endif
