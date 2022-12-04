/*
 * lcd_dev.h
 *
 *  Created on: 2022/10/29
 *      Author: User
 */

#ifndef DEV_LCD_DEV_H_
#define DEV_LCD_DEV_H_

#define LCD_DISPLAY_WIDTH  (16U)
#define LCD_DISPLAY_HEIGHT (2U)

extern void LCD_dev_init(void);
extern int8_t LCD_dev_write(uint8_t x, uint8_t y, uint16_t *str);
extern int8_t LCD_dev_clear(uint8_t x, uint8_t y, uint8_t num);
extern int8_t LCD_dev_clear_disp(void);
extern int8_t LCD_dev_disp(void);

#endif /* DEV_LCD_DEV_H_ */
