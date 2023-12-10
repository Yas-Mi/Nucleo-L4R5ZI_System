
#ifndef DMA_H_
#define DMA_H_

#include "stm32l4xx.h"

// チャネル定義
// (*)DMA1、2がありそれぞれ7chずつあるが、DMA1しか使用しない
typedef enum {
	DMA_CH1 = 0,
	DMA_CH2,
	DMA_CH3,
	DMA_CH4,
	DMA_CH5,
	DMA_CH6,
	DMA_CH7,
	DMA_CH_MAX,
} DMA_CH;

// 転送単位
typedef enum {
	DMA_TRANSFER_UNIT_8BIT = 0,				// 8bit単位で転送
	DMA_TRANSFER_UNIT_16BIT,				// 16bit単位で転送
	DMA_TRANSFER_UNIT_32BIT,				// 32bit単位で転送
	DMA_TRANSFER_UNIT_MAX,					// 最大値
} DMA_TRANSFER_UNIT;

// 転送パラメータ
typedef struct {
	uint32_t			src_addr;			// 転送元のアドレス
	uint8_t				src_addr_inc;		// 転送元のアドレスをインクリメントするか (*)インクリメントする場合、transfer_unit単位でインクリメントされる
	uint32_t			dst_addr;			// 転送先のアドレス
	uint8_t				dst_addr_inc;		// 転送先のアドレスをインクリメントするか (*)インクリメントする場合、transfer_unit単位でインクリメントされる
	DMA_TRANSFER_UNIT	transfer_unit;		// 転送単位
	uint32_t			transfer_count;		// 転送回数 (総転送サイズ[byte] = transfer_unit * transfer_count)
} DMA_SEND;

// DMAリソース
#define DMA_RESOURCE_SAI1_A		(0)				// SAI1のA
#define DMA_RESOURCE_SAI1_B		(1)				// SAI1のB
#define DMA_RESOURCE_SAI2_A		(2)				// SAI2のA
#define DMA_RESOURCE_SAI2_B		(3)				// SAI2のB
#define DMA_RESOURCE_SPI1_RX	(4)				// SPI1_RX
#define DMA_RESOURCE_SPI1_TX	(5)				// SPI1_TX
#define DMA_RESOURCE_SPI2_RX	(6)				// SPI2_RX
#define DMA_RESOURCE_SPI2_TX	(7)				// SPI2_TX
#define DMA_RESOURCE_SPI3_RX	(8)				// SPI3_RX
#define DMA_RESOURCE_SPI3_TX	(9)				// SPI3_TX
#define DMA_RESOURCE_MAX 		(10)

// コールバック関数定義   
typedef void (*DMA_CALLBACK)(DMA_CH ch, int32_t ret, void *vp);

// 公開関数
extern void dma_init(void);
extern int32_t dma_open(DMA_CH ch, uint32_t resource, DMA_CALLBACK callback, void * callback_vp);
extern int32_t dma_start(DMA_CH ch, DMA_SEND *send_info);
extern int32_t dma_stop(DMA_CH ch);
extern int32_t dma_close(DMA_CH ch, USART_CALLBACK cb, void *vp);

#endif /* DMA_H_ */
