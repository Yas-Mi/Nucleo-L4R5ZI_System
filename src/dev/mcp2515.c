/*
 * mcp2515.c
 *
 *  Created on: 2024/2/12
 *      Author: User
 */
#include <string.h>
#include <stdio.h>
#include "defines.h"
#include "kozos.h"
#include "stm32l4xx_hal_gpio.h"
#include "util.h"
#include "console.h"
#include "spi.h"
#include "intr.h"

#include "mcp2515.h"

// status
#define ST_NOT_INTIALIZED	(0U) // not initialized
#define ST_INITIALIZED		(1U) // initialized
#define ST_OPENED			(2U) // opened
#define ST_RUN				(3U) // running

// spi ch
#define USE_SPI_CH		(SPI_CH1)

// instruction set
#define MCP2515_INST_RESET			(0xC0)		// リセット
#define MCP2515_INST_READ_REG		(0x0C)		// レジスタ読み込み
#define MCP2515_INST_READ_RXBUF		(0x90)		// RXバッファ読み込み
#define MCP2515_INST_WRITE_REG		(0x02)		// レジスタ書き込み
#define MCP2515_INST_WRITE_TXBUF	(0x40)		// TXバッファ書き込み
#define MCP2515_INST_TXREQ			(0x80)		// メッセージ送信要求
#define MCP2515_INST_READ_STATUS	(0xA0)		// 状態読み込み
#define MCP2515_INST_READ_RXSTATUS	(0xB0)		// RX状態読み込み
#define MCP2515_INST_CHANGE_BIT		(0x05)		// ビット変更

// register define
// CTRL
#define MCP2515_REG_CANCTRL			(0x0F)		// CAN制御レジスタ
#define MCP2515_REG_CANSTAT			(0x0E)		// CAN状態レジスタ
// TX
#define MCP2515_REG_TXB0			(0x30)		// TX buffer 0
#define MCP2515_REG_TXB1			(0x40)		// TX buffer 1
#define MCP2515_REG_TXB2			(0x50)		// TX buffer 2
#define MCP2515_REG_TXRTSCTRL		(0x0D)		// TXnRTSピン制御と状態レジスタ
#define MCP2515_REG_TXB0S1DH		(0x31)		// 送信バッファ0用の標準識別子 上位
#define MCP2515_REG_TXB1S1DH		(0x41)		// 送信バッファ1用の標準識別子 上位
#define MCP2515_REG_TXB2S1DH		(0x51)		// 送信バッファ2用の標準識別子 上位
#define MCP2515_REG_TXB0S1DL		(0x32)		// 送信バッファ0用の標準識別子 下位
#define MCP2515_REG_TXB1S1DL		(0x42)		// 送信バッファ1用の標準識別子 下位
#define MCP2515_REG_TXB2S1DL		(0x52)		// 送信バッファ2用の標準識別子 下位
#define MCP2515_REG_TXB0E1D8		(0x33)		// 送信バッファ0用の拡張識別子 上位
#define MCP2515_REG_TXB1E1D8		(0x43)		// 送信バッファ1用の拡張識別子 上位
#define MCP2515_REG_TXB2E1D8		(0x53)		// 送信バッファ2用の拡張識別子 上位
#define MCP2515_REG_TXB1E1D0		(0x44)		// 送信バッファ1用の拡張識別子 下位
#define MCP2515_REG_TXB2E1D0		(0x54)		// 送信バッファ2用の拡張識別子 下位
#define MCP2515_REG_TXB0DLC			(0x35)		// 送信バッファ0用のデータ長コード
#define MCP2515_REG_TXB1DLC			(0x45)		// 送信バッファ1用のデータ長コード
#define MCP2515_REG_TXB2DLC			(0x55)		// 送信バッファ2用のデータ長コード
#define MCP2515_REG_TXB0D_BASE		(0x36)		// 送信バッファ0用のデータバイト数 ベースアドレス
#define MCP2515_REG_TXB1D_BASE		(0x46)		// 送信バッファ0用のデータバイト数 ベースアドレス
#define MCP2515_REG_TXB2D_BASE		(0x56)		// 送信バッファ0用のデータバイト数 ベースアドレス
// RX
#define MCP2515_REG_RXB0CTRL		(0x60)		// 受信バッファ0制御
#define MCP2515_REG_RXB1CTRL		(0x70)		// 受信バッファ1制御
#define MCP2515_REG_BFPCTRL			(0x0C)		// RXnBF ピン制御と状態
#define MCP2515_REG_RXB0S1DH		(0x61)		// 受信バッファ0用の標準識別子 上位
#define MCP2515_REG_RXB1S1DH		(0x71)		// 受信バッファ1用の標準識別子 上位
#define MCP2515_REG_RXB0S1DL		(0x62)		// 受信バッファ0用の標準識別子 下位
#define MCP2515_REG_RXB1S1DL		(0x72)		// 受信バッファ1用の標準識別子 下位
#define MCP2515_REG_RXB0E1D8		(0x63)		// 受信バッファ0用の拡張識別子 上位
#define MCP2515_REG_RXB1E1D8		(0x73)		// 受信バッファ1用の拡張識別子 上位
#define MCP2515_REG_RXB0E1D0		(0x64)		// 受信バッファ0用の拡張識別子 下位
#define MCP2515_REG_RXB1E1D0		(0x74)		// 受信バッファ1用の拡張識別子 下位
#define MCP2515_REG_RXB0DLC			(0x65)		// 受信バッファ0用のデータ長コード
#define MCP2515_REG_RXB1DLC			(0x75)		// 受信バッファ1用のデータ長コード
#define MCP2515_REG_RXB0D_BASE		(0x66)		// 受信バッファ0用のデータバイト数 ベースアドレス
#define MCP2515_REG_RXB1D_BASE		(0x76)		// 受信バッファ0用のデータバイト数 ベースアドレス
#define MCP2515_REG_RXF0S1DH		(0x00)		// フィルタ0の標準識別子 上位
#define MCP2515_REG_RXF1S1DH		(0x04)		// フィルタ1の標準識別子 上位
#define MCP2515_REG_RXF2S1DH		(0x08)		// フィルタ2の標準識別子 上位
#define MCP2515_REG_RXF3S1DH		(0x10)		// フィルタ3の標準識別子 上位
#define MCP2515_REG_RXF4S1DH		(0x14)		// フィルタ4の標準識別子 上位
#define MCP2515_REG_RXF5S1DH		(0x18)		// フィルタ5の標準識別子 上位
#define MCP2515_REG_RXF0S1DL		(0x01)		// フィルタ0の標準識別子 下位
#define MCP2515_REG_RXF1S1DL		(0x05)		// フィルタ1の標準識別子 下位
#define MCP2515_REG_RXF2S1DL		(0x09)		// フィルタ2の標準識別子 下位
#define MCP2515_REG_RXF3S1DL		(0x11)		// フィルタ3の標準識別子 下位
#define MCP2515_REG_RXF4S1DL		(0x15)		// フィルタ4の標準識別子 下位
#define MCP2515_REG_RXF5S1DL		(0x19)		// フィルタ5の標準識別子 下位
#define MCP2515_REG_RXF0E1D8		(0x02)		// フィルタ0の拡張識別子 上位
#define MCP2515_REG_RXF1E1D8		(0x05)		// フィルタ1の拡張識別子 上位
#define MCP2515_REG_RXF2E1D8		(0x0A)		// フィルタ2の拡張識別子 上位
#define MCP2515_REG_RXF3E1D8		(0x12)		// フィルタ3の拡張識別子 上位
#define MCP2515_REG_RXF4E1D8		(0x16)		// フィルタ4の拡張識別子 上位
#define MCP2515_REG_RXF5E1D8		(0x1A)		// フィルタ5の拡張識別子 上位
#define MCP2515_REG_RXF0E1D0		(0x03)		// フィルタ0の拡張識別子 下位
#define MCP2515_REG_RXF1E1D0		(0x07)		// フィルタ1の拡張識別子 下位
#define MCP2515_REG_RXF2E1D0		(0x0B)		// フィルタ2の拡張識別子 下位
#define MCP2515_REG_RXF3E1D0		(0x13)		// フィルタ3の拡張識別子 下位
#define MCP2515_REG_RXF4E1D0		(0x17)		// フィルタ4の拡張識別子 下位
#define MCP2515_REG_RXF5E1D0		(0x1B)		// フィルタ5の拡張識別子 下位
#define MCP2515_REG_RXM0S1DH		(0x20)		// マスク0の標準識別子 上位
#define MCP2515_REG_RXM1S1DH		(0x24)		// マスク1の標準識別子 上位
#define MCP2515_REG_RXM0S1DL		(0x21)		// マスク0の標準識別子 下位
#define MCP2515_REG_RXM1S1DL		(0x25)		// マスク1の標準識別子 下位
// configuratuin
#define MCP2515_REG_CNF1			(0x2A)		// コンフィグレーション1
#define MCP2515_REG_CNF2			(0x29)		// コンフィグレーション2
#define MCP2515_REG_CNF3			(0x28)		// コンフィグレーション3
// status
#define MCP2515_REG_TEC				(0x1C)		// 送信エラーカウンタ
#define MCP2515_REG_REC				(0x1D)		// 受信エラーカウンタ
#define MCP2515_REG_EFLG			(0x2D)		// エラーフラグ
// interrupt
#define MCP2515_REG_CANINTE			(0x2B)		// 割込み許可
#define MCP2515_REG_CANINTF			(0x2B)		// 割込みフラグ

// bit define
// CANCTRL
#define CANCTRL_REQOP				(((v) & 0x7) << 5)		// 動作モード要求ビット
#define CANCTRL_ABAT				(1 << 4)				// すべての送信の停止ビット
#define CANCTRL_OSM					(1 << 3)				// ワンショットモードビット
#define CANCTRL_CLKEN				(1 << 2)				// CLKOUTピン有効化ビット
#define CANCTRL_CLKPRE				(((v) & 0x3) << 0)		// CLKOUTピン分周設定ビット
// CANSTAT
#define CANSTAT_OPMOD				(((v) & 0x7) << 5)		// 動作モードビット
#define CANSTAT_ICOD				(((v) & 0x7) << 1)		// 割込みフラグコードビット
// TXB
#define TXB_ABTF					(1 << 6)				// メッセージ停止フラグ
#define TXB_MLOA					(1 << 6)				// メッセージはアービトレーションを失った
#define TXB_TXERR					(1 << 6)				// 送信エラー検出ビット
#define TXB_TXREQ					(1 << 6)				// メッセージ送信要求ビット
#define TXB_TXP(v)					(((v) & 0x3) << 0)		// 送信バッファ優先順位
// TXRTSCTRL
#define TXRTSCTRL_B2RTS				(1 << 5)				// TX2RTSピン状態ビット
#define TXRTSCTRL_B1RTS				(1 << 4)				// TX1RTSピン状態ビット
#define TXRTSCTRL_B0RTS				(1 << 3)				// TX0RTSピン状態ビット
#define TXRTSCTRL_B2RTSM			(1 << 2)				// TX2RTSピンモードビット
#define TXRTSCTRL_B1RTSM			(1 << 1)				// TX1RTSピンモードビット
#define TXRTSCTRL_B0RTSM			(1 << 0)				// TX0RTSピンモードビット
// TXB0S1D
#define TXB0S1D_EXDIE				(1 << 3)				// 拡張識別子イネーブルビット
#define TXB0S1D_EID					(1 << 3)				// 拡張識別子ビット
// TXBDLC
#define TXBDLC_RTR					(1 << 6)				// リモート送信要求ビット
#define TXBDLC_DLC(v)				(((v) & 0xF) << 0)		// データ長コード
// RXB0CTRL
#define RXB0CTRL_RXM					(((v) & 0x3) << 5)	// 受信バッファ動作モードビット
#define RXB0CTRL_RXRTR				(1 << 3)				// リモート送信要求ビットを受信した
#define RXB0CTRL_BUKT				(1 << 2)				// 切り替え許可ビット
#define RXB0CTRL_BUKT1				(1 << 1)				// 上記と同じ
#define RXB0CTRL_FILHIT				(1 << 0)				// フィルタ一致ビット
// RXB1CTRL
#define RXB1CTRL_RXM				(((v) & 0x3) << 5)		// 受信バッファ動作モードビット
#define RXB1CTRL_RXRTR				(1 << 3)				// リモート送信要求ビットを受信した
#define RXB1CTRL_FILHIT				(((v) & 0x7) << 0)		// フィルタ一致ビット
// BFPCTRL
#define BFPCTRL_B1BFS				(1 << 5)				// RX1BFピン状態ビット
#define BFPCTRL_B0BFS				(1 << 4)				// RX0BFピン状態ビット
#define BFPCTRL_B1BFE				(1 << 3)				// RX1BFピン機能有効化ビット
#define BFPCTRL_B0BFE				(1 << 2)				// RX0BFピン機能有効化ビット
#define BFPCTRL_B1BFM				(1 << 1)				// RX1BFピン動作モードビット
#define BFPCTRL_B0BFM				(1 << 0)				// RX0BFピン動作モードビット
// RXBS1DL
#define RXBS1DL_SRR					(1 << 4)				// 標準フレームのリモート送信要求ビット
#define RXBS1DL_IDE					(1 << 3)				// 標準フレームのリモート送信要求ビット
#define RXBS1DL_EID					(((v) & 0x3) << 0)		// 標準フレームのリモート送信要求ビット
// RXBDLC
#define RXBDLC_RTR					(1 << 6)				// リモート送信要求ビット
#define RXBDLC_RB1					(1 << 5)				// リモート送信要求ビット
#define RXBDLC_RB0					(1 << 4)				// リモート送信要求ビット
#define RXBDLC_DLC(v)				(((v) & 0xF) << 0)		// データ長コード
// RXFS1DL
#define RXFS1DL_EXDIE				(1 << 3)				// 拡張識別子有効化ビット
#define RXFS1DL_EID					(1 << 3)				// 拡張識別子ビット
// RXMS1DL
#define RXMS1DL_EID					(((v) & 0x3) << 0)		// 拡張識別子マスクビット
// CNF1
#define CNF1_SJW					(((v) & 0x3) << 6)		// 再同期ジャンプ幅長ビット
#define CNF1_BRP					(((v) & 0x3F) << 0)		// ボーレート分周ビット
// CNF2
#define CNF2_BTLMODE				(1 << 7)				// PS2ビットタイム長ビット
#define CNF2_SAM					(1 << 6)				// サンプル店コンフィグレーションビット
#define CNF2_PHSEG1					(((v) & 0x3) << 3)		// PS1長さビット
#define CNF2_PHSEG					(((v) & 0x3) << 0)		// 伝搬セグメント長ビット
// CNF3
#define CNF3_SOF					(1 << 7)				// スタートオブフレーム信号ビット
#define CNF3_WAKFIL					(1 << 6)				// ウェイクアップフィルタビット
#define CNF3_PHSEG2					(((v) & 0x3) << 0)		// PS2長さビット
// EFLG
#define EFLG_RX1OVR					(1 << 7)				// 受信バッファ1オーバーフローフラグビット
#define EFLG_RX0OVR					(1 << 6)				// 受信バッファ0オーバーフローフラグビット
#define EFLG_TXBO					(1 << 5)				// バスオフエラーフラグビット
#define EFLG_TXEP					(1 << 4)				// 送信エラー パッシブエラーフラグビット
#define EFLG_RXEP					(1 << 3)				// 受信エラー パッシブフラグビット
#define EFLG_TXWAR					(1 << 2)				// 送信エラー警告フラグビット
#define EFLG_RXWAR					(1 << 1)				// 受信エラー警告フラグビット
#define EFLG_EWARN					(1 << 0)				// 受信バッファ0オーバーフローフラグビット
// CANINTE
#define CANINTE_MERRE				(1 << 7)				// メッセージエラー割込み許可ビット
#define CANINTE_WAKIEE				(1 << 6)				// ウェイクアップ割込み許可ビット
#define CANINTE_ERRIE				(1 << 5)				// エラー割込み許可ビット
#define CANINTE_TX2IE				(1 << 4)				// 送信バッファ2空割込み許可ビット
#define CANINTE_TX1IE				(1 << 3)				// 送信バッファ1空割込み許可ビット
#define CANINTE_TX0IE				(1 << 2)				// 送信バッファ0空割込み許可ビット
#define CANINTE_RX1IE				(1 << 1)				// 受信バッファ1フル割込み許可ビット
#define CANINTE_RX0IE				(1 << 0)				// 受信バッファ0フル割込み許可ビット
// CANINTF
#define CANINTF_MERRF				(1 << 7)				// メッセージエラー割込みフラグビット
#define CANINTF_WAKIEF				(1 << 6)				// ウェイクアップ割込みフラグビット
#define CANINTF_ERRIF				(1 << 5)				// エラー割込みフラグビット
#define CANINTF_TX2IF				(1 << 4)				// 送信バッファ2空割込みフラグビット
#define CANINTF_TX1IF				(1 << 3)				// 送信バッファ1空割込みフラグビット
#define CANINTF_TX0IF				(1 << 2)				// 送信バッファ0空割込みフラグビット
#define CANINTF_RX1IF				(1 << 1)				// 受信バッファ1フル割込みフラグビット
#define CANINTF_RX0IF				(1 << 0)				// 受信バッファ0フル割込みフラグビット

typedef struct {
	uint32_t	status;
	
} MCP2515_CTL;
static MCP2515_CTL mcp2515_ctl;

// spi open parameter
static const SPI_OPEN spi_open_par = {
	1000*1000,					// speed 1MHz
	SPI_CPOL_NEGATIVE,			// Clock Polarity
	SPI_CPHA_FIRST_EDGE,    	// Clock Phase
	SPI_FRAME_FMT_MSB_FIRST,	// フレームフォーマット
	SPI_DATA_SIZE_8BIT,			// データサイズ
};

// 割込み情報
static void exti5_9_handler(void);
typedef struct {
	IRQn_Type 		irq_type;							// 割込みタイプ
	INT_HANDLER 	handler;							// 割込みハンドラ
	uint32_t		vec_no;								// 割込み番号
	uint32_t		int_priority;						// 割込み優先度
} INT_CFG;
static const INT_CFG int_cfg = {
	{EXTI9_5_IRQn, exti5_9_handler, EXTI9_5_INTERRUPT_NO, INTERRPUT_PRIORITY_5},
};
#define get_ire_type()		(int_cfg.irq_type)			// 割込みタイプ取得マクロ
#define get_handler()		(int_cfg.handler)			// 割り込みハンドラ取得マクロ
#define get_vec_no()		(int_cfg.vec_no)			// 割り込み番号取得マクロ
#define get_int_prio()		(int_cfg.int_priority)		// 割り込み優先度取得マクロ

/* 割込みハンドラ */
void exti5_9_handler(void){
	;
}

// リセット
static int32_t can_reset(void)
{
	uint8_t snd_data;
	uint32_t ret;
	
	// データ設定
	snd_data = MCP2515_INST_RESET;
	
	// SPI送信
	ret = spi_send_recv(USE_SPI_CH, &snd_data, NULL, 1);
	
	return ret;
}

// レジスタリード
static int32_t can_reg_read(uint8_t reg, uint8_t *data)
{
	uint8_t snd_data[3];
	uint8_t rcv_data[3];
	uint32_t ret;
	
	// データ設定
	snd_data[0] = MCP2515_INST_READ_REG;
	snd_data[1] = reg;
	snd_data[2] = 0x00;	// (*)ダミー
	
	// SPI送信
	ret = spi_send_recv(USE_SPI_CH, snd_data, rcv_data, 3);
	
	// 受信データ設定
	*data = rcv_data[2];
	
	return ret;
}

// レジスタライト
static int32_t can_reg_write(uint8_t reg, uint8_t data)
{
	uint8_t snd_data[3];
	uint32_t ret;
	
	// データ設定
	snd_data[0] = MCP2515_INST_WRITE_REG;
	snd_data[1] = reg;
	snd_data[2] = data;
	
	// SPI送信
	ret = spi_send_recv(USE_SPI_CH, snd_data, NULL, 3);
	
	return ret;
}

// 状態読み込み
static int32_t can_reg_read_status(uint8_t *data)
{
	uint8_t snd_data[2];
	uint8_t rcv_data[2];
	uint32_t ret;
	
	// データ設定
	snd_data[0] = MCP2515_INST_READ_STATUS;
	snd_data[1] = 0x00;	// (*)ダミー
	
	// SPI送信
	ret = spi_send_recv(USE_SPI_CH, snd_data, rcv_data, 2);
	
	// 受信データ設定
	*data = rcv_data[1];
	
	return ret;
}

// RX状態読み込み
static int32_t can_reg_read_rxstatus(uint8_t *data)
{
	uint8_t snd_data[2];
	uint8_t rcv_data[2];
	uint32_t ret;
	
	// データ設定
	snd_data[0] = MCP2515_INST_READ_RXSTATUS;
	snd_data[1] = 0x00;	// (*)ダミー
	
	// SPI送信
	ret = spi_send_recv(USE_SPI_CH, snd_data, rcv_data, 2);
	
	// 受信データ設定
	*data = rcv_data[1];
	
	return ret;
}

// MCP2515デバイス初期化
// 参考:http://reclearnengoolong.blog.fc2.com/blog-entry-1666.html
static int32_t mcp2515_init(void)
{
	uint32_t ret;
	
	// リセット処理
	ret = can_reset();
	if (ret != E_OK) {
		goto MCP2515_INIT_EXIT;
	}
	
MCP2515_INIT_EXIT:
	if (ret != E_OK) {
		console_str_send("mcp2515_init spi_send_recv error\n");
	}
	
	return ret;
}

// 外部公開関数
// 初期化関数
void mcp2515_dev_init(void)
{
	MCP2515_CTL *this = &mcp2515_ctl;
	
	// 制御ブロックの初期化
	memset(this, 0, sizeof(MCP2515_CTL));
	
	// 割込みハンドラ登録
	kz_setintr(get_vec_no(), get_handler());
	
	// 状態の更新
	this->status = ST_INITIALIZED;
	
	return;
}

// オープン関数
int32_t mcp2515_dev_open(void)
{
	MCP2515_CTL *this = &mcp2515_ctl;
	uint32_t ret;
	
	// 未初期化の場合はエラーを返して終了
	if (this->status != ST_INITIALIZED) {
		return -1;
	}
	
	// spi open
	ret = spi_open(USE_SPI_CH, &spi_open_par);
	if (ret != E_OK) {
		console_str_send("spi_open error\n");
		goto OPEN_ERROR;
	}
	
	// 割り込み有効
	HAL_NVIC_SetPriority(get_ire_type(), get_int_prio(), 0);
	HAL_NVIC_EnableIRQ(get_ire_type());
	
	// デバイス初期化
	ret = mcp2515_init();
	if (ret != E_OK) {
		console_str_send("mcp2515_init error\n");
		goto OPEN_ERROR;
	}
	
	// 状態を更新
 	this->status = ST_OPENED;
	
OPEN_ERROR:
	return ret;
}

// コマンド
static void mcp2515_cmd_open(int argc, char *argv[])
{
	uint32_t ret;
	
	ret = mcp2515_dev_open();
	if (ret != E_OK) {
		console_str_send("mcp2515_dev_open error\n");
	}
}

static void mcp2515_cmd_read_reg(int argc, char *argv[])
{
	uint32_t ret;
	uint8_t reg;
	uint8_t data = 0;
	
	// 引数チェック
	if (argc < 2) {
		console_str_send("flash_mng read <addr> <byte access 0: 1byte access 1: 4byte access>\n");
		return;
	}
	
	reg = atoi(argv[2]);
	
	ret = can_reg_read(reg, &data);
	if (ret != E_OK) {
		console_str_send("can_reg_read error\n");
		goto CMD_READ_REG_EXIT;
	}
	
	console_str_send("val:0x");
	console_val_send_hex(data, 2);
	console_str_send("\n");
	
CMD_READ_REG_EXIT:
	return;
}

// コマンド設定関数
void mcp2515_set_cmd(void)
{
	COMMAND_INFO cmd;
	
	// コマンドの設定
	cmd.input = "mcp2515 open";
	cmd.func = mcp2515_cmd_open;
	console_set_command(&cmd);
	cmd.input = "mcp2515 read_reg";
	cmd.func = mcp2515_cmd_read_reg;
	console_set_command(&cmd);
}
