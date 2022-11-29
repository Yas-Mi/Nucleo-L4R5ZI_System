/* Includes ------------------------------------------------------------------*/
#include "defines.h"
#include "kozos.h"
#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_pwr_ex.h"
#include "stm32l4xx_hal_cortex.h"
#include "stm32l4xx.h"
#include "usart.h"

/* マクロ */
#define USART_CH_NUM (2U)
#define BUF_SIZE (32U)
#define USE_FIFO (1U)

/* USART ベースレジスタ */
#define USART1_BASE_ADDR (0x40013800)
#define USART2_BASE_ADDR (0x40004400)
#define USART3_BASE_ADDR (0x40004800)

/* レジスタ設定値 */
#define CR1_FIFOEN_MASK    (0x20000000)
#define CR1_FIFOEN_ENABLE  (0x20000000)
#define CR1_FIFOEN_DISABLE (0x20000000)
#define CR1_M1_MASK        (0x10000000)
#define CR1_M0_MASK        (0x00001000)
#define CR1_M_8DATA        (0x00000000)
#define CR1_TXEIE_MASK     (0x00000080)
#define CR1_TXEIE_ENABLE   (0x00000080)
#define CR1_TCIE_MASK      (0x00000040)
#define CR1_TCIE_ENABLE    (0x00000040)
#define CR1_RXNEIE_MASK    (0x00000020)
#define CR1_RXNEIE_ENABLE  (0x00000020)
#define CR1_TE_MASK        (0x00000008)
#define CR1_TE_ENABLE      (0x00000004)
#define CR1_RE_MASK        (0x00000004)
#define CR1_RE_ENABLE      (0x00000004)
#define CR1_UE_MASK        (0x00000001)
#define CR1_UE_ENABLE      (0x00000001)
// FIFO mode enable
#define CR1_RXFFIE_ENABLE  (0x80000000)
#define CR1_TXFEIE_ENABLE  (0x40000000)
#define CR1_RXFNEIE_ENABLE (0x00000020)
#define CR3_OVRDIS_MASK    (0x00001000)
#define ISR_BUSY_MASK      (0x00010000)
#define ISR_TXE_MASK       (0x00000080)
#define ISR_TC_MASK        (0x00000040)
#define ISR_RXNE_MASK      (0x00000020)

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

/* USART制御ブロック */
typedef struct {
	uint8_t ch;
	USART_CALLBACK callback;
	uint8_t     *send_buf;
	uint8_t     *recv_buf;
	uint8_t		*p_send_data;
	uint32_t	send_num;
} USART_CTL;
static USART_CTL usart_ctl[USART_CH_NUM];
#define get_myself(n) (&usart_ctl[(n)])

/* USARTレジスタ情報 */
typedef struct {
	/* USART */
	volatile struct stm32l4_usart *usart_base_addr;
	uint32_t baudrate;
	/* GPIO */
	uint32_t rx_Pin;
	uint32_t tx_Pin;
	uint32_t Mode;
	uint32_t Pull;
	uint32_t Speed;
	uint32_t Alternate;
	/* NVIC */
	IRQn_Type irq_type;
} USART_REG;

static const USART_REG usart_reg_table[USART_CH_NUM]=
{
	/* USART1 */
	/* Console */
	/**USART1 GPIO Configuration
    PA9     ------> USART1_TX
	PA10    ------> USART1_RX
	*/
	{
		USART1_BASE_ADDR,
		9600,
		GPIO_PIN_9,
		GPIO_PIN_10,
		GPIO_MODE_AF_PP,
		GPIO_NOPULL,
		GPIO_SPEED_FREQ_VERY_HIGH,
		GPIO_AF7_USART1,
		USART1_IRQn,
	},
	/* USART2 */
	/* BlueTooth */
	/**USART2 GPIO Configuration
    PA2     ------> USART2_TX
	PA3     ------> USART2_RX
	*/
	{
		USART2_BASE_ADDR,
		115200,
		GPIO_PIN_2,
		GPIO_PIN_3,
		GPIO_MODE_AF_PP,
		GPIO_NOPULL,
		GPIO_SPEED_FREQ_VERY_HIGH,
		GPIO_AF7_USART2,
		USART2_IRQn,
	},
};

/* 割込み共通ハンドラ */
void usart_common_handler(uint32_t ch){
	USART_CTL *this;

	this = get_myself(ch);
	this->callback(ch);
}

/* 割込みハンドラ */
void usart1_handler(void){
	usart_common_handler(USART1);
}

void usart2_handler(void){
	usart_common_handler(USART2);
}

void usart3_handler(void){
	usart_common_handler(USART3);
}

static uint32_t calc_brr(uint32_t baudrate)
{
	return (4000000/baudrate);
}

/* 現状、USART1,2の初期化のみ */
uint32_t usart_open(uint32_t ch)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	volatile struct stm32l4_usart *usart_base_addr;
	
	/* pin設定 */
	/*   クロックをイネーブル */
	__HAL_RCC_USART1_CLK_ENABLE();
	__HAL_RCC_USART2_CLK_ENABLE();
	__HAL_RCC_USART3_CLK_ENABLE();
	
	/*   GPIOのクロックをイネーブル */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	
	/*   電源供給 */
	HAL_PWREx_EnableVddIO2();

	/* 送信設定 */
    GPIO_InitStruct.Pin = usart_reg_table[ch].tx_Pin;
    GPIO_InitStruct.Mode = usart_reg_table[ch].Mode;
    GPIO_InitStruct.Pull = usart_reg_table[ch].Pull;
    GPIO_InitStruct.Speed = usart_reg_table[ch].Speed;
    GPIO_InitStruct.Alternate = usart_reg_table[ch].Alternate;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* 受信設定 */
    GPIO_InitStruct.Pin = usart_reg_table[ch].rx_Pin;
    GPIO_InitStruct.Mode = usart_reg_table[ch].Mode;
    GPIO_InitStruct.Pull = usart_reg_table[ch].Pull;
    GPIO_InitStruct.Speed = usart_reg_table[ch].Speed;
    GPIO_InitStruct.Alternate = usart_reg_table[ch].Alternate;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* USART1レジスタ設定 */
	/*
	     usart_ker_ck_pres = 4MHz
	     9600baud
	     USARTDIV = 4000000/9600 = 416.666...(0x1A0)
	     BRR = 0x1A0
	*/
	usart_base_addr = (volatile struct stm32l4_usart *)usart_reg_table[ch].usart_base_addr;
	usart_base_addr->brr = calc_brr(usart_reg_table[ch].baudrate);
    /*   騾∝女菫｡譛牙柑 */
#if USE_FIFO
	// FIFO有効
	usart_base_addr->cr1 |= CR1_FIFOEN_ENABLE | CR1_TE_MASK | CR1_RE_ENABLE ;
	// USART1の有効
	usart_base_addr->cr1 |= CR1_UE_ENABLE ;
#else
	usart_base_addr->cr1 |= CR1_UE_ENABLE | CR1_TE_MASK | CR1_RE_ENABLE ;
#endif

	// オーバーランは見ない
	usart_base_addr->cr3 |= CR3_OVRDIS_MASK;

	/* 割り込み有効 */
    HAL_NVIC_SetPriority(usart_reg_table[ch].irq_type, 0, 0);
    HAL_NVIC_EnableIRQ(usart_reg_table[ch].irq_type);

	return 0;
}

uint32_t usart_init(uint32_t ch, USART_CALLBACK callback)
{
	int i = 0;
	USART_CTL *this;
	
	/* 制御ブロックの初期化 */
	memset(&usart_ctl[ch], 0, sizeof(usart_ctl[0]));

	this = get_myself(ch);
	this->send_buf = kz_kmalloc(BUF_SIZE);
	this->recv_buf = kz_kmalloc(BUF_SIZE);
	this->callback = callback;
	
	return 0;
}

#if 0
uint32_t usart_send(uint32_t ch, uint8_t *data, uint32_t send_num)
{
	uint32_t ret;
	uint32_t i;
	uint32_t is_send_enable;
	USART_CTL *this;
	volatile struct stm32l4_usart *usart_base_addr;
	
	/* パラメータチェック */
	if(ch >= USART_CH_NUM){
		return -1;
	}
	
	if(data == NULL){
		return -1;
	}
	
	if(send_num > BUF_SIZE){
		return -1;
	}
	
	/* 制御ブロック取得 */
	this = get_myself(ch);
	
	/* ベースレジスタ取得 */
	usart_base_addr = (volatile struct stm32l4_usart *)&usart_reg_table[ch];
	
	/* 送信可能かチェック */
	is_send_enable = usart_is_send_enable(ch);
	
	/* 送信 */
	if(is_send_enable){
		/* 送信データをバッファに設定 */
		for(i=0;i<send_num;i++){
			this->send_buf[i] = data[i];
		}
		
		/* 送信レジスタから割り込み有効 */
		usart_base_addr->cr1 |= CR1_TXEIE_MASK;
		
		ret = 0;
	}else{
		ret = -1;
	}
	
	return ret;
}
#endif

uint32_t usart_is_send_enable(uint32_t ch)
{
	int ret;
	volatile struct stm32l4_usart *usart_base_addr;
	usart_base_addr = (volatile struct stm32l4_usart *)usart_reg_table[ch].usart_base_addr;
	int tmp;

	/* TXE : Transmit data register empty */
	/*   TDRにデータを設定すると、自動的に0クリアされる */
	/*   TXEIE=1の場合、割り込みを発生させる */
	/*   0 : Data register full */
	/*   1 : Data register not full */
	tmp = usart_base_addr->isr;
	if(usart_base_addr->isr & ISR_TXE_MASK){
		ret = 1;
	}else{
		ret = 0;
	}

	return ret;
}

/* １文字送信 */
uint32_t usart_send_byte(uint32_t ch, uint8_t c)
{
	volatile struct stm32l4_usart *usart_base_addr;
	usart_base_addr = (volatile struct stm32l4_usart *)usart_reg_table[ch].usart_base_addr;

	/* 送信可能になるまで待つ */
	while (!usart_is_send_enable(ch))
    ;
	usart_base_addr->tdr = c;
	/* 自動的にTXEはclearされる */

  return 0;
}

/* 受信可能か？ */
uint32_t usart_is_rcv_enable(uint32_t ch)
{
	int ret;
	volatile struct stm32l4_usart *usart_base_addr;
	usart_base_addr = (volatile struct stm32l4_usart *)usart_reg_table[ch].usart_base_addr;
	/* RXNE : Read data register not empty */
	/*   RDRにデータが格納されると、自動的に1に設定される */
	/*   RDRを読むと、RXNEは0クリアされる */
	/*   RXNEIE=1の場合、割り込みが発生する */
	/*   0 : Data is not received */
	/*   1 : DReceived data is ready to be read */
	if(usart_base_addr->isr & ISR_RXNE_MASK){
		ret = 1;
	}else{
		ret = 0;
	}

	return ret;
}

/* １文字受信 */
uint8_t usart_recv_byte(uint32_t ch)
{
	volatile struct stm32l4_usart *usart_base_addr;
	uint8_t c;

	/* ベースレジスタ取得 */
	usart_base_addr = (volatile struct stm32l4_usart *)usart_reg_table[ch].usart_base_addr;


  /* 受信文字が来るまで待つ */
  while (!usart_is_rcv_enable(ch))
    ;
  c = usart_base_addr->rdr;

  return c;
}

/* 送信割込み有効か？ */
uint32_t usart_intr_is_send_enable(uint32_t ch)
{
	volatile struct stm32l4_usart *usart_base_addr;
	uint32_t ret;

	/* ベースレジスタ取得 */
	usart_base_addr = (volatile struct stm32l4_usart *)usart_reg_table[ch].usart_base_addr;
#if USE_FIFO
	if(usart_base_addr->cr1 & CR1_TXFEIE_ENABLE){
#else
	if(usart_base_addr->cr1 & CR1_TXEIE_ENABLE){
#endif
		ret = 1;
	}else{
		ret = 0;
	}

	return ret;
}

/* 送信割込み有効化 */
void usart_intr_send_enable(int32_t ch)
{
	volatile struct stm32l4_usart *usart_base_addr;

	/* ベースレジスタ取得 */
	usart_base_addr = (volatile struct stm32l4_usart *)usart_reg_table[ch].usart_base_addr;
#if USE_FIFO
	usart_base_addr->cr1 |= CR1_TXFEIE_ENABLE;
#else
	usart_base_addr->cr1 |= CR1_TXEIE_ENABLE;
#endif
}

/* 送信割込み無効化 */
void usart_intr_send_disable(int32_t ch)
{
	volatile struct stm32l4_usart *usart_base_addr;

	/* ベースレジスタ取得 */
	usart_base_addr = (volatile struct stm32l4_usart *)usart_reg_table[ch].usart_base_addr;
#if USE_FIFO
	usart_base_addr->cr1 &= ~CR1_TXFEIE_ENABLE;
#else
	usart_base_addr->cr1 &= ~CR1_TXEIE_ENABLE;
#endif
}

/* 受信割込み有効か？ */
int usart_intr_is_recv_enable(int32_t ch)
{
	volatile struct stm32l4_usart *usart_base_addr;
	uint32_t ret;

	/* ベースレジスタ取得 */
	usart_base_addr = (volatile struct stm32l4_usart *)usart_reg_table[ch].usart_base_addr;
#if USE_FIFO
	if(usart_base_addr->cr1 & CR1_RXNEIE_ENABLE){
#else
	if(usart_base_addr->cr1 & CR1_RXNEIE_ENABLE){
#endif
		ret = 1;
	}else{
		ret = 0;
	}

	return ret;
}

/* 受信割込み有効化 */
void usart_intr_recv_enable(int32_t ch)
{
	volatile struct stm32l4_usart *usart_base_addr;

	/* ベースレジスタ取得 */
	usart_base_addr = (volatile struct stm32l4_usart *)usart_reg_table[ch].usart_base_addr;
#if USE_FIFO
	//usart_base_addr->cr1 |= CR1_RXFFIE_ENABLE;
	usart_base_addr->cr1 |= CR1_RXNEIE_ENABLE;
#else
	usart_base_addr->cr1 |= CR1_RXNEIE_ENABLE;
#endif
}

/* 受信割込み無効化 */
void usart_intr_recv_disable(int32_t ch)
{
	volatile struct stm32l4_usart *usart_base_addr;

	/* ベースレジスタ取得 */
	usart_base_addr = (volatile struct stm32l4_usart *)usart_reg_table[ch].usart_base_addr;

#if USE_FIFO
	usart_base_addr->cr1 &= ~CR1_RXFFIE_ENABLE;
#else
	usart_base_addr->cr1 &= ~CR1_RXNEIE_ENABLE;
#endif
}
