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

typedef enum {
	MSP2807_PEN_STATE_RELEASE = 0,	// 離された
	MSP2807_PEN_STATE_PUSHED,		// 押された
	MSP2807_PEN_STATE_MAX,
} MSP2807_PEN_STATE;

extern int32_t msp2807_init(void);
extern int32_t msp2807_open(void);
extern int32_t msp2807_write(uint16_t *disp_data);
extern int32_t msp2807_get_touch_pos(uint16_t *x, uint16_t *y);
extern MSP2807_PEN_STATE msp2807_get_touch_state(void);

#endif /* DEV_MSP2807_H_ */