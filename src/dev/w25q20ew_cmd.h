/*
 * bt_dev.h
 *
 *  Created on: 2022/12/22
 *      Author: User
 */

#ifndef DEV_W25Q20EW_CMD_H_
#define DEV_W25Q20EW_CMD_H_

#include "defines.h"
#include "kozos.h"

typedef void (*W25Q20EW_CALLBACK)(void *vp);	// コールバック

extern int32_t w25q20ew_cmd_write_enable(void);
extern int32_t w25q20ew_cmd_wait_wel(uint32_t flag, uint32_t timeout);
extern int32_t w25q20ew_cmd_write_single(uint32_t addr, uint8_t *data, uint8_t size);
extern int32_t w25q20ew_cmd_write_quad(uint32_t addr, uint8_t *data, uint8_t size);
extern int32_t w25q20ew_cmd_erase(uint32_t addr);
extern int32_t w25q20ew_cmd_wait_wr_proc(uint32_t timeout);
extern int32_t w25q20ew_cmd_read_signle(uint32_t addr, uint8_t *data, uint8_t size);
extern int32_t w25q20ew_cmd_read_quad(uint32_t addr, uint8_t *data, uint8_t size);
extern int32_t w25q20ew_cmd_read_status_1(uint8_t *data);
extern int32_t w25q20ew_cmd_read_status_2(uint8_t *data);
extern int32_t w25q20ew_cmd_write_status_2(uint8_t *data);
extern int32_t w25q20ew_cmd_get_devise_id(uint8_t *id);
extern int32_t w25q20ew_cmd_get_sfdp(uint8_t *sfdp);
extern int32_t w25q20ew_cmd_write_disable(void);

#endif /* DEV_W25Q20EW_CMD_H_ */