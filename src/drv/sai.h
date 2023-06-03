
#ifndef sai_H_
#define sai_H_

#include "stm32l4xx.h"

// チャネル定義
typedef enum {
	SAI_CH1,
	SAI_CH_MAX,
} SAI_CH;

// モード
typedef enum {
	SAI_MODE_STEREO,			// ステレオ
	SAI_MODE_MONAURAL,			// モノラル
	SAI_MODE_MAX,				// 最大値
} SAI_MODE;

// フォーマット
typedef enum {
	SAI_FMT_I2S,				// I2S
	SAI_FMT_LSB_JUSTIFIED,		// 左寄せ
	SAI_FMT_MSB_JUSTIFIED,		// 右寄せ
	SAI_FMT_MAX,				// 最大値
} SAI_FMT;

// データの詰め方
typedef enum {
	SAI_PACK_LSB_FIRST,		// LSB First
	SAI_PACK_MSB_FIRST,		// MSB First
	SAI_PACK_MAX,			// 最大値
} SAI_PACK;

// データ幅
typedef enum {
	SAI_DATA_WIDTH_8,		// 8bit
	SAI_DATA_WIDTH_10,		// 10bit
	SAI_DATA_WIDTH_16,		// 16bit
	SAI_DATA_WIDTH_20,		// 20bit
	SAI_DATA_WIDTH_24,		// 24bit
	SAI_DATA_WIDTH_32,		// 32bit
	SAI_DATA_WIDTH_MAX,		// 最大値
} SAI_DATA_WIDTH;

// ビットクロック
typedef enum {
	SAI_BICK_TYPE_32FS,		// BICKはサンプリング周波数の32倍
	SAI_BICK_TYPE_48FS,		// BICKはサンプリング周波数の48倍
	SAI_BICK_TYPE_64FS,		// BICKはサンプリング周波数の64倍
	SAI_BICK_TYPE_MAX,		// 最大値
} SAI_BICK_TYPE;

typedef struct {
	SAI_MODE			mode;			// モード(ステレオ or モノラル)
	SAI_FMT				fmt;			// オーディオシリアル信号のフォーマット
	SAI_PACK			packing;		// データの詰め方
	SAI_DATA_WIDTH		width;			// データ幅
	uint32_t			bick;			// ビットクロック
	uint32_t			fs;				// サンプリング周波数
	uint8_t				is_mclk_used;	// マスタークロックを使用するかどうか
} SAI_OPEN;

// コールバック関数定義
typedef void (*SAI_CALLBACK)(SAI_CH ch, void *vp);

// 公開関数
extern void sai_init(void);
extern int32_t sai_open(SAI_CH ch, SAI_OPEN *par, SAI_CALLBACK callback, void *callback_vp);
extern int32_t sai_send(SAI_CH ch, uint32_t *data, uint32_t size);

#endif /* sai_H_ */