/* Includes ------------------------------------------------------------------*/
#include "string.h"
#include "defines.h"
#include "kozos.h"
#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_pwr_ex.h"
#include "stm32l4xx_hal_cortex.h"
#include "stm32l4xx.h"
#include "usart.h"
#include "intr.h"

// マクロ
#define USART_BUF_SIZE		(1024U)		// 1024byte
#define USART_CLOCK			(4000000)	// 4MHz
#define USART_FIFO_SIZE		(8U)		// 8段

//USART ベースレジスタ
#define USART1_BASE_ADDR	(0x40013800)
#define USART2_BASE_ADDR	(0x40004400)
#define USART3_BASE_ADDR	(0x40004800)

// レジスタ設定値
// USART_CR1 (FIFO有効時)
#define CR1_RXFIE		(1UL << 31)		// RXFIFO FULL interrupt enable
#define CR1_TXFIE		(1UL << 30)		// RXFIFO FULL interrupt enable
#define CR1_FIFOEN		(1UL << 29)		// FIFO mode enable
#define CR1_M1			(1UL << 28)		//
#define CR1_M0			(1UL << 12)
#define CR1_TXFNFIE		(1UL << 7)
#define CR1_TCIE		(1UL << 6)
#define CR1_RXNEIE		(1UL << 5)
#define CR1_TE			(1UL << 3)
#define CR1_RE			(1UL << 2)
#define CR1_UE			(1UL << 0)
// USART_CR2
// USART_CR3
#define CR3_TXFTCFG		(7UL << 29)		// TXFIFO threshold configuration
#define CR3_RXFTIE		(1UL << 28)		// RXFIFO threshold interrupt enable
#define CR3_RXFTCFG		(7UL << 25)		// Receive FIFO threshold configuration
#define CR3_TXFTIE		(1UL << 23)		// TXFIFO threshold interrupt enable
// USART_BRR
// USART_ISR (FIFO有効時)
#define ISR_TXFT	(1UL << 27)		// TXFIFO threshold flag
#define ISR_RXFT	(1UL << 26)		// RXFIFO threshold flag
#define ISR_RXFF	(1UL << 24)		// RXFIFO ful
#define ISR_TXFE	(1UL << 23)		// TXFIFO empty
#define ISR_BUSY	(1UL << 16)		// Busy flag
#define ISR_TXFNF	(1UL << 7)		// TXFIFO not full
#define ISR_TC		(1UL << 6)		// Transmission complete
#define ISR_RXFNE	(1UL << 5)		// RXFIFO not empty
#define ISR_ORE		(1UL << 3)		// Overrun error
#define ISR_NE		(1UL << 2)		// Noise detection flag
#define ISR_FE		(1UL << 1)		// Framing error
#define ISR_PE		(1UL << 0)		// Parity error
// USART_ICR (FIFO有効時)

// レジスタ設定マクロ
#define SET_CR3_TXFTCFG(val)	((val & 0x7) << 29)
#define SET_CR3_RXFTCFG(val)	((val & 0x7) << 25)

// USARTレジスタ定義
struct stm32l4_usart {
  volatile uint32_t cr1;
  volatile uint32_t cr2;
  volatile uint32_t cr3;
  volatile uint32_t brr;
  volatile uint32_t gtpr;
  volatile uint32_t rtor;
  volatile uint32_t rqr;
  volatile uint32_t isr;
  volatile uint32_t icr;
  volatile uint32_t rdr;
  volatile uint32_t tdr;
  volatile uint32_t presc;
};

/* リングバッファ */
typedef struct {
	uint8_t  buf[USART_BUF_SIZE];	// バッファ
	uint16_t r_idx;					// リードインデックス
	uint16_t w_idx;					// ライトインデックス
} RING_BUF;

/* USART制御ブロック */
typedef struct {
	uint8_t				ch;					// 	チャネル
	RING_BUF			r_buf;				// 受信用リングバッファ
	RING_BUF 			s_buf;				// 送信用リングバッファ
	USART_CALLBACK		send_callback;		// 送信コールバック
	void 				*send_callback_vp;	// 送信コールバック引数
	USART_CALLBACK		recv_callback;		// 受信コールバック
	void 				*recv_callback_vp;	// 受信コールバック引数
} USART_CTL;
static USART_CTL usart_ctl[USART_CH_MAX];
#define get_myself(n) (&usart_ctl[(n)])

// 割込みハンドラのプロトタイプ
void usart1_handler(void);
void usart2_handler(void);

/* USARTチャネル固有情報 */
typedef struct {
	volatile struct stm32l4_usart *usart_base_addr;		// ベースレジスタ
	IRQn_Type irq_type;									// 割込みタイプ
	INT_HANDLER handler;								// 割込みハンドラ
	uint32_t	vec_no;									// 割込み番号
} USART_CFG;

/* USARTチャネル固有情報テーブル */
static const USART_CFG usart_cfg[USART_CH_MAX] =
{
	{(volatile struct stm32l4_usart*)USART1_BASE_ADDR, USART1_IRQn, usart1_handler, USART1_GLOBAL_INTERRUPT_NO},
	{(volatile struct stm32l4_usart*)USART2_BASE_ADDR, USART2_IRQn, usart2_handler, USART2_GLOBAL_INTERRUPT_NO},
};
#define get_reg(ch)			(usart_cfg[ch].usart_base_addr)		// レジスタ取得マクロ
#define get_ire_type(ch)	(usart_cfg[ch].irq_type)			// 割込みタイプ取得マクロ
#define get_handler(ch)		(usart_cfg[ch].handler)				// 割り込みハンドラ取得マクロ
#define get_vec_no(ch)		(usart_cfg[ch].vec_no)				// 割り込み番号取得マクロ

/* 割込み共通ハンドラ */
static void usart_common_handler(USART_CH ch){
	USART_CTL *this;
	RING_BUF *buf_info;
	volatile struct stm32l4_usart *usart_base_addr;
	
	// ベースレジスタ取得
	usart_base_addr = get_reg(ch);
	// 制御ブロック取得
	this = get_myself(ch);
	
	// エラーをチェック
	if (usart_base_addr->isr & (ISR_ORE | ISR_FE | ISR_PE)) {
		// エラーが発生したらとりあえず無限ループ
		while(1){};
	}
	
	// 受信データがある？
	if (usart_base_addr->isr & ISR_RXFNE) {
		buf_info = &(this->r_buf);
		// 受信用リングバッファに空きがある?
		if (((buf_info->w_idx + 1U) & (USART_BUF_SIZE - 1U)) != buf_info->r_idx) {
			// 受信データを受信用リングバッファに設定
			buf_info->buf[buf_info->w_idx] = usart_base_addr->rdr;
			// ライトインデックスを進める
			buf_info->w_idx = (buf_info->w_idx + 1U) & (USART_BUF_SIZE - 1U);
		}
		// コールバック実行
		if (this->recv_callback) {
			this->recv_callback(ch, this->recv_callback_vp);
		}
	}
	
	// 送信データがある？
	// 送信割り込みが有効? かつ 送信可能?
	if ((usart_base_addr->cr3 & CR3_TXFTIE) && (usart_base_addr->isr & ISR_TXFNF)) {
		// 送信用リングバッファにデータがある？
		buf_info = &(this->s_buf);
		if (buf_info->w_idx != buf_info->r_idx) {
			// 送信データを設定
			usart_base_addr->tdr = buf_info->buf[buf_info->r_idx];
			// リードインデックスを進める
			buf_info->r_idx = (buf_info->r_idx + 1U) & (USART_BUF_SIZE - 1U);
			// コールバック実行
			if (this->send_callback) {
				this->send_callback(ch, this->send_callback_vp);
			}
		// 送信リングバッファにデータがない場合
		} else {
			// 割込みを無効にする
			usart_base_addr->cr3 &= ~CR3_TXFTIE;
		}
	}
}

/* 割込みハンドラ */
void usart1_handler(void){
	usart_common_handler(USART_CH1);
}

void usart2_handler(void){
	usart_common_handler(USART_CH2);
}

// 指定したボーレートからレジスタ設定値を計算する関数
static uint32_t calc_brr(uint32_t baudrate)
{
	return (USART_CLOCK/baudrate);
}

// 外部公開関数
// USART初期化関数
void usart_init(void)
{
	uint32_t ch;
	USART_CTL *this;
	
	for (ch = 0; ch < USART_CH_MAX; ch++) {
		// 制御ブロック取得
		this = get_myself(ch);
		// 制御ブロック初期化
		memset(this, 0, sizeof(USART_CTL));
		// 割込みハンドラ登録
		kz_setintr(get_vec_no(ch), get_handler(ch));
	}
	
	return;
}

// USARTオープン関数
int32_t usart_open(USART_CH ch, uint32_t baudrate)
{
	volatile struct stm32l4_usart *usart_base_addr;
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= USART_CH_MAX) {
		return -1;
	}
	
	// ベースレジスタ取得
	usart_base_addr = get_reg(ch);
	// ボーレート設定
	usart_base_addr->brr = calc_brr(baudrate);
	// FIFO有効、送受信有効
	usart_base_addr->cr1 |= (CR1_FIFOEN | CR1_TE | CR1_RE);
	// USART1の有効
	usart_base_addr->cr1 |= CR1_UE ;
	
	// FIFOの設定
	// いったんクリア
	usart_base_addr->cr3 &= (~CR3_TXFTCFG | ~CR3_RXFTCFG);
	// 送信FIFOが空になった時に送信割り込みが入るように設定
	usart_base_addr->cr3 |= SET_CR3_TXFTCFG(5);
	// 受信FIFOに一つでもデータがあった時に受信割り込みが入るように設定
	usart_base_addr->cr3 |= SET_CR3_RXFTCFG(0);
	
	// 受信割り込みを有効
	usart_base_addr->cr3 |= CR3_RXFTIE;
	
	/* 割り込み有効 */
    HAL_NVIC_SetPriority(get_ire_type(ch), INTERRPUT_PRIORITY_5, 0);
    HAL_NVIC_EnableIRQ(get_ire_type(ch));

	return 0;
}

// USART送信関数
int32_t usart_send(USART_CH ch, uint8_t *data, uint32_t size) 
{
	USART_CTL *this;
	RING_BUF *buf_info;
	uint32_t tmp_w_idx;
	uint32_t i;
	volatile struct stm32l4_usart *usart_base_addr;
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= USART_CH_MAX) {
		return -1;
	}
	
	// 送信データがNULLの場合、エラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	// リングバッファ情報取得
	buf_info = &(this->s_buf);
	
	// 送信リングバッファに空きがない場合、エラーを返して終了
	tmp_w_idx = buf_info->w_idx;
	for (i = 0; i < size; i++) {
		tmp_w_idx++;
		if (tmp_w_idx == buf_info->r_idx) {
			return -1;
		}
	}
	
	// 割込み禁止
	INTR_DISABLE;
	
	for (i = 0; i < size; i++) {
		// 送信用リングバッファに送信データを設定
		buf_info->buf[buf_info->w_idx] = *(data++);
		// ライトインデックスを進める
		buf_info->w_idx = (buf_info->w_idx + 1U) & (USART_BUF_SIZE - 1U);
	}
	
	// 割込み禁止解除
	INTR_ENABLE;
	
	// USARTの割り込み有効
	usart_base_addr = get_reg(ch);
	usart_base_addr->cr3 |= CR3_TXFTIE;
	
	return 0;
}

// USART受信関数
int32_t usart_recv(USART_CH ch, uint8_t *data, uint32_t size) 
{
	USART_CTL *this;
	RING_BUF *buf_info;
	uint32_t i;
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= USART_CH_MAX) {
		return -1;
	}
	
	// 送信データがNULLの場合、エラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	// リングバッファ情報取得
	buf_info = &(this->r_buf);
	
	// 読みたいサイズ分データがない場合は、エラーを返して終了
	if ((buf_info->r_idx + size) > buf_info->w_idx) {
		return -1;
	}
	
	// 割込み禁止
	INTR_DISABLE;
	
	for (i = 0; i < size; i++) {
		// 受信用リングバッファからデータを取得
		*data = buf_info->buf[buf_info->r_idx];
		// リードインデックスを進める
		buf_info->r_idx = (buf_info->r_idx + 1U) & (USART_BUF_SIZE - 1U);
		// データのポインタを進める
		data++;
	}
	
	// 割込み禁止解除
	INTR_ENABLE;
	
	return 0;
}

// 受信コールバック設定関数
int32_t usart_reg_recv_callback(USART_CH ch, USART_CALLBACK cb, void *vp)
{
	USART_CTL *this;
	
	// コールバック関数がNULLの場合、エラーを返して終了
	if (cb == NULL) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// コールバック関数を登録
	this->recv_callback = cb;
	this->recv_callback_vp = vp;
	
	return 0;
}

// 送信コールバック設定関数
int32_t usart_reg_send_callback(USART_CH ch, USART_CALLBACK cb, void *vp)
{
	USART_CTL *this;
	
	// コールバック関数がNULLの場合、エラーを返して終了
	if (cb == NULL) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// コールバック関数を登録
	this->send_callback = cb;
	this->send_callback_vp = vp;
	
	return 0;
}
