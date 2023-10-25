/*
 * bt_dev.h
 *
 *  Created on: 2022/12/22
 *      Author: User
 */

#ifndef DEV_W25Q20EW_H_
#define DEV_W25Q20EW_H_

#include "defines.h"
#include "kozos.h"

typedef void (*W25Q20EW_CALLBACK)(void *vp);	// コールバック

extern int32_t w25q20ew_init(void);
extern int32_t w25q20ew_open(W25Q20EW_CALLBACK callback, void* callback_vp);
extern int32_t w25q20ew_close(void);
extern int32_t w25q20ew_write_enable(void);
extern int32_t w25q20ew_write_send(void);
extern int32_t w25q20ew_write_disable(void);
extern void w5q20ew_set_cmd(void);

#endif /* DEV_W25Q20EW_H_ */