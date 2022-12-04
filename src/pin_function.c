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
	{ GPIOA, { GPIO_PIN_9,  GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART1 }},	// USART1 TX
	{ GPIOA, { GPIO_PIN_10, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART1 }},	// USART1 RX
	{ GPIOA, { GPIO_PIN_2,  GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART2 }},	// USART2 TX
	{ GPIOA, { GPIO_PIN_3,  GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF7_USART2 }},	// USART2 RX
	{ GPIOE, { GPIO_PIN_0,  GPIO_MODE_INPUT, GPIO_PULLUP, 0U					   , 0U				 }},	// 上ボタン
	{ GPIOE, { GPIO_PIN_1,  GPIO_MODE_INPUT, GPIO_PULLUP, 0U					   , 0U				 }},	// 下ボタン
	{ GPIOE, { GPIO_PIN_2,  GPIO_MODE_INPUT, GPIO_PULLUP, 0U					   , 0U				 }},	// 戻るボタン
	{ GPIOE, { GPIO_PIN_3,  GPIO_MODE_INPUT, GPIO_PULLUP, 0U					   , 0U				 }},	// 決定ボタン
	{ GPIOG, { GPIO_PIN_13, GPIO_MODE_AF_OD, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF4_I2C1	 }},	// I2C1 SDA
	{ GPIOG, { GPIO_PIN_14, GPIO_MODE_AF_OD, GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF4_I2C1	 }},	// I2C1 SCL
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