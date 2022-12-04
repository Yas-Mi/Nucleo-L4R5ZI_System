/* Includes ------------------------------------------------------------------*/
#include "defines.h"
#include "kozos.h"
#include "stm32l4xx_hal_i2c.h"
#include "i2c_wrapper.h"
#include "intr.h"

// 制御用ブロック
typedef struct {
	I2C_HandleTypeDef hi2c1;
} I2C_CTL;
static I2C_CTL i2c_ctl[I2C_CH_MAX];
#define get_myself(ch) &(i2c_ctl[(ch)])

// USARTチャネル固有情報
typedef struct {
	IRQn_Type irq_type;									// 割込みタイプ
	INT_HANDLER handler;								// 割込みハンドラ
	uint32_t	vec_no;									// 割込み番号
} I2C_CFG;
typedef struct {
	I2C_TypeDef *instance;		// インスタンス
	I2C_CFG irq;				// 通常割込み情報
	I2C_CFG err_irq;			// エラー割込み情報
} I2C_CH_CFG;

// 割込みハンドラのプロトタイプ宣言
static void I2C1_EV_IRQHandler(void);
static void I2C1_ER_IRQHandler(void);

// USARTチャネル固有情報テーブル
static const I2C_CH_CFG i2c_cfg_tbl[I2C_CH_MAX] =
{
	{ I2C1, { I2C1_EV_IRQn, I2C1_EV_IRQHandler, I2C1_EV_INTERRUPT_NO}, { I2C1_ER_IRQn, I2C1_ER_IRQHandler, I2C1_ER_INTERRUPT_NO}},
};
#define get_instance(ch)		(i2c_cfg_tbl[ch].instance)			// インスタンス取得マクロ
#define get_irq_type(ch)		(i2c_cfg_tbl[ch].irq.irq_type)			// 割込みタイプ取得マクロ
#define get_handler(ch)			(i2c_cfg_tbl[ch].irq.handler)			// 割り込みハンドラ取得マクロ
#define get_vec_no(ch)			(i2c_cfg_tbl[ch].irq.vec_no)			// 割り込み番号取得マクロ
#define get_err_irq_type(ch)	(i2c_cfg_tbl[ch].err_irq.irq_type)		// エラー割込みタイプ取得マクロ
#define get_err_handler(ch)		(i2c_cfg_tbl[ch].err_irq.handler)		// エラー割り込みハンドラ取得マクロ
#define get_err_vec_no(ch)		(i2c_cfg_tbl[ch].err_irq.vec_no)		// エラー割り込み番号取得マクロ

// 内部関数
// I2Cドライバのエラーハンドラ
static Error_Handler(void)
{
	while(1) {} ;
}

// サンプルから持ってきた割込みハンドラ
static void I2C1_EV_IRQHandler(void)
{
	/* USER CODE BEGIN I2C1_EV_IRQn 0 */
	I2C_CTL *this;
	
	// 制御ブロック取得
	this = get_myself(I2C_CH1);
	
	/* USER CODE END I2C1_EV_IRQn 0 */
	if (&(this->hi2c1) != 0) {
		HAL_I2C_EV_IRQHandler(&(this->hi2c1));
	}
	/* USER CODE BEGIN I2C1_EV_IRQn 1 */

	/* USER CODE END I2C1_EV_IRQn 1 */
}

static void I2C1_ER_IRQHandler(void)
{
	/* USER CODE BEGIN I2C1_ER_IRQn 0 */
	I2C_CTL *this;
	
	// 制御ブロック取得
	this = get_myself(I2C_CH1);
	
	/* USER CODE END I2C1_ER_IRQn 0 */
	if (&(this->hi2c1) != 0) {
		HAL_I2C_ER_IRQHandler(&(this->hi2c1));
	}
	/* USER CODE BEGIN I2C1_ER_IRQn 1 */
	
	/* USER CODE END I2C1_ER_IRQn 1 */
}

// 外部公開関数
// I2C初期化関数
void i2c_wrapper_init(void)
{
	I2C_CTL *this;
	uint32_t ch;
	
	for (ch = 0; ch < I2C_CH_MAX; ch++) {
		// 制御ブロック取得
		this = get_myself(ch);
		// 制御ブロック初期化
		memset(this, 0, sizeof(I2C_CTL));
		// 割込みハンドラ登録
		kz_setintr(get_vec_no(ch), get_handler(ch));
		kz_setintr(get_err_vec_no(ch), get_err_handler(ch));
	}
}

// I2Cオープン関数
int32_t i2c_wrapper_open(I2C_CH ch)
{
	I2C_CTL *this;
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= I2C_CH_MAX) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// オープンパラメータの設定
	this->hi2c1.Instance = get_instance(ch);
	this->hi2c1.Init.Timing = 0x00000004;
	this->hi2c1.Init.OwnAddress1 = 0;
	this->hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	this->hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	this->hi2c1.Init.OwnAddress2 = 0;
	this->hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	this->hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	this->hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	// I2Cの初期化
	if (HAL_I2C_Init(&(this->hi2c1)) != HAL_OK)
	{
		Error_Handler();
	}
	/** Configure Analogue filter
	*/
	if (HAL_I2CEx_ConfigAnalogFilter(&(this->hi2c1), I2C_ANALOGFILTER_ENABLE) != HAL_OK)
	{
		Error_Handler();
	}

	/** Configure Digital filter
	*/
	if (HAL_I2CEx_ConfigDigitalFilter(&(this->hi2c1), 0) != HAL_OK)
	{
		Error_Handler();
	}
	
	// 割込みの設定
	HAL_NVIC_SetPriority(get_irq_type(ch), INTERRPUT_PRIORITY_5, 0);
	HAL_NVIC_EnableIRQ(get_irq_type(ch));
	HAL_NVIC_SetPriority(get_err_irq_type(ch), INTERRPUT_PRIORITY_5, 0);
	HAL_NVIC_EnableIRQ(get_err_irq_type(ch));

	return 0;
}

// I2C送信関数
int32_t i2c_wrapper_send(I2C_CH ch, uint8_t slave_addr, uint8_t *data, uint32_t size) 
{
	I2C_CTL *this;
	
	// チャネル番号が範囲外の場合エラーを返して終了
	if (ch >= I2C_CH_MAX) {
		return -1;
	}
	
	// 送信データがNULLの場合、エラーを返して終了
	if (data == NULL) {
		return -1;
	}
	
	// 制御ブロック取得
	this = get_myself(ch);
	
	// データを送信(sampleを参考)
	while(HAL_I2C_Master_Transmit_IT(&(this->hi2c1), (uint16_t)(slave_addr << 1), data, size)!= HAL_OK) {
		/* Error_Handler() function is called when Timout error occurs.
		 When Acknowledge failure ocucurs (Slave don't acknowledge it's address)
		 Master restarts communication */
		if (HAL_I2C_GetError(&(this->hi2c1)) != HAL_I2C_ERROR_AF) {
			Error_Handler();
		}
	}
	
	return 0;
}
