/*
 * bt_dev.h
 *
 *  Created on: 2022/12/22
 *      Author: User
 */

#ifndef DEV_W25Q20EW_CTRL_H_
#define DEV_W25Q20EW_CTRL_H_

extern int32_t w25q20ew_init(void);
extern int32_t w25q20ew_open(void);
extern int32_t w25q20ew_close(void);
extern int32_t w25q20ew_write(uint32_t addr, uint8_t *data, uint8_t size);
extern int32_t w25q20ew_erase(uint32_t addr);
extern int32_t w25q20ew_read(uint32_t addr, uint8_t *data, uint8_t size);
extern int32_t w25q20ew_get_devise_id(uint8_t *id);
extern int32_t w25q20ew_get_sfdp(uint8_t *sfdp);

extern void w25q20ew_set_cmd(void);

#endif /* DEV_W25Q20EW_CTRL_H_ */