#ifndef _KOZOS_MEMORY_H_INCLUDED_
#define _KOZOS_MEMORY_H_INCLUDED_

int kzmem_init(void);        /* “®“Iƒƒ‚ƒŠ‚Ì‰Šú‰» */
void *kzmem_alloc(int size); /* “®“Iƒƒ‚ƒŠ‚ÌŠl“¾ */
void kzmem_free(void *mem);  /* ƒƒ‚ƒŠ‚Ì‰ğ•ú */

#endif
