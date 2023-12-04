/*
 * MSP2807.h
 *
 *  Created on: 2023/11/27
 *      Author: User
 */

#ifndef DEV_MSP2807_H_
#define DEV_MSP2807_H_

#define MSP2807_DISPLAY_WIDTH	(240)
#define MSP2807_DISPLAY_HEIGHT	(320)

extern int32_t msp2807_init(void);
extern int32_t msp2807_open(void);
extern int32_t msp2807_write(uint16_t *disp_data);

#endif /* DEV_MSP2807_H_ */