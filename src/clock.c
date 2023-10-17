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

static void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	*/
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	* in the RCC_OscInitTypeDef structure.
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = 0;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 60;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	*/
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
	                          |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
	{
		Error_Handler();
	}
}

// 外部公開関数
// クロックの有効化
void periferal_clock_init(void)
{
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
	
	// クロックの設定
	SystemClock_Config();
	
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
	
	// OCTOSPIクロックの設定
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_OSPI;
	PeriphClkInit.OspiClockSelection = RCC_OSPICLKSOURCE_SYSCLK;
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
	__HAL_RCC_OSPIM_CLK_ENABLE();		// OCTOSPIMクロックの設定
	__HAL_RCC_OSPI1_CLK_ENABLE();		// OCTOSPI1クロックの設定
	__HAL_RCC_OSPI2_CLK_ENABLE();		// OCTOSPI1クロックの設定
	__HAL_RCC_TIM2_CLK_ENABLE();		// TIM2のクロックを有効
	__HAL_RCC_TIM3_CLK_ENABLE();		// TIM3のクロックを有効
	__HAL_RCC_TIM4_CLK_ENABLE();		// TIM4のクロックを有効
	__HAL_RCC_TIM5_CLK_ENABLE();		// TIM5のクロックを有効
	__HAL_RCC_TIM15_CLK_ENABLE();		// TIM15のクロックを有効
	__HAL_RCC_TIM16_CLK_ENABLE();		// TIM16のクロックを有効
	__HAL_RCC_TIM17_CLK_ENABLE();		// TIM17のクロックを有効
	__HAL_RCC_DMA1_CLK_ENABLE();		// DMA1のクロック有効
	__HAL_RCC_DMAMUX1_CLK_ENABLE();		// DMAMUXのクロック有効
	
	// GPIOのクロックを有効にする
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();
	
	HAL_PWREx_EnableVddIO2();
}