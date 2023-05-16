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
	// ch1
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
	PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
	{
		Error_Handler();
	}
	// ch2
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
	PeriphClkInit.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
	{
		Error_Handler();
	}
	
	// SAIクロックの設定
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SAI1;
	PeriphClkInit.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLLSAI1;
	PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_MSI;
	PeriphClkInit.PLLSAI1.PLLSAI1M = 2;
	PeriphClkInit.PLLSAI1.PLLSAI1N = 12;
	PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV2;
	PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV2;
	PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV2;
	PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_SAI1CLK;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
	{
		Error_Handler();
	}
	
	// 各ペリフェラルのクロックを有効にする
	__HAL_RCC_USART1_CLK_ENABLE();		// USART1のクロックを有効
	__HAL_RCC_USART2_CLK_ENABLE();		// USART2のクロックを有効
	__HAL_RCC_USART3_CLK_ENABLE();		// USART3のクロックを有効
	__HAL_RCC_I2C1_CLK_ENABLE();		// I2C1のクロックを有効
	__HAL_RCC_I2C2_CLK_ENABLE();		// I2C2のクロックを有効
	__HAL_RCC_SAI1_CLK_ENABLE();		// SAI1のクロックを有効
	__HAL_RCC_TIM2_CLK_ENABLE();		// TIM2のクロックを有効
	__HAL_RCC_TIM3_CLK_ENABLE();		// TIM3のクロックを有効
	__HAL_RCC_TIM4_CLK_ENABLE();		// TIM4のクロックを有効
	__HAL_RCC_TIM5_CLK_ENABLE();		// TIM5のクロックを有効
	__HAL_RCC_TIM15_CLK_ENABLE();		// TIM15のクロックを有効
	__HAL_RCC_TIM16_CLK_ENABLE();		// TIM16のクロックを有効
	__HAL_RCC_TIM17_CLK_ENABLE();		// TIM17のクロックを有効
	
	// GPIOのクロックを有効にする
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	HAL_PWREx_EnableVddIO2();
}