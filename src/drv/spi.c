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

#include "spi.h"

// マクロ
#define BUFF_SIZE	(4*1024)		// バッファサイズ (4KB)

// 状態
#define ST_NOT_INTIALIZED	(0U)	// 未初期化
#define ST_INTIALIZED		(1U)	// 初期化済
#define ST_OPENED			(2U)	// オープン済
#define ST_RUN				(3U)	// 実行中

// SPI ベースレジスタ
#define SPI1_BASE_ADDR		(0x40013000)
#define SPI2_BASE_ADDR		(0x40003800)
#define SPI3_BASE_ADDR		(0x40003C00)

// SPIレジスタ定義
struct stm32l4_spi {
	volatile uint32_t cr1;		// Control Register 1		OFSET 0x00
	volatile uint32_t cr2;		// Control Register 2		OFSET 0x04
	volatile uint32_t sr;		// Status Register 2		OFSET 0x08
	volatile uint8_t dr[2];		// Data Register 2			OFSET 0x0C
	volatile uint16_t reserved;	// リザーブ
	volatile uint32_t crcpr;	// CRC polynomial Register	OFSET 0x00
	volatile uint32_t rxcrcr;	// Rx CRC Register			OFSET 0x00
	volatile uint32_t txcrcr;	// Tx CRC Register			OFSET 0x00
};

// レジスタビット定義
// CR1
#define CR1_BIDIMODE		(1 << 15)
#define CR1_BIDIOE			(1 << 14)
#define CR1_CRCEN			(1 << 13)
#define CR1_CRCNEXT			(1 << 12)
#define CR1_CRCL			(1 << 11)
#define CR1_RXONLY			(1 << 10)
#define CR1_SSM				(1 << 9)
#define CR1_SSI				(1 << 8)
#define CR1_LSBFIRST		(1 << 7)
#define CR1_SPE				(1 << 6)
#define CR1_BR(v)			(((v) & 0x7) << 3)
#define CR1_MSTR			(1 << 2)
#define CR1_CPOL			(1 << 1)
#define CR1_CPHA			(1 << 0)
// CR2
#define CR2_LDMA_TX			(1 << 14)
#define CR2_LDMA_RX			(1 << 13)
#define CR2_FRXTH			(1 << 12)
#define CR2_DS(v)			(((v) & 0xF) << 8)
#define CR2_TXEIE			(1 << 7)
#define CR2_RXNEIE			(1 << 6)
#define CR2_ERRIE			(1 << 5)
#define CR2_FRF				(1 << 4)
#define CR2_NSSP			(1 << 3)
#define CR2_SSOE			(1 << 2)
#define CR2_TXDMAEN			(1 << 1)
#define CR2_RXDMAEN			(1 << 0)
// SR
#define SR_FTLVL			(0x3 << 11)
#define SR_FRLVL			(0x3 << 9)
#define SR_FRE				(1 << 8)
#define SR_BUSY				(1 << 7)
#define SR_OVR				(1 << 6)
#define SR_MODF				(1 << 5)
#define SR_CRCERR			(1 << 4)
#define SR_TXE				(1 << 1)
#define SR_RXNE				(1 << 0)

// 制御ブロック
typedef struct {
	SPI_CH				ch;				// チャネル
	uint32_t			status;			// 状態
	uint8_t				*p_snd_data;	// 送信データポインタ
	uint32_t			snd_sz;			// 送信データサイズ
	uint8_t				*p_rcv_data;	// 受信データポインタ
	uint32_t			rcv_sz;			// 受信データサイズ
	kz_msgbox_id_t		msg_id;			// メールボックスID
} SPI_CTL;
static SPI_CTL spi_ctl[SPI_CH_MAX];
#define get_myself(n) (&spi_ctl[(n)])

// SPIチャネル固有情報
typedef struct {
	volatile struct stm32l4_spi *spi_base_addr;			// ベースレジスタ
	IRQn_Type 		irq_type;							// 割込みタイプ
	INT_HANDLER 	handler;							// 割込みハンドラ
	uint32_t		vec_no;								// 割込み番号
	uint32_t		int_priority;						// 割込み優先度
	uint32_t		clk;								// クロック定義
	kz_msgbox_id_t	msg_id;								// メールボックスID
} SPI_CFG;

// 割込みハンドラのプロトタイプ
void spi1_handler(void);
void spi2_handler(void);
void spi3_handler(void);

/* SPIチャネル固有情報テーブル */
static const SPI_CFG spi_cfg[USART_CH_MAX] =
{
	{(volatile struct stm32l4_spi*)SPI1_BASE_ADDR, SPI1_IRQn, spi1_handler, SPI1_GLOBAL_INTERRUPT_NO, INTERRPUT_PRIORITY_5, RCC_PERIPHCLK_USART1, MSGBOX_ID_SPI1},
	{(volatile struct stm32l4_spi*)SPI2_BASE_ADDR, SPI2_IRQn, spi2_handler, SPI2_GLOBAL_INTERRUPT_NO, INTERRPUT_PRIORITY_5, RCC_PERIPHCLK_USART2, MSGBOX_ID_SPI2},
	{(volatile struct stm32l4_spi*)SPI3_BASE_ADDR, SPI3_IRQn, spi3_handler, SPI3_GLOBAL_INTERRUPT_NO, INTERRPUT_PRIORITY_5, RCC_PERIPHCLK_USART3, MSGBOX_ID_SPI3},
};
#define get_reg(ch)			(spi_cfg[ch].spi_base_addr)		// レジスタ取得マクロ
#define get_ire_type(ch)	(spi_cfg[ch].irq_type)			// 割込みタイプ取得マクロ
#define get_handler(ch)		(spi_cfg[ch].handler)			// 割り込みハンドラ取得マクロ
#define get_vec_no(ch)		(spi_cfg[ch].vec_no)			// 割り込み番号取得マクロ
#define get_int_prio(ch)	(spi_cfg[ch].int_priority)		// 割り込み優先度取得マクロ
#define get_clk_no(ch)		(spi_cfg[ch].clk)				// クロック定義取得マクロ
#define get_mbx_id(ch)		(spi_cfg[ch].msg_id)			// メールボックスID取得マクロ

// DMA情報
typedef struct {
	DMA_CH tx_ch;			// SPI送信に使用するDMAのチャネル
	DMA_CH rx_ch;			// SPI送信に使用するDMAのチャネル
	uint32_t tx_resorce;	// 送信リソース
	uint32_t rx_resorce;	// 受信リソース
} SPI_DMA_INFO;

// DMA情報テーブル
static const SPI_DMA_INFO spi_dma_info[SPI_CH_MAX] =
{
	{DMA_CH2,	DMA_CH2,	DMA_RESOURCE_SPI1_TX,	DMA_RESOURCE_SPI1_RX},	// SPI1のDMA使用情報
	{DMA_CH3,	DMA_CH4,	DMA_RESOURCE_SPI2_TX,	DMA_RESOURCE_SPI2_RX},	// SPI2のDMA使用情報
	{DMA_CH5,	DMA_CH6,	DMA_RESOURCE_SPI3_TX,	DMA_RESOURCE_SPI3_RX},	// SPI3のDMA使用情報
};

// 割込み共通ハンドラ
static void spi_common_handler(DMA_CH ch)
{
	SPI_CTL *this = get_myself(ch);
	volatile struct stm32l4_spi *spi_base_addr = get_reg(ch);
	uint8_t dummy_data;
	int32_t *ret;
	
	// エラー
	if (spi_base_addr->sr & (SR_FRE | SR_OVR | SR_MODF)) {
		// 送受信割込み無効
		spi_base_addr->cr2 &= ~(CR2_RXNEIE | CR2_TXEIE);
		// メッセージ送信
		ret = kx_kmalloc(sizeof(int32_t));
		*ret = E_OBJ;
		kx_send(this->msg_id, sizeof(int32_t), ret);
		
	}
	
	// 送信バッファエンプティ割込み
	if (((spi_base_addr->cr2 & CR2_TXEIE) != 0) && (spi_base_addr->sr & SR_TXE) != 0) {
		// まだ送信データがある？
		if (this->snd_sz != 0) {
			// データ送信
			spi_base_addr->dr[0] = *(this->p_snd_data++);
			this->snd_sz--;
			
		// もう送信データはない？
		} else {
			// 送受信割込み禁止
			spi_base_addr->cr2 &= ~CR2_TXEIE;
		}
	}
	// 受信バッファ割込み
	if (((spi_base_addr->cr2 & CR2_RXNEIE) != 0) && (spi_base_addr->sr & SR_RXNE) != 0) {
		// まだ送信データがある？
		// データ送信時にダミーデータを受信してしまうため読み捨てる
		if (this->rcv_sz != 0) {
			// データ受信
			dummy_data = spi_base_addr->dr[0];
			if (this->p_rcv_data != NULL) {
				*(this->p_rcv_data++) = dummy_data;
			}
			this->rcv_sz--;
			
		// もう全部受信した
		} else {
			// 受信割り込み禁止
			spi_base_addr->cr2 &= ~(CR2_RXNEIE | CR2_TXEIE);
			
		}
	}
	// 送信/受信が完了した
	if ((this->snd_sz == 0) && (this->rcv_sz == 0)) {
		// 割込み無効
		spi_base_addr->cr2 &= ~(CR2_RXNEIE | CR2_TXEIE | CR2_ERRIE);
		// メッセージ送信
		ret = kx_kmalloc(sizeof(int32_t));
		*ret = E_OK;
		kx_send(this->msg_id, sizeof(int32_t), ret);
		
	}
}

/* 割込みハンドラ */
void spi1_handler(void){
	spi_common_handler(SPI_CH1);
}

void spi2_handler(void){
	spi_common_handler(SPI_CH2);
}

void spi3_handler(void){
	spi_common_handler(SPI_CH3);
}

// DMAのコールバック
static void dma_callback(DMA_CH ch, int32_t ret, void *vp)
{
	volatile struct stm32l4_spi *spi_base_addr;
	SPI_CTL *this = (SPI_CTL*)vp;
	int32_t *callback_ret;
	
	// メッセージ送信
	callback_ret = kx_kmalloc(sizeof(int32_t));
	*callback_ret = ret;
	kx_send(this->msg_id, sizeof(int32_t), callback_ret);
}

// 指定したボーレートからレジスタ設定値を計算する関数
static uint32_t calc_br(SPI_CH ch, uint32_t baudrate)
{
	uint32_t spi_clk;
	uint8_t power_of_2;
	uint8_t i;
	uint8_t br;
	
	// SPIクロック取得
	spi_clk = HAL_RCCEx_GetPeriphCLKFreq(get_clk_no(ch));
	
	// BR値を計算
	for(i = 0; i < 8; i++) {
		power_of_2 = (1 << i);
		if ((spi_clk/power_of_2) < baudrate) {
			break;
		}
	}
	
	// br値を計算
	if (i > 0) {
		br = i - 1;
	}
	
	return br;
}

// SPI無効処理
static int32_t spi_disable(SPI_CH ch)
{
	volatile struct stm32l4_spi *spi_base_addr;
	uint32_t timeout = 10;
	int32_t ret = E_OK;
	uint8_t dummy_data;
	
	// ベースレジスタ取得
	spi_base_addr = get_reg(ch);
	
	// 送信FIFOが0になるまで待つ
	while(((spi_base_addr->sr >> 11) & 0x3) != 0) {
		// タイムアウト発生
		if (timeout-- == 0) {
			ret = E_TMOUT;
			goto SPI_DISABLE_EXIT;
		}
		// 1msウェイト
		kz_tsleep(1);
	}
	
	// BUSYフラグがクリアされるまで待つ
	while((spi_base_addr->sr & SR_BUSY) != 0) {
		// タイムアウト発生
		if (timeout-- == 0) {
			ret = E_TMOUT;
			goto SPI_DISABLE_EXIT;
		}
		// 1msウェイト
		kz_tsleep(1);
	}
	
	// SPI無効
	spi_base_addr->cr1 &= ~CR1_SPE;
	
	// 受信FIFOが0になるまで待つ
	while(((spi_base_addr->sr >> 9) & 0x3) != 0) {
		// ダミーリード
		dummy_data = spi_base_addr->dr[0];
	}
	
SPI_DISABLE_EXIT:
	return ret;
}

// レジスタ設定
static void set_config(SPI_CH ch, SPI_OPEN *open_par)
{
	volatile struct stm32l4_spi *spi_base_addr;
	
	// ベースレジスタ取得
	spi_base_addr = get_reg(ch);
	
	// メモ
	// NSS input : Low → 誰かが下げているからスレーブなれる High → 誰も下げていないから、送信できる
	
	// レジスタ設定
	// CR1設定：tranmit only, crc caluclation disabled, Master
	// SSM=1:SSIでNSSを制御
	// SSM=0:ハードウェアがNSSを制御
	//        SSOE=1:マスターのみ使用。NSSは、SPE=1でlow、SPE=0でHigh。NSSP=1の場合は通信間でパルスを発生させる。
	//        SSOE=0:マルチマスター時に使用。
	//spi_base_addr->cr1 = CR1_BIDIOE | CR1_BR(calc_br(ch, open_par->baudrate)) | CR1_MSTR;
	spi_base_addr->cr1 = CR1_BR(calc_br(ch, open_par->baudrate)) | CR1_MSTR;
	if (open_par->fmt == SPI_FRAME_FMT_LSB_FIRST) {
		spi_base_addr->cr1 |= CR1_LSBFIRST;
	}
	if (open_par->cpol == SPI_CPOL_NEGATIVE) {
		spi_base_addr->cr1 |= CR1_CPOL;
	}
	if (open_par->cpha == SPI_CPHA_LAST_EDGE) {
		spi_base_addr->cr1 |= CR1_CPHA;
	}
	
	// CR2設定：
	//spi_base_addr->cr2 = CR2_DS(open_par->size) | CR2_NSSP | CR2_SSOE;
	spi_base_addr->cr2 = CR2_FRXTH | CR2_DS(open_par->size) | CR2_SSOE;
	
}

// 外部公開関数
// 初期化関数
void spi_init(void)
{
	SPI_CTL *this;
	uint32_t ch;
	
	for (ch = 0; ch < SPI_CH_MAX; ch++) {
		// 制御ブロック取得
		this = get_myself(ch);
		// 制御ブロック初期化
		memset(this, 0, sizeof(SPI_CTL));
		// チャネル設定
		this->ch = ch;
		// 割込みハンドラ登録
		kz_setintr(get_vec_no(ch), get_handler(ch));
		// メールボックスID取得
		this->msg_id = get_mbx_id(ch);
		// 状態を更新
		this->status = ST_INTIALIZED;
	}
	
	return;
}

// オープン関数
// (*) マスターのみ
int32_t spi_open(SPI_CH ch, SPI_OPEN *open_par)
{
	SPI_CTL *this;
	int32_t ret;
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= SPI_CH_MAX) {
		return E_PAR;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// 状態が初期化済みでない場合エラーを返して終了
	if (this->status != ST_INTIALIZED) {
		return -1;
	}
	
	// レジスタ設定
	set_config(ch, open_par);
	
	// DMAオープン (*) とりあえず送信だけ
	if (spi_dma_info[ch].tx_ch != DMA_CH_MAX) {
		ret = dma_open(spi_dma_info[ch].tx_ch, spi_dma_info[ch].tx_resorce, dma_callback, this);
		if (ret != 0) {
			return -1;
		}
	}
	
	// 割り込み有効
	HAL_NVIC_SetPriority(get_ire_type(ch), INTERRPUT_PRIORITY_5, 0);
	HAL_NVIC_EnableIRQ(get_ire_type(ch));
	
	// 状態を更新
	this->status = ST_OPENED;
	
	return 0;
}

// 送受信関数
// (*) マスターのみ
int32_t spi_send_recv(SPI_CH ch, uint8_t *snd_data, uint8_t *rcv_data, uint32_t snd_sz)
{
	SPI_CTL *this;
	volatile struct stm32l4_spi *spi_base_addr;
	int32_t size;
	int32_t *ercd;
	
	// NULLの場合エラーを返して終了
	if (snd_data == NULL) {
		return E_PAR;
	}
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= SPI_CH_MAX) {
		return E_PAR;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// 状態がオープン済みでない場合エラーを返して終了
	if (this->status != ST_OPENED) {
		return -1;
	}
	
	// 状態を更新
	this->status = ST_RUN;
	
	// データ情報設定
	this->p_snd_data = snd_data;
	this->snd_sz = snd_sz;
	this->p_rcv_data = rcv_data;
	this->rcv_sz = snd_sz;
	
	// ベースレジスタ取得
	spi_base_addr = get_reg(ch);
	
	// SPI有効
	spi_base_addr->cr1 |= CR1_SPE;
	
	// 送信バッファエンプティ割り込み、受信割り込み、エラー割込み有効
	spi_base_addr->cr2 |= (CR2_TXEIE | CR2_RXNEIE | CR2_ERRIE);
	
	// 送信待ち
	kz_recv(this->msg_id, &size, &ercd);
	// メッセージを解放
	kz_kmfree(ercd);
	
	// 初期化
	this->p_snd_data = NULL;
	this->snd_sz = 0;
	this->p_rcv_data = NULL;
	this->rcv_sz = 0;
	
	// SPI無効
	spi_disable(ch);
	
	// 1msウェイト
	kz_tsleep(1);
	
	// 状態を更新
	this->status = ST_OPENED;
	
	return *ercd;
}

// 送受信関数 DMA使用
// (*) マスターのみ
int32_t spi_send_dma(SPI_CH ch, uint8_t *data, uint32_t size)
{
	SPI_CTL *this;
	volatile struct stm32l4_spi *spi_base_addr;
	int32_t msg_size;
	int32_t *ercd;
	DMA_SEND send_info;
	int32_t ret;
	
	// NULLの場合エラーを返して終了
	if (data == NULL) {
		return E_PAR;
	}
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= SPI_CH_MAX) {
		return E_PAR;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// 状態がオープン済みでない場合エラーを返して終了
	if (this->status != ST_OPENED) {
		return -1;
	}
	
	// 状態を更新
	this->status = ST_RUN;
	
	// ベースレジスタ取得
	spi_base_addr = get_reg(ch);
	
	// 送信情報を設定
	send_info.src_addr       = (uint32_t)data;
	send_info.src_addr_inc   = TRUE;
	send_info.dst_addr       = (uint32_t)(&(spi_base_addr->dr[0]));
	send_info.dst_addr_inc   = FALSE;
	send_info.transfer_unit  = DMA_TRANSFER_UNIT_8BIT;		// (*) とりあえず8bit固定
	send_info.transfer_count = size;
	
	// DMAを使用して送信の設定
	if (spi_dma_info[ch].tx_ch != DMA_CH_MAX) {
		ret = dma_start_ex(spi_dma_info[ch].tx_ch, &send_info);
		if (ret != 0) {
			this->status = ST_OPENED;
			return -1;
		}
	// DMAを使用する設定になっていなければエラーを返して終了
	} else {
		this->status = ST_OPENED;
		return -1;
	}
	
	// 送信だけdma有効
	// TXEビットがセットされたときにDMAへ要求を送信
	// TXEはFIFO(4byte)に半分以下のデータがあるときにセットされる
	spi_base_addr->cr2 |= CR2_TXDMAEN;
	
	// SPI有効
	spi_base_addr->cr1 |= CR1_SPE;
	
	// 送信待ち
	kz_recv(this->msg_id, &msg_size, &ercd);
	// メッセージを解放
	kz_kmfree(ercd);
	
	// 初期化
	this->p_snd_data = NULL;
	this->snd_sz = 0;
	this->p_rcv_data = NULL;
	this->rcv_sz = 0;
	
	// SPI無効
	//(*) DMA時は、受信FIFOのエラーが必ず起きるが、SPI無効時にクリアされる
	spi_disable(ch);
	
	// dma無効
	spi_base_addr->cr2 &= ~CR2_TXDMAEN;
	
	// 1msウェイト
	kz_tsleep(1);
	
	// 状態を更新
	this->status = ST_OPENED;
	
	return *ercd;
}
