/*
 * octspi.h
 *
 *  Created on: 2022/08/19
 *      Author: User
 */
#ifndef DRV_OCTSPI_H_
#define DRV_OCTSPI_H_

// チャンネル情報
typedef enum {
	OCTOSPI_CH_1 = 0,
	OCTOSPI_CH_2,
	OCTOSPI_CH_MAX,
} OCTOSPI_CH;

// アクセス方法
typedef enum {
	OCTOSPI_OPE_MODE_INDIRECT = 0,
	// OCTOSPI_OPE_MODE_STATUS_POLLING,		// ★未サポート
	OCTOSPI_OPE_MODE_MEMORY_MAPPED,
	OCTOSPI_OPE_MODE_MAX,
} OCTOSPI_OPE_MODE;

// インタフェース
typedef enum {
	OCTOSPI_IF_NONE = 0,
	OCTOSPI_IF_SINGLE,
	OCTOSPI_IF_DUAL,
	OCTOSPI_IF_QUAD,
	OCTOSPI_IF_OCTO,
	OCTOSPI_IF_MAX,
} OCTOSPI_IF;

// メモリタイプ
typedef enum {
	OCTOSPI_MEM_MICRON = 0,			// D0/D1 DTR8bitモード 
	OCTOSPI_MEM_MACRONIX,			// D1/D0 DTR8bitモード
	OCTOSPI_MEM_STANDARD,			// standard
	OCTOSPI_MEM_MACRONIX_RAM,		// D1/D0 DTR8bitモード
	OCTOSPI_MEM_HYPERBUS_MEMORY,	// HyperBus
	OCTOSPI_MEM_HYPERBUS_REGISTER,	// 
	OCTOSPI_MEM_MAX,
} OCTOSPI_MEM;

// クロック動作設定
typedef enum {
	OCTOSPI_CLKMODE_LOW,		// CLK must stay low while NCS is high
	OCTOSPI_CLKMODE_HIGH,		// CLK must stay high while NCS is high
	OCTOSPI_CLKMODE_MAX,
} OCTOSPI_CLKMODE;

// オープンパラメータ
typedef struct {
	uint32_t clk;				// 通信速度
	OCTOSPI_OPE_MODE ope_mode;	// アクセス方法
	OCTOSPI_IF interface;		// メモリインタフェース
	OCTOSPI_MEM mem_type;		// メモリタイプ
	uint32_t size;				// サイズ
	uint8_t dual_mode;			// デュアルモードかどうか 1:デュアルモード 0:デュアルモードでない
	uint8_t dqs_used;			// DQSを使用するかどうか 1:dqsを使用する 0:dqsを使用しない
	uint8_t nclk_used;			// NCLK(CLKの反転)を使用するかどうか 1:NCLKを使用する 0:NCLKを使用しない
	OCTOSPI_CLKMODE clk_mode;	// NCSがhighのときCLKをどうするか
	uint8_t ssio_used;			// Send instruction only once mode
} OCTOSPI_OPEN;

// コールバック関数定義
typedef void (*OCTOSPI_CALLBACK)(OCTOSPI_CH ch, void *vp);

// サイズ
typedef enum {
	OCTOSPI_SZ_8BIT,
	OCTOSPI_SZ_16BIT,
	OCTOSPI_SZ_24BIT,
	OCTOSPI_SZ_32BIT,
	OCTOSPI_SZ_MAX,
} OCTOSPI_SZ;

// 通信パラメータ
typedef struct {
	uint32_t	inst;				// 命令
	OCTOSPI_SZ	inst_size;			// 命令サイズ
	OCTOSPI_IF	inst_if;			// 命令インタフェース
	uint32_t	addr;				// アドレス
	OCTOSPI_SZ	addr_size;			// アドレスサイズ
	OCTOSPI_IF	addr_if;			// アドレスインタフェース
	uint32_t	dummy_cycle;		// ダミーサイクル
	uint32_t	data_size;			// データサイズ
	OCTOSPI_IF	data_if;			// データインタフェース
	uint8_t		*data;				// データ
	OCTOSPI_SZ	alternate_size;		// オルタナティブサイズ
	OCTOSPI_IF	alternate_if;		// オルタナティブインタフェース
	uint32_t	alternate_all_size;	// オルタナティブサイズ
} OCTOSPI_COM_CFG;

// 公開関数
extern void octospi_init(void);
extern int32_t octospi_open(OCTOSPI_CH ch, OCTOSPI_OPEN *par, OCTOSPI_CALLBACK callback_fp, void *callback_vp);
extern int32_t octspi_send(uint32_t ch, OCTOSPI_COM_CFG *cfg);
extern int32_t octspi_recv(uint32_t ch, OCTOSPI_COM_CFG *cfg);

#endif
