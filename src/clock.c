#include "stm32l4xx_hal_rcc.h"
#include "stm32l4xx_hal_gpio.h"
#include "stm32l4xx_hal_pwr_ex.h"
#include "stm32l4xx_hal_cortex.h"
#include "stm32l4xx.h"

//内部関数
// エラーハンドラ
static Error_Handler(void)
{
	while(1) {} ;
}

// 外部公開関数
// クロックの有効化
void periferal_clock_init(void)
{
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
	
	// I2Cクロックの設定
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
	PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
	{
		Error_Handler();
	}
	
	// 各ペリフェラルのクロックを有効にする
	__HAL_RCC_USART1_CLK_ENABLE();		// USART1のクロックを有効
	__HAL_RCC_USART2_CLK_ENABLE();		// USART2のクロックを有効
	__HAL_RCC_USART3_CLK_ENABLE();		// USART3のクロックを有効
	__HAL_RCC_I2C1_CLK_ENABLE();		// I2C1のクロックを有効
	
	// GPIOのクロックを有効にする
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	HAL_PWREx_EnableVddIO2();
}