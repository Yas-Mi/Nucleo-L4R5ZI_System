/*
 * gysfdmaxb.h
 *
 *  Created on: 2023/11/1
 *      Author: ronald
 */

#ifndef DEV_GYSFDMAXB_H_
#define DEV_GYSFDMAXB_H_

#include "defines.h"
#include "kozos.h"

typedef void (*GYSFDMAXB_CALLBACK)(void *vp);	// コールバック

extern int32_t gysfdmaxb_init(void);
extern void gysfdmaxb_set_cmd(void);

#endif /* DEV_GYSFDMAXB_H_ */