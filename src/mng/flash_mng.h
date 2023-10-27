/*
 * flash_mng.h
 *
 *  Created on: 2023/10/19
 *      Author: ronald
 */

#ifndef MNG_FLASH_MNG_H_
#define MNG_FLASH_MNG_H_

#define FLASH_MNG_KIND_W25Q20EW	(0)	// w25q20ew
#define FLASH_MNG_KIND_MAX		(1)	// 最大値

extern int32_t flash_mng_init(void);
extern int32_t flash_mng_open(uint32_t kind);
extern int32_t flash_mng_write(uint32_t kind, uint32_t addr, uint8_t *data, uint32_t size);
extern int32_t flash_mng_erase(uint32_t kind, uint32_t addr);
extern int32_t flash_mng_read(uint32_t kind, uint32_t addr, uint8_t *data, uint32_t size);

#endif /* MNG_FLASH_MNG_H_ */
