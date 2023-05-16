/* Includes ------------------------------------------------------------------*/
#include "string.h"
#include "defines.h"
#include "kozos.h"
#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_pwr_ex.h"
#include "stm32l4xx_hal_cortex.h"
#include "stm32l4xx.h"
#include "tim.h"
#include "intr.h"

// 状態
#define ST_NOT_INTIALIZED	(0U)	// 未初期化
#define ST_INTIALIZED		(1U)	// 初期化済
#define ST_OPENED			(2U)	// オープン済
#define ST_RUN				(3U)	// 実行中

// TIM ベースレジスタ
#define TIM2_BASE_ADDR	(0x40000000)
#define TIM3_BASE_ADDR	(0x40000400)
#define TIM4_BASE_ADDR	(0x40000800)
#define TIM5_BASE_ADDR	(0x40000C00)
#define TIM15_BASE_ADDR	(0x40014000)
#define TIM16_BASE_ADDR	(0x40014400)
#define TIM17_BASE_ADDR	(0x40014800)

// レジスタ設定値
// CR1
#define CR1_CEN				(1UL << 0)

// CCMR1
#define CCMR1_CCIS(v)		(((v) & 0x3) << 0)
#define CCMR1_IC1PSC(v)		(((v) & 0x3) << 2)
#define CCMR1_IC1F(v)		(((v) & 0xF) << 4)

// CCER
#define CCER_CC1NP			(1UL << 3)
#define CCER_CC1NE			(1UL << 2)
#define CCER_CC1P			(1UL << 1)
#define CCER_CC1E			(1UL << 0)

// DIER
#define DIER_CCIE			(1UL << 1)

// TIMレジスタ定義
struct stm32l4_tim {
	volatile uint32_t cr1;
	volatile uint32_t cr2;
	volatile uint32_t smcr;
	volatile uint32_t dier;
	volatile uint32_t sr;
	volatile uint32_t egr;
	volatile uint32_t ccmr1;
	volatile uint32_t ccmr2;
	volatile uint32_t ccer;
	volatile uint32_t cnt;
	volatile uint32_t psc;
	volatile uint32_t arr;
	volatile uint32_t reserve1;
	volatile uint32_t ccr1;
	volatile uint32_t ccr2;
	volatile uint32_t ccr3;
	volatile uint32_t ccr4;
	volatile uint32_t reserve2;
	volatile uint32_t dcr;
	volatile uint32_t dmar;
	volatile uint32_t or1;	// TIM2、3にしかない
	volatile uint32_t or2;	// TIM2、3にしかない
};

// TIMインプットキャプチャ情報ブロック
typedef struct {
	uint32_t captured_cnt;						// カウンタ
	TIM_INPUT_CAPTURE_CALLBACK callback;		// コールバック
	void * callback_vp;							// コールバックパラメータ
} TIM_INPUT_CAPUTURE_CTL;

// TIM制御ブロック
typedef struct {
	uint32_t status;							// 状態
	TIM_MODE mode;								// 使用用途
	TIM_INPUT_CAPUTURE_CTL input_capture_ctl;	// インプットキャプチャの情報	
} TIM_CTL;
static TIM_CTL tim_ctl[TIM_CH_MAX];
#define get_myself(n) (&tim_ctl[(n)])

// 割込みハンドラのプロトタイプ
static void tim2_handler(void);
static void tim3_handler(void);
static void tim4_handler(void);
static void tim5_handler(void);
static void tim15_handler(void);
static void tim16_handler(void);
static void tim17_handler(void);

// ピン情報
typedef struct {
	GPIO_TypeDef	 	*pin_group;
	GPIO_InitTypeDef	pin_cfg;
} PIN_FUNC_INFO;

// TIMチャネル固有情報
typedef struct {
	volatile struct stm32l4_tim *tim_base_addr;			// ベースレジスタ
	IRQn_Type		irq_type;							// 割込みタイプ
	INT_HANDLER 	handler;							// 割込みハンドラ
	uint32_t		vec_no;								// 割込み番号
	PIN_FUNC_INFO	input_capture_pin;					// インプットキャプチャ時の対応する外部入力
} TIM_CFG;

// TIMチャネル固有情報テーブル
static const TIM_CFG tim_cfg[TIM_CH_MAX] = {
	{(volatile struct stm32l4_tim *)TIM2_BASE_ADDR,		TIM2_IRQn,					tim2_handler,	TIM2_GLOBAL_INTERRUPT_NO,	{GPIOA, { GPIO_PIN_5,  GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_AF1_TIM2	 	}}},
	{(volatile struct stm32l4_tim *)TIM3_BASE_ADDR,		TIM3_IRQn,					tim3_handler,	TIM3_GLOBAL_INTERRUPT_NO,	{GPIOC, { GPIO_PIN_6,  GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_AF2_TIM3	 	}}},
	{(volatile struct stm32l4_tim *)TIM4_BASE_ADDR,		TIM4_IRQn,					tim4_handler,	TIM4_GLOBAL_INTERRUPT_NO,	{GPIOD, { GPIO_PIN_12, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_AF2_TIM4	 	}}},
	{(volatile struct stm32l4_tim *)TIM5_BASE_ADDR,		TIM5_IRQn,					tim5_handler,	TIM5_GLOBAL_INTERRUPT_NO,	{GPIOF, { GPIO_PIN_6,  GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_AF2_TIM5	 	}}},
	{(volatile struct stm32l4_tim *)TIM15_BASE_ADDR,	TIM1_BRK_TIM15_IRQn,		tim15_handler,	TIM15_GLOBAL_INTERRUPT_NO,	{GPIOB, { GPIO_PIN_14, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_AF14_TIM15	}}},
	{(volatile struct stm32l4_tim *)TIM16_BASE_ADDR,	TIM1_UP_TIM16_IRQn,			tim16_handler,	TIM16_GLOBAL_INTERRUPT_NO,	{GPIOA, { GPIO_PIN_6,  GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_AF14_TIM16	}}},
	{(volatile struct stm32l4_tim *)TIM17_BASE_ADDR,	TIM1_TRG_COM_TIM17_IRQn,	tim17_handler,	TIM17_GLOBAL_INTERRUPT_NO,	{GPIOA, { GPIO_PIN_7,  GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_AF14_TIM17	}}},
};
#define get_reg(ch)			(tim_cfg[ch].tim_base_addr)			// レジスタ取得マクロ
#define get_ire_type(ch)	(tim_cfg[ch].irq_type)				// 割込みタイプ取得マクロ
#define get_handler(ch)		(tim_cfg[ch].handler)				// 割り込みハンドラ取得マクロ
#define get_vec_no(ch)		(tim_cfg[ch].vec_no)				// 割り込み番号取得マクロ
#define get_pin_info(ch)	&(tim_cfg[ch].input_capture_pin)	// 割り込み番号取得マクロ

// 割込み共通ハンドラ
static void tim_common_handler(TIM_CH ch)
{
	volatile struct stm32l4_tim *tim_base_addr = get_reg(ch);
	TIM_CTL *this = get_myself(ch);
	
	// インプットキャプチャの場合
	if (this->mode == TIM_MODE_INPUT_CAPTURE) {
		// カウンタを取得 (*)カウンタを取得することで、割込みフラグが自動的にクリアされる
		this->input_capture_ctl.captured_cnt = tim_base_addr->ccr1;
		// コールバック
		if (this->input_capture_ctl.callback) {
			this->input_capture_ctl.callback(ch, this->input_capture_ctl.callback_vp, this->input_capture_ctl.captured_cnt);
		}
	} else if (this->mode == TIM_MODE_OUTPUT_COMPARE) {
		// 未サポート
	} else if (this->mode == TIM_MODE_PWM_GENERATE) {
		// 未サポート
	} else if (this->mode == TIM_MODE_ONE_PULSE_OUTPUT) {
		// 未サポート
	} else {
		;
	}
	
	return;
}

// 割込みハンドラ
static void tim2_handler(void){
	tim_common_handler(TIM_CH2);
}

static void tim3_handler(void){
	tim_common_handler(TIM_CH3);
}

static void tim4_handler(void){
	tim_common_handler(TIM_CH4);
}

static void tim5_handler(void){
	tim_common_handler(TIM_CH5);
}

static void tim15_handler(void){
	tim_common_handler(TIM_CH15);
}

static void tim16_handler(void){
	tim_common_handler(TIM_CH16);
}

static void tim17_handler(void){
	tim_common_handler(TIM_CH17);
}

// TIMのレジスタ設定
static int32_t set_config(TIM_CH ch, TIM_OPEN *open_par)
{
	volatile struct stm32l4_tim *tim_base_addr = get_reg(ch);
	TIM_CTL *this;
	PIN_FUNC_INFO *pin;
	
	// モードが対象外の場合、エラーを返して終了
	// (*) 現状、インプットキャプチャのみ対応
	if (open_par->mode >= TIM_MODE_OUTPUT_COMPARE) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// クロック設定
	// (*)クロックは内部クロックを使用する前提
	// 内部クロックの設定手順 (マニュアルから抜粋)
	/*
		If the slave mode controller is disabled (SMS=000 in the TIMx_SMCR register), then the 
		CEN, DIR (in the TIMx_CR1 register) and UG bits (in the TIMx_EGR register) are actual 
		control bits and can be changed only by software (except UG which remains cleared 
		automatically). As soon as the CEN bit is written to 1, the prescaler is clocked by the internal 
		clock CK_INT.
	*/
	tim_base_addr->cr1 = CR1_CEN;
	
	// インプットキャプチャ
	if (open_par->mode == TIM_MODE_INPUT_CAPTURE) {
		// 外部入力の設定
		pin = get_pin_info(ch);
		HAL_GPIO_Init(pin->pin_group, &(pin->pin_cfg));
		// モードの設定
		this->mode = TIM_MODE_INPUT_CAPTURE;
		// インプットキャプチャの設定手順 (マニュアルから抜粋)
		/*
			1.	Select the active input: TIMx_CCR1 must be linked to the TI1 input, so write the CC1S 
				bits to 01 in the TIMx_CCMR1 register. As soon as CC1S becomes different from 00, 
				the channel is configured in input and the TIMx_CCR1 register becomes read-only.
			
			2.	Program the appropriate input filter duration in relation with the signal connected to the 
				timer (when the input is one of the TIx (ICxF bits in the TIMx_CCMRx register). Let’s 
				imagine that, when toggling, the input signal is not stable during at must 5 internal clock 
				cycles. We must program a filter duration longer than these 5 clock cycles. We can 
				validate a transition on TI1 when 8 consecutive samples with the new level have been 
				detected (sampled at fDTS frequency). Then write IC1F bits to 0011 in the TIMx_CCMR1 register.
					
			3.	Select the edge of the active transition on the TI1 channel by writing the CC1P and 
				CC1NP and CC1NP bits to 000 in the TIMx_CCER register (rising edge in this case).
			
			4.	Program the input prescaler. In our example, we wish the capture to be performed at 
				each valid transition, so the prescaler is disabled (write IC1PS bits to 00 in the TIMx_CCMR1 register).
			
			5.	Enable capture from the counter into the capture register by setting the CC1E bit in the 
				TIMx_CCER register.
					
			6.	If needed, enable the related interrupt request by setting the CC1IE bit in the 
				TIMx_DIER register, and/or the DMA request by setting the CC1DE bit in the TIMx_DIER register.
		*/
		// レジスタの設定
		tim_base_addr->ccmr1 = CCMR1_CCIS(0x01);
		tim_base_addr->ccmr1 |= CCMR1_IC1F(0x03);
		tim_base_addr->ccer &= ~(CCER_CC1NP | CCER_CC1NE | CCER_CC1P);
		tim_base_addr->ccmr1 |= CCMR1_IC1PSC(0x00);
	} else if (open_par->mode == TIM_MODE_OUTPUT_COMPARE) {
		;	// 未サポート
	} else if (open_par->mode == TIM_MODE_PWM_GENERATE) {
		;	// 未サポート
	} else if (open_par->mode == TIM_MODE_ONE_PULSE_OUTPUT) {
		;	// 未サポート
	} else {
		;
	}
	
	return 0;
}

// 外部公開関数
// TIM初期化関数
void tim_init(void)
{
	uint32_t ch;
	TIM_CTL *this;
	
	for (ch = 0; ch < TIM_CH_MAX; ch++) {
		// 制御ブロック取得
		this = get_myself(ch);
		// 制御ブロック初期化
		memset(this, 0, sizeof(TIM_CTL));
		// 使用用途を初期化
		this->mode = TIM_MODE_MAX;
		// 割込みハンドラ登録
		kz_setintr(get_vec_no(ch), get_handler(ch));
		// 状態を更新
		this->status = ST_INTIALIZED;
	}
	
	return;
}

// TIMオープン関数
int32_t tim_open(TIM_CH ch, TIM_OPEN *open_par, TIM_INPUT_CAPTURE_CALLBACK callback, void * callback_vp)
{
	TIM_CTL *this;
	int32_t ret;
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= TIM_CH_MAX) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// 状態が初期化済みでない場合エラーを返して終了
	if (this->status != ST_INTIALIZED) {
		return -1;
	}
	
	// コールバックの設定
	if (open_par->mode == TIM_MODE_INPUT_CAPTURE) {
		this->input_capture_ctl.callback = callback;
		this->input_capture_ctl.callback_vp = callback_vp;
	} else if (open_par->mode == TIM_MODE_OUTPUT_COMPARE) {
		;	// 未サポート
	} else if (open_par->mode == TIM_MODE_PWM_GENERATE) {
		;	// 未サポート
	} else if (open_par->mode == TIM_MODE_ONE_PULSE_OUTPUT) {
		;	// 未サポート
	} else {
		;
	}
	
	// コンフィグ
	ret = set_config(ch, open_par);
	if (ret != 0) {
		return -1;
	}
	
	// 割り込み有効
	HAL_NVIC_SetPriority(get_ire_type(ch), INTERRPUT_PRIORITY_5, 0);
	HAL_NVIC_EnableIRQ(get_ire_type(ch));
	
	// 状態を更新
	this->status = ST_OPENED;
	
	return 0;
}

// インプットキャプチャ開始関数
int32_t tim_start_input_capture(TIM_CH ch)
{
	volatile struct stm32l4_tim *tim_base_addr;
	TIM_CTL *this;
	int32_t ret;
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= TIM_CH_MAX) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// 状態がオープン済みでない場合、エラーを返して終了
	if (this->status != ST_OPENED) {
		return -1;
	}
	
	// インプットキャプチャの設定がされている？
	if (this->mode != TIM_MODE_INPUT_CAPTURE) {
		return -1;
	}
	
	// ベースレジスタ取得
	tim_base_addr = get_reg(ch);
	
	// タイマ有効
	tim_base_addr->ccer |= CCER_CC1E;
	
	// 割込みの設定
	tim_base_addr->dier |= DIER_CCIE;
	
	// 状態の更新
	this->status = ST_RUN;
	
	return 0;
}
