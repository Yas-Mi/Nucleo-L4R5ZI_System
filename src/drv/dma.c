/* Includes ------------------------------------------------------------------*/
#include "string.h"
#include "defines.h"
#include "kozos.h"
#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_pwr_ex.h"
#include "stm32l4xx_hal_cortex.h"
#include "stm32l4xx.h"
#include "dma.h"
#include "intr.h"
#include "system_def.h"

// マクロ
#define MAX_TRANSFER_COUNT	(0xFFFF)	// 最大送信回数

// 状態
#define ST_NOT_INTIALIZED	(0U)	// 未初期化
#define ST_INTIALIZED		(1U)	// 初期化済
#define ST_OPENED			(2U)	// オープン済
#define ST_RUN				(3U)	// 実行中

//DMAMUX ベースレジスタ
#define DMAMUX1_BASE_ADDR	(0x40020800)
#define DMA1_BASE_ADDR		(0x40020000)
#define DMA2_BASE_ADDR		(0x40020400)

// レジスタ設定値
// DMAMUX

// DMA
// CCR
#define _CCR_MEM2MEM		(1UL << 14)
#define _CCR_PL(v)		(((v) & 0x3) << 12)
#define _CCR_MSIZE(v)	(((v) & 0x3) << 10)
#define _CCR_PSIZE(v)	(((v) & 0x3) << 8)
#define _CCR_MINC		(1UL << 7)
#define _CCR_PINC		(1UL << 6)
#define _CCR_CIRC		(1UL << 5)
#define _CCR_DIR			(1UL << 4)
#define _CCR_TEIE		(1UL << 3)
#define _CCR_HTIE		(1UL << 2)
#define _CCR_TCIE		(1UL << 1)
#define _CCR_EN			(1UL << 0)

// レジスタ設定マクロ

// DMAMUXレジスタ定義
struct stm32l4_dmamux {
	volatile uint32_t ccr[14];		
	volatile uint8_t reserved0[72];
	volatile uint32_t csr;
	volatile uint32_t cfr;
	volatile uint8_t reserved1[117];
	volatile uint32_t rg1cr;
	volatile uint32_t rg2cr;
	volatile uint32_t rg3cr;
	volatile uint8_t reserved2[51];
	volatile uint32_t rgsr;
	volatile uint32_t rgsfr;
	volatile uint8_t reserved3[693];
};

// DMA1レジスタ定義
typedef struct {
	volatile uint32_t ccr;
	volatile uint32_t cndtr;
	volatile uint32_t cpar;
	volatile uint32_t cmar;
	volatile uint32_t reserved;
}COMMON_REG;
struct stm32l4_dma {
	volatile uint32_t isr;
	volatile uint32_t ifcr;
	volatile COMMON_REG commonn_reg[7];
};

// 転送のパターン
typedef enum {
	DMA_TRANSFER_PTN_M2M = 0,				// メモリーからメモリー
	DMA_TRANSFER_PTN_P2P,					// ペリフェラルからペリフェラル
	DMA_TRANSFER_PTN_M2P,					// メモリーからペリフェラル
	DMA_TRANSFER_PTN_P2M,					// ペリフェラルからメモリー
	DMA_TRANSFER_PTN_MAX,					// 最大値
} DMA_TRANSFER_PTN;

// DMA制御ブロック
typedef struct {
	uint32_t		status;					// 状態
	DMA_CALLBACK	callback;				// コールバック
	void			*callback_vp;			// コールバックパラメータ
	// 拡張転送関数用
	uint32_t		remain_transfer_count;	// 残り送信回数
	DMA_SEND		send_info;				// 送信情報
} DMA_CTL;
static DMA_CTL dma_ctl[DMA_CH_MAX];
#define get_myself(n) (&dma_ctl[(n)])

// 割込みハンドラのプロトタイプ
void dma1_handler(void);
void dma2_handler(void);
void dma3_handler(void);
void dma4_handler(void);
void dma5_handler(void);
void dma6_handler(void);
void dma7_handler(void);

// DMAチャネル固有情報
typedef struct {
	IRQn_Type irq_type;									// 割込みタイプ
	INT_HANDLER handler;								// 割込みハンドラ
	uint32_t	vec_no;									// 割込み番号
} DMA_CFG;

// DMAチャネル固有情報テーブル
static const DMA_CFG dma_cfg[DMA_CH_MAX] =
{
	{DMA1_Channel1_IRQn, dma1_handler, DMA1_CH1_GLOBAL_INTERRUPT_NO},
	{DMA1_Channel2_IRQn, dma2_handler, DMA1_CH2_GLOBAL_INTERRUPT_NO},
	{DMA1_Channel3_IRQn, dma3_handler, DMA1_CH3_GLOBAL_INTERRUPT_NO},
	{DMA1_Channel4_IRQn, dma4_handler, DMA1_CH4_GLOBAL_INTERRUPT_NO},
	{DMA1_Channel5_IRQn, dma5_handler, DMA1_CH5_GLOBAL_INTERRUPT_NO},
	{DMA1_Channel6_IRQn, dma6_handler, DMA1_CH6_GLOBAL_INTERRUPT_NO},
	{DMA1_Channel7_IRQn, dma7_handler, DMA1_CH7_GLOBAL_INTERRUPT_NO},
};
#define get_ire_type(ch)	(dma_cfg[ch].irq_type)			// 割込みタイプ取得マクロ
#define get_handler(ch)		(dma_cfg[ch].handler)				// 割り込みハンドラ取得マクロ
#define get_vec_no(ch)		(dma_cfg[ch].vec_no)				// 割り込み番号取得マクロ

// DMA Resouce 変換テーブル
static const uint32_t dma_resource_cnv_tbl[DMA_RESOURCE_MAX] = 
{
	36,		// DMA_RESOURCE_SAI1_A
	37,		// DMA_RESOURCE_SAI1_B
	38,		// DMA_RESOURCE_SAI2_A
	39,		// DMA_RESOURCE_SAI2_B
	10,		// DMA_RESOURCE_SPI1_RX
	11,		// DMA_RESOURCE_SPI1_TX
	12,		// DMA_RESOURCE_SPI2_RX
	13,		// DMA_RESOURCE_SPI2_TX
	14,		// DMA_RESOURCE_SPI3_RX
	15,		// DMA_RESOURCE_SPI3_TX
};

// 割込み共通ハンドラ
static void dma_common_handler(DMA_CH ch)
{
	volatile struct stm32l4_dma *dma = (struct stm32l4_dma*)DMA1_BASE_ADDR;
	DMA_CTL *this = get_myself(ch);
	uint32_t global_bit_pos = ((ch * 4) + 0);
	uint32_t comp_bit_pos =   ((ch * 4) + 1);
	uint32_t half_bit_pos =   ((ch * 4) + 2);
	uint32_t err_bit_pos =    ((ch * 4) + 3);
	DMA_SEND send_info;
	
	// まずはエラーチェック
	if (dma->isr & (1UL << err_bit_pos)) {
		// エラーをクリア
		// (*)エラー発生時ENビットが自動的に0になる
		dma->ifcr |= (1UL << err_bit_pos);
		// 停止
		dma_stop(ch);
		// エラーコールバックを返して終了
		this->callback(ch, -1, this->callback_vp);
		return;
	}
	
	// 割込み処理
	if (dma->isr & (1UL << comp_bit_pos)) {
		// 割込みステータスをクリア
		dma->ifcr |= (1UL << half_bit_pos) | (1UL << comp_bit_pos) | (1UL << global_bit_pos);
		// 停止
		dma_stop(ch);
		// まだ残りがあるなら
		if (this->remain_transfer_count != 0) {
			// DMA送信
			memcpy(&send_info, &(this->send_info), sizeof(DMA_SEND));
			send_info.src_addr = this->send_info.src_addr + this->send_info.transfer_count;
			send_info.transfer_count = this->remain_transfer_count;
			dma_start_ex(ch, &send_info);
			
		// 残りがないなら
		} else {
			// コールバック
			this->callback(ch, 0, this->callback_vp);
		}
	}
	
	return;
}

/* 割込みハンドラ */
void dma1_handler(void){
	dma_common_handler(DMA_CH1);
}

void dma2_handler(void){
	dma_common_handler(DMA_CH2);
}

void dma3_handler(void){
	dma_common_handler(DMA_CH3);
}

void dma4_handler(void){
	dma_common_handler(DMA_CH4);
}

void dma5_handler(void){
	dma_common_handler(DMA_CH5);
}

void dma6_handler(void){
	dma_common_handler(DMA_CH6);
}

void dma7_handler(void){
	dma_common_handler(DMA_CH7);
}

// 外部公開関数
// DMA初期化関数
void dma_init(void)
{
	uint32_t ch;
	DMA_CTL *this;
	
	for (ch = 0; ch < DMA_CH_MAX; ch++) {
		// 制御ブロック取得
		this = get_myself(ch);
		// 制御ブロック初期化
		memset(this, 0, sizeof(DMA_CTL));
		// 割込みハンドラ登録
		kz_setintr(get_vec_no(ch), get_handler(ch));
		// 状態を更新
		this->status = ST_INTIALIZED;
	}
	
	return;
}

// DMAオープン関数
int32_t dma_open(DMA_CH ch, uint32_t resource, DMA_CALLBACK callback, void * callback_vp)
{
	DMA_CTL *this;
	volatile struct stm32l4_dmamux *dmamux = (struct stm32l4_dmamux*)DMAMUX1_BASE_ADDR;
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= DMA_CH_MAX) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// 状態が初期化済みでない場合エラーを返して終了
	if (this->status != ST_INTIALIZED) {
		return -1;
	}
	
	// リソース番号が範囲外の場合エラーを返して終了
	if (resource >= DMA_RESOURCE_MAX) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// コールバックの設定
	this->callback = callback;
	this->callback_vp = callback_vp;
	
	// リソースの設定
	dmamux->ccr[ch] = dma_resource_cnv_tbl[resource];
	
	// 割り込み有効
    HAL_NVIC_SetPriority(get_ire_type(ch), INTERRPUT_PRIORITY_5, 0);
    HAL_NVIC_EnableIRQ(get_ire_type(ch));
	
	// 状態を更新
	this->status = ST_OPENED;
	
	return 0;
}

// DMA転送関数
int32_t dma_start(DMA_CH ch, DMA_SEND *send_info)
{
	volatile struct stm32l4_dma *dma = (struct stm32l4_dma*)DMA1_BASE_ADDR;
	DMA_CTL *this;
	DMA_TRANSFER_PTN trans_ptn;
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= DMA_CH_MAX) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// 状態がオープン済みでない場合、エラーを返して終了
	if (this->status != ST_OPENED) {
		return -1;
	}
	
	// 転送パターンの設定
	// 転送元のアドレスがRAM？
	if (is_ram_addr(send_info->src_addr)) {
		// 転送先のアドレスがRAM？
		if (is_ram_addr(send_info->dst_addr)) {
			trans_ptn = DMA_TRANSFER_PTN_M2M;
		// 転送先のアドレスがペリフェラル？
		} else if (is_peri_addr(send_info->dst_addr)) {
			trans_ptn = DMA_TRANSFER_PTN_M2P;
		// 転送先のアドレスがRAMでもペリフェラルでもない場合はエラーを返して終了
		} else {
			return -1;
		}
	// 転送元のアドレスがペリフェラル？
	} else if (is_peri_addr(send_info->src_addr)) {
		// 転送先のアドレスがRAM？
		if (is_ram_addr(send_info->dst_addr)) {
			trans_ptn = DMA_TRANSFER_PTN_P2M;
		// 転送先のアドレスがペリフェラル？
		} else if (is_peri_addr(send_info->dst_addr)) {
			trans_ptn = DMA_TRANSFER_PTN_P2P;
		// 転送先のアドレスがRAMでもペリフェラルでもない場合はエラーを返して終了
		} else {
			return -1;
		}
	// 転送元のアドレスがRAMでもペリフェラルでもない場合はエラーを返して終了
	} else {
		return -1;
	}
	
	// DMAレジスタの設定
	// 下記はマニュアルに記載されているDMA設定手順
	/*
		The following sequence is needed to configure a DMA channel x:
		1. Set the peripheral register address in the DMA_CPARx register. 
			The data is moved from/to this address to/from the memory after the peripheral event, 
			or after the channel is enabled in memory-to-memory mode.
		2. Set the memory address in the DMA_CMARx register. 
			The data is written to/read from the memory after the peripheral event or after the 
			channel is enabled in memory-to-memory mode.
		3. Configure the total number of data to transfer in the DMA_CNDTRx register.
			After each data transfer, this value is decremented.
		4. Configure the parameters listed below in the DMA_CCRx register:
			– the channel priority
			– the data transfer direction
			– the circular mode
			– the peripheral and memory incremented mode
			– the peripheral and memory data size
			– the interrupt enable at half and/or full transfer and/or transfer error
		5. Activate the channel by setting the EN bit in the DMA_CCRx register
	*/
	
	// 転送元/転送先のアドレス設定
	// 転送元がメモリの場合
	if ((trans_ptn == DMA_TRANSFER_PTN_M2M) || (trans_ptn == DMA_TRANSFER_PTN_M2P)) {
		// 転送元のアドレス設定
		dma->commonn_reg[ch].cmar = send_info->src_addr;
		// 転送先のアドレス設定
		dma->commonn_reg[ch].cpar = send_info->dst_addr;
	// 転送元がperiferalの場合
	} else {
		// 転送元のアドレス設定
		dma->commonn_reg[ch].cpar = send_info->src_addr;
		// 転送先のアドレス設定
		dma->commonn_reg[ch].cmar = send_info->dst_addr;
	}
	
	// 転送サイズの設定
	dma->commonn_reg[ch].cndtr = send_info->transfer_count;
	// コンフィグ
	// MSIZEとPSIZEの関係
	// DIR:0 memory to periferal
	//     1 periferal to memory
	// DIR = 1
	//  転送元は、CCRのMSIZEとMINCで定義
	//  転送先は、CCRのPSIZEとPINCで定義
	// DIR = 0
	//  転送元は、CCRのPSIZEとPINCで定義
	//  転送先は、CCRのMSIZEとMINCで定義
	// (例) 転送元のサイズが8bit、転送先のサイズが16bitの場合
	//   転送元        転送先
	//     0x00 0xAA --> 0x00 0x00
	//                   0x01 0xAA
	//     0x01 0xBB --> 0x02 0x00
	//                   0x03 0xBB
	//     0x02 0xCC --> 0x04 0x00
	//                   0x05 0xCC
	//     0x03 0xDD --> 0x06 0x00
	//                   0x07 0xDD
	// 転送パターンの設定
	// Memory to Memoryの場合
	if (trans_ptn == DMA_TRANSFER_PTN_M2M) {
		dma->commonn_reg[ch].ccr = _CCR_MEM2MEM | _CCR_DIR;
	// Memory to Periferal 
	} else if (trans_ptn == DMA_TRANSFER_PTN_M2P) {
		dma->commonn_reg[ch].ccr |= _CCR_DIR;
	} else {
		;
	}
	// アドレスとインクリメントの設定
	// 転送元がMemoryの場合
	if ((trans_ptn == DMA_TRANSFER_PTN_M2M)||(trans_ptn == DMA_TRANSFER_PTN_M2P)) {
		// アドレスの設定
		
		if (send_info->src_addr_inc) {
			dma->commonn_reg[ch].ccr |= _CCR_MINC;
		}
		if (send_info->dst_addr_inc) {
			dma->commonn_reg[ch].ccr |= _CCR_PINC;
		}
	// 転送元がペリフェラルの場合
	} else {
		if (send_info->src_addr_inc) {
			dma->commonn_reg[ch].ccr |= _CCR_PINC;
		}
		if (send_info->dst_addr_inc) {
			dma->commonn_reg[ch].ccr |= _CCR_MINC;
		}
	}
	// 転送単位の設定
	dma->commonn_reg[ch].ccr |= (_CCR_MSIZE(send_info->transfer_unit) | _CCR_PSIZE(send_info->transfer_unit));
	// 割込みの設定(エラー割込みと転送完了割り込み)
	dma->commonn_reg[ch].ccr |= (_CCR_TEIE | _CCR_TCIE);
	// 有効化
	dma->commonn_reg[ch].ccr |= _CCR_EN;
	
	// 状態の更新
	this->status = ST_RUN;
	
	return 0;
}

// DMA転送関数
// (*) 転送回数（transfer_count）が17bitしかないため、それ以上のサイズが指定されたとしても大丈夫転送関数
int32_t dma_start_ex(DMA_CH ch, DMA_SEND *send_info)
{
	volatile struct stm32l4_dma *dma = (struct stm32l4_dma*)DMA1_BASE_ADDR;
	DMA_CTL *this;
	DMA_TRANSFER_PTN trans_ptn;
	uint32_t transfer_count;
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= DMA_CH_MAX) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// 状態がオープン済みでない場合、エラーを返して終了
	if (this->status != ST_OPENED) {
		return -1;
	}
	
	// 転送パターンの設定
	// 転送元のアドレスがRAM？
	if (is_ram_addr(send_info->src_addr)) {
		// 転送先のアドレスがRAM？
		if (is_ram_addr(send_info->dst_addr)) {
			trans_ptn = DMA_TRANSFER_PTN_M2M;
		// 転送先のアドレスがペリフェラル？
		} else if (is_peri_addr(send_info->dst_addr)) {
			trans_ptn = DMA_TRANSFER_PTN_M2P;
		// 転送先のアドレスがRAMでもペリフェラルでもない場合はエラーを返して終了
		} else {
			return -1;
		}
	// 転送元のアドレスがペリフェラル？
	} else if (is_peri_addr(send_info->src_addr)) {
		// 転送先のアドレスがRAM？
		if (is_ram_addr(send_info->dst_addr)) {
			trans_ptn = DMA_TRANSFER_PTN_P2M;
		// 転送先のアドレスがペリフェラル？
		} else if (is_peri_addr(send_info->dst_addr)) {
			trans_ptn = DMA_TRANSFER_PTN_P2P;
		// 転送先のアドレスがRAMでもペリフェラルでもない場合はエラーを返して終了
		} else {
			return -1;
		}
	// 転送元のアドレスがRAMでもペリフェラルでもない場合はエラーを返して終了
	} else {
		return -1;
	}
	
	// DMAレジスタの設定
	// 下記はマニュアルに記載されているDMA設定手順
	/*
		The following sequence is needed to configure a DMA channel x:
		1. Set the peripheral register address in the DMA_CPARx register. 
			The data is moved from/to this address to/from the memory after the peripheral event, 
			or after the channel is enabled in memory-to-memory mode.
		2. Set the memory address in the DMA_CMARx register. 
			The data is written to/read from the memory after the peripheral event or after the 
			channel is enabled in memory-to-memory mode.
		3. Configure the total number of data to transfer in the DMA_CNDTRx register.
			After each data transfer, this value is decremented.
		4. Configure the parameters listed below in the DMA_CCRx register:
			– the channel priority
			– the data transfer direction
			– the circular mode
			– the peripheral and memory incremented mode
			– the peripheral and memory data size
			– the interrupt enable at half and/or full transfer and/or transfer error
		5. Activate the channel by setting the EN bit in the DMA_CCRx register
	*/
	
	// 転送元/転送先のアドレス設定
	// 転送元がメモリの場合
	if ((trans_ptn == DMA_TRANSFER_PTN_M2M) || (trans_ptn == DMA_TRANSFER_PTN_M2P)) {
		// 転送元のアドレス設定
		dma->commonn_reg[ch].cmar = send_info->src_addr;
		// 転送先のアドレス設定
		dma->commonn_reg[ch].cpar = send_info->dst_addr;
	// 転送元がperiferalの場合
	} else {
		// 転送元のアドレス設定
		dma->commonn_reg[ch].cpar = send_info->src_addr;
		// 転送先のアドレス設定
		dma->commonn_reg[ch].cmar = send_info->dst_addr;
	}
	
	// 転送サイズの設定
	// 一回の最大送信回数以上の場合
	if (send_info->transfer_count > MAX_TRANSFER_COUNT) {
		// 送信回数を最大に設定
		transfer_count = MAX_TRANSFER_COUNT;
		// 残送信回数を設定
		this->remain_transfer_count = send_info->transfer_count - transfer_count;
		// 送信情報を覚えておく
		memcpy(&(this->send_info), send_info, sizeof(DMA_SEND));
		this->send_info.transfer_count = transfer_count;
	// 一回で送信できる場合
	} else {
		this->remain_transfer_count = 0;
		transfer_count = send_info->transfer_count;
	}
	dma->commonn_reg[ch].cndtr = transfer_count;
	// コンフィグ
	// MSIZEとPSIZEの関係
	// DIR:0 memory to periferal
	//     1 periferal to memory
	// DIR = 1
	//  転送元は、CCRのMSIZEとMINCで定義
	//  転送先は、CCRのPSIZEとPINCで定義
	// DIR = 0
	//  転送元は、CCRのPSIZEとPINCで定義
	//  転送先は、CCRのMSIZEとMINCで定義
	// (例) 転送元のサイズが8bit、転送先のサイズが16bitの場合
	//   転送元        転送先
	//     0x00 0xAA --> 0x00 0x00
	//                   0x01 0xAA
	//     0x01 0xBB --> 0x02 0x00
	//                   0x03 0xBB
	//     0x02 0xCC --> 0x04 0x00
	//                   0x05 0xCC
	//     0x03 0xDD --> 0x06 0x00
	//                   0x07 0xDD
	// 転送パターンの設定
	// Memory to Memoryの場合
	if (trans_ptn == DMA_TRANSFER_PTN_M2M) {
		dma->commonn_reg[ch].ccr = _CCR_MEM2MEM | _CCR_DIR;
	// Memory to Periferal 
	} else if (trans_ptn == DMA_TRANSFER_PTN_M2P) {
		dma->commonn_reg[ch].ccr |= _CCR_DIR;
	} else {
		;
	}
	// アドレスとインクリメントの設定
	// 転送元がMemoryの場合
	if ((trans_ptn == DMA_TRANSFER_PTN_M2M)||(trans_ptn == DMA_TRANSFER_PTN_M2P)) {
		// アドレスの設定
		
		if (send_info->src_addr_inc) {
			dma->commonn_reg[ch].ccr |= _CCR_MINC;
		}
		if (send_info->dst_addr_inc) {
			dma->commonn_reg[ch].ccr |= _CCR_PINC;
		}
	// 転送元がペリフェラルの場合
	} else {
		if (send_info->src_addr_inc) {
			dma->commonn_reg[ch].ccr |= _CCR_PINC;
		}
		if (send_info->dst_addr_inc) {
			dma->commonn_reg[ch].ccr |= _CCR_MINC;
		}
	}
	// 転送単位の設定
	dma->commonn_reg[ch].ccr |= (_CCR_MSIZE(send_info->transfer_unit) | _CCR_PSIZE(send_info->transfer_unit));
	// 割込みの設定(エラー割込みと転送完了割り込み)
	dma->commonn_reg[ch].ccr |= (_CCR_TEIE | _CCR_TCIE);
	// 有効化
	dma->commonn_reg[ch].ccr |= _CCR_EN;
	
	// 状態の更新
	this->status = ST_RUN;
	
	return 0;
}

// DMA停止関数
int32_t dma_stop(DMA_CH ch) 
{
	volatile struct stm32l4_dma *dma = (struct stm32l4_dma*)DMA1_BASE_ADDR;
	DMA_CTL *this;
	uint32_t err_bit_pos = ((ch * 4) + 3);
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= DMA_CH_MAX) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// 状態が転送中でない場合、エラーを返して終了
	if (this->status != ST_RUN) {
		return -1;
	}
	// 無効化
	// (*) エラーが起きてたらエラーフラグをクリアしない限りDMA無効化にできない
	if (dma->isr & (1UL << err_bit_pos)) {
		// エラーをクリア
		dma->ifcr |= (1UL << err_bit_pos);
	}
	dma->commonn_reg[ch].ccr &= ~_CCR_EN;
	
	// 状態を更新
	this->status = ST_OPENED;
	
	return 0;
}

