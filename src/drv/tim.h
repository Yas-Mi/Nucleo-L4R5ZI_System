
#ifndef TIM_H_
#define TIM_H_

#include "stm32l4xx.h"

// チャネル定義 (*)とりあえずGeneral-purpose timersだけ対応する
typedef enum {
//	TIM_CH1 = 0,	// Advanced-control timers (*) 未使用
	TIM_CH2 = 0,	// General-purpose timers (*) 未使用
	TIM_CH3,		// General-purpose timers (*) 未使用
	TIM_CH4,		// General-purpose timers (*) 未使用
	TIM_CH5,		// General-purpose timers (*) 未使用
//	TIM_CH6,		// Basic timers
//	TIM_CH7,		// Basic timers
//	TIM_CH8,		// Advanced-control timers (*) 未使用
	TIM_CH15,		// General-purpose timers
	TIM_CH16,		// General-purpose timers
	TIM_CH17,		// General-purpose timers
	TIM_CH_MAX,
} TIM_CH;

// 使用用途
typedef enum {
	TIM_MODE_INPUT_CAPTURE = 0,		// インプットキャプチャ
	TIM_MODE_OUTPUT_COMPARE,		// 未サポート
	TIM_MODE_PWM_GENERATE,			// 未サポート
	TIM_MODE_ONE_PULSE_OUTPUT,		// 未サポート
	TIM_MODE_MAX,					// 最大値
} TIM_MODE;

// オープンパラメータ
typedef struct {
	TIM_MODE	mode;		// 使用用途
} TIM_OPEN;

// コールバック関数定義   
typedef void (*TIM_INPUT_CAPTURE_CALLBACK)(TIM_CH ch, void *vp, uint32_t);

// 公開関数
extern void tim_init(void);
extern int32_t tim_open(TIM_CH ch, TIM_OPEN *open_par, TIM_INPUT_CAPTURE_CALLBACK callback, void * callback_vp);
extern int32_t tim_start_input_capture(TIM_CH ch);

#endif /* TIM_H_ */
