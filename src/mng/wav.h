/*
 * lcd_fw.h
 *
 *  Created on: 2022/12/12
 *      Author: User
 */

#ifndef MNG_WAV_H_
#define MNG_WAV_H_

typedef void (*WAV_RCV_CALLBACK)(int32_t data, void *vp);	// 受信コールバック
typedef void (*WAV_END_CALLBACK)(void *vp);					// 音楽終了コールバック

extern void wav_init(void);
extern int32_t wav_open(WAV_RCV_CALLBACK rcv_callback, WAV_END_CALLBACK end_callback, void *callback_fp);

#endif /* MNG_WAV_H_ */
