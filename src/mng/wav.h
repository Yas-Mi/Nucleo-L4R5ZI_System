/*
 * wav.h
 *
 *  Created on: 2022/12/12
 *      Author: User
 */

#ifndef MNG_WAV_H_
#define MNG_WAV_H_

// チャンネル数
typedef enum {
	WAV_CH_MONO = 0,	// モノラル
	WAV_CH_STE,			// ステレオ
	WAV_CH_MAX
} WAV_CH;

typedef struct {
	uint32_t size;
	WAV_CH ch_num; 
	uint32_t sample_rate;
	uint16_t bps;
} WAV_INFO;

typedef void (*WAV_STA_CALLBACK)(WAV_INFO *wav_info, void *vp);	// 開始コールバック
typedef void (*WAV_RCV_CALLBACK)(int32_t data, void *vp);		// 受信コールバック
typedef void (*WAV_END_CALLBACK)(void *vp);						// 音楽終了コールバック

extern void wav_init(void);
extern int32_t wav_open(WAV_STA_CALLBACK sta_callback, WAV_RCV_CALLBACK rcv_callback, WAV_END_CALLBACK end_callback, void *callback_fp);

#endif /* MNG_WAV_H_ */
