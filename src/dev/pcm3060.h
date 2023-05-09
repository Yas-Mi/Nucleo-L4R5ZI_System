/*
 * pcm3060.h
 *
 *  Created on: 2022/10/29
 *      Author: User
 */

#ifndef DEV_PCM3060_H_
#define DEV_PCM3060_H_

extern void pcm3060_init(void);
extern int32_t pcm3060_open(uint32_t fs, uint8_t data_width);

#endif /* DEV_PCM3060_H_ */