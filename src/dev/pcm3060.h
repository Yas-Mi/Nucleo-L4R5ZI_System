/*
 * pcm3060.h
 *
 *  Created on: 2022/10/29
 *      Author: User
 */

#ifndef DEV_PCM3060_H_
#define DEV_PCM3060_H_

// コールバック関数定義
typedef void (*PCM3060_CALLBACK)(void *vp);

extern void pcm3060_init(void);
extern int32_t pcm3060_open(uint32_t fs, uint8_t data_width, PCM3060_CALLBACK callback, void* callback_vp);
extern int32_t pcm3060_play(uint32_t *data, uint32_t size);
#endif /* DEV_PCM3060_H_ */