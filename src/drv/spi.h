
#ifndef SPI_H_
#define SPI_H_

// チャネル定義
typedef enum {
	SPI_CH1 = 0,
	SPI_CH2,
	SPI_CH3,
	SPI_CH_MAX,
} SPI_CH;

typedef enum {
	SPI_CPOL_POSITIVE = 0,			// 正極性：通信を行っていない間はSCLKはLow
	SPI_CPOL_NEGATIVE,				// 負極性：通信を行っていない間はSCLKはHigh
	SPI_CPOL_MAX,
} SPI_CPOL;

typedef enum {
	SPI_CPHA_FIRST_EDGE = 0,		// クロックの最初のエッジでサンプリング
	SPI_CPHA_LAST_EDGE,				// クロックの最後のエッジでサンプリング
	SPI_CPHA_MAX,
} SPI_CPHA;

typedef enum {
	SPI_FRAME_FMT_MSB_FIRST = 0,	// MSB Fisrt
	SPI_FRAME_FMT_LSB_FIRST,		// LSB Fisrt
	SPI_FRAME_FMT_MAX,
} SPI_FRAME_FMT;

typedef enum {
	SPI_DATA_SIZE_4BIT = 3,	// 4-bit
	SPI_DATA_SIZE_5BIT,		// 5-bit
	SPI_DATA_SIZE_6BIT,		// 6-bit
	SPI_DATA_SIZE_7BIT,		// 7-bit
	SPI_DATA_SIZE_8BIT,		// 8-bit
	SPI_DATA_SIZE_9BIT,		// 9-bit
	SPI_DATA_SIZE_10BIT,	// 10-bit
	SPI_DATA_SIZE_11BIT,	// 11-bit
	SPI_DATA_SIZE_12BIT,	// 12-bit
	SPI_DATA_SIZE_13BIT,	// 13-bit
	SPI_DATA_SIZE_14BIT,	// 14-bit
	SPI_DATA_SIZE_15BIT,	// 15-bit
	SPI_DATA_SIZE_16BIT,	// 16-bit
	SPI_DATA_SIZE_MAX,
} SPI_DATA_SIZE;

// オープンパラメータ
typedef struct {
	uint32_t		baudrate;	// 通信速度
	SPI_CPOL		cpol;		// Clock Polarity
	SPI_CPHA		cpha;		// Clock Phase
	SPI_FRAME_FMT	fmt;		// フレームフォーマット
	SPI_DATA_SIZE	size;		// データサイズ
} SPI_OPEN;

extern void spi_init(void);
extern int32_t spi_open(SPI_CH ch, SPI_OPEN *open_par);
extern int32_t spi_send_recv(SPI_CH ch, uint8_t *snd_data, uint32_t snd_sz, uint8_t *rcv_data, uint32_t rcv_size);

#endif /* SPI_H_ */