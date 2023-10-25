/*
 * flash_mng.h
 *
 *  Created on: 2023/10/19
 *      Author: ronald
 */

#ifndef MNG_FLASH_MNG_H_
#define MNG_FLASH_MNG_H_

extern int32_t flash_mng_init(void);
extern int32_t flash_mng_write(uint32_t addr, uint8_t *data, uint32_t size);
extern int32_t flash_mng_read(uint32_t addr, uint8_t *data, uint32_t size);

#endif /* MNG_FLASH_MNG_H_ */
