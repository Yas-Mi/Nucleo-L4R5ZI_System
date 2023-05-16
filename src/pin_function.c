#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_pwr_ex.h"
#include "stm32l4xx_hal_cortex.h"
#include "stm32l4xx.h"

typedef struct {
	GPIO_TypeDef	 	*pin_group;
	GPIO_InitTypeDef	pin_cfg;
} PIN_FUNC_INFO;

static const PIN_FUNC_INFO pin_func[] = {
	{ GPIOA, { GPIO_PIN_9,  GPIO_MODE_AF_PP,     GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART1 }},	// USART1 TX
	{ GPIOA, { GPIO_PIN_10, GPIO_MODE_AF_PP,     GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART1 }},	// USART1 RX
	{ GPIOA, { GPIO_PIN_2,  GPIO_MODE_AF_PP,     GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART2 }},	// USART2 TX
	{ GPIOA, { GPIO_PIN_3,  GPIO_MODE_AF_PP,     GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART2 }},	// USART2 RX
	{ GPIOG, { GPIO_PIN_3,  GPIO_MODE_INPUT,     GPIO_NOPULL, 0U,                        0U				 }},	// BlueToothの接続状態
	{ GPIOE, { GPIO_PIN_0,  GPIO_MODE_INPUT,     GPIO_PULLUP, 0U,                        0U				 }},	// 上ボタン
	{ GPIOE, { GPIO_PIN_1,  GPIO_MODE_INPUT,     GPIO_PULLUP, 0U,                        0U				 }},	// 下ボタン
	{ GPIOE, { GPIO_PIN_2,  GPIO_MODE_INPUT,     GPIO_PULLUP, 0U,                        0U				 }},	// 戻るボタン
	{ GPIOE, { GPIO_PIN_3,  GPIO_MODE_INPUT,     GPIO_PULLUP, 0U,                        0U				 }},	// 決定ボタン
	{ GPIOG, { GPIO_PIN_13, GPIO_MODE_AF_OD,     GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF4_I2C1	 }},	// I2C1 SDA
	{ GPIOG, { GPIO_PIN_14, GPIO_MODE_AF_OD,     GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF4_I2C1	 }},	// I2C1 SCL
	{ GPIOB, { GPIO_PIN_10, GPIO_MODE_AF_OD,     GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF4_I2C2	 }},	// I2C2 SDA
	{ GPIOB, { GPIO_PIN_11, GPIO_MODE_AF_OD,     GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF4_I2C2	 }},	// I2C2 SCL
	{ GPIOE, { GPIO_PIN_4,  GPIO_MODE_AF_PP,     GPIO_NOPULL, GPIO_SPEED_FREQ_LOW,       GPIO_AF13_SAI1	 }},	// SAI1 FS
	{ GPIOE, { GPIO_PIN_5,  GPIO_MODE_AF_PP,     GPIO_NOPULL, GPIO_SPEED_FREQ_LOW,       GPIO_AF13_SAI1	 }},	// SAI1 SCK
	{ GPIOE, { GPIO_PIN_6,  GPIO_MODE_AF_PP,     GPIO_NOPULL, GPIO_SPEED_FREQ_LOW,       GPIO_AF13_SAI1	 }},	// SAI1 SD
//	{ GPIOE, { GPIO_PIN_9,  GPIO_MODE_AF_PP,     GPIO_NOPULL, GPIO_SPEED_FREQ_LOW,       GPIO_AF1_TIM1	 }},	// TIM1_CH1 (*) tim.cでインプットキャプチャ使用時に設定
	{ GPIOG, { GPIO_PIN_7,  GPIO_MODE_AF_PP,     GPIO_NOPULL, GPIO_SPEED_FREQ_LOW,       GPIO_AF13_SAI1	 }},	// SAI1 MCLK
	{ GPIOC, { GPIO_PIN_13, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW,       0U				 }},	// PCM3060 RST
};

// 外部公開関数
// ピンファンクションの有効化
void pin_function_init(void)
{
	uint32_t size;
	uint32_t i;
	
	// サイズを取得
	size = sizeof(pin_func)/sizeof(pin_func[0]);
	
	for (i = 0; i < size; i++) {
		// pinファンクション初期化
		HAL_GPIO_Init(pin_func[i].pin_group, &(pin_func[i].pin_cfg));
	}
}