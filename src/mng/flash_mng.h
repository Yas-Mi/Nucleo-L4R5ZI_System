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

// フラッシュ関数情報
typedef int32_t (*FLASH_MNG_INIT)(void);
typedef int32_t (*FLASH_MNG_OPEN)(void);
typedef int32_t (*FLASH_MNG_WRITE)(uint32_t addr, uint8_t *data, uint8_t size);
typedef int32_t (*FLASH_MNG_ERASE)(uint32_t addr);
typedef int32_t (*FLASH_MNG_READ)(uint32_t addr, uint8_t *data, uint8_t size);
typedef struct {
	FLASH_MNG_INIT			init;			// 初期化
	FLASH_MNG_OPEN			open;			// オープン
	FLASH_MNG_WRITE			write;			// ライト
	FLASH_MNG_ERASE			erase;			// イレース
	FLASH_MNG_READ			read;			// リード
} FLASH_MNG_FUNC;

extern int32_t flash_mng_init(void);
extern int32_t flash_mng_open(uint32_t kind);
extern int32_t flash_mng_write(uint32_t kind, uint32_t addr, uint8_t *data, uint32_t size);
extern int32_t flash_mng_erase(uint32_t kind, uint32_t addr);
extern int32_t flash_mng_read(uint32_t kind, uint32_t addr, uint8_t *data, uint32_t size);

extern void flash_mng_set_cmd(void);

#endif /* MNG_FLASH_MNG_H_ */
