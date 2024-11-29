/**
 ******************************************************************************
 * @file   	system.c
 * @brief  	...
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */
#include "main.h"
#include "drv_system.h"
#include "drv_gpio.h"
#include "drv_uart.h"
#include "drv_ain.h"
#include "drv_timer.h"
#include "drv_aout.h"
#include "drv_spi.h"

#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif


static TIM_HandleTypeDef htim1;

/* Wait until CPU2 boots and enters in stop mode or timeout*/
static drv_system_status_t prvDRV_SYSTEM_CPU2_Wait(uint32_t timeout)
{
	while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
	if ( timeout < 0 ) return DRV_SYSTEM_STATUS_ERROR;
	return DRV_SYSTEM_STATUS_OK;
}

static void prvDRV_SYSTEM_CACHE_Enable()
{
	SCB_EnableICache();

	SCB_EnableDCache();
}

static drv_system_status_t prvDRV_SYSTEM_CLOCK_Init()
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Supply configuration update enable
	*/
	HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

	/** Configure the main internal regulator output voltage
	*/
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

	/** Initializes the RCC Oscillators according to the specified parameters
	* in the RCC_OscInitTypeDef structure.
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 50;
	RCC_OscInitStruct.PLL.PLLP = 2;
	RCC_OscInitStruct.PLL.PLLQ = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
	RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
	RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
	RCC_OscInitStruct.PLL.PLLFRACN = 0;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) return DRV_SYSTEM_STATUS_ERROR;

	/** Initializes the CPU, AHB and APB buses clocks
	*/
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
							  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
							  |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
	RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) return DRV_SYSTEM_STATUS_ERROR;

	return DRV_SYSTEM_STATUS_OK;
}

static drv_system_status_t prvDRV_SYSTEM_MPU_Init()
{
	MPU_Region_InitTypeDef MPU_InitStruct = {0};

	/* Disables the MPU */
	HAL_MPU_Disable();

	/** Initializes and configures the Region and the memory to be protected
	*/
	MPU_InitStruct.Enable 			= MPU_REGION_ENABLE;
	MPU_InitStruct.Number			= MPU_REGION_NUMBER0;
	MPU_InitStruct.BaseAddress 		= 0;
	MPU_InitStruct.Size 			= MPU_REGION_SIZE_4GB;
	MPU_InitStruct.SubRegionDisable = 0x87;
	MPU_InitStruct.TypeExtField 	= MPU_TEX_LEVEL0;
	MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
	MPU_InitStruct.DisableExec 		= MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.IsShareable 		= MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.IsCacheable 		= MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsBufferable 	= MPU_ACCESS_NOT_BUFFERABLE;

	HAL_MPU_ConfigRegion(&MPU_InitStruct);

	/* Configure the MPU attributes as Normal Non Cacheable
	 for LwIP RAM heap which contains the Tx buffers */
	MPU_InitStruct.Enable 			= MPU_REGION_ENABLE;
	MPU_InitStruct.Number 			= MPU_REGION_NUMBER1;
	MPU_InitStruct.BaseAddress 		= 0x30020000;
	MPU_InitStruct.Size 			= MPU_REGION_SIZE_128KB;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.TypeExtField 	= MPU_TEX_LEVEL1;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.DisableExec 		= MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.IsShareable 		= MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.IsCacheable 		= MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsBufferable 	= MPU_ACCESS_NOT_BUFFERABLE;

	HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Configure the MPU attributes as Device not cacheable
	 for ETH DMA descriptors */
	MPU_InitStruct.Enable 			= MPU_REGION_ENABLE;
	MPU_InitStruct.Number 			= MPU_REGION_NUMBER2;
	MPU_InitStruct.BaseAddress 		= 0x30040000;
	MPU_InitStruct.Size 			= MPU_REGION_SIZE_512B;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.TypeExtField 	= MPU_TEX_LEVEL0;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.DisableExec 		= MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.IsShareable 		= MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.IsCacheable 		= MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsBufferable 	= MPU_ACCESS_BUFFERABLE;

	HAL_MPU_ConfigRegion(&MPU_InitStruct);

	/** Initializes and configures the Region and the memory to be protected
	*/
	MPU_InitStruct.Enable 			= MPU_REGION_ENABLE;
	MPU_InitStruct.Number 			= MPU_REGION_NUMBER3;
	MPU_InitStruct.BaseAddress 		= 0x38000000;
	MPU_InitStruct.Size 			= MPU_REGION_SIZE_8KB;
	MPU_InitStruct.SubRegionDisable = 0x0;
	MPU_InitStruct.TypeExtField 	= MPU_TEX_LEVEL0;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.DisableExec 		= MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.IsShareable 		= MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.IsCacheable 		= MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsBufferable 	= MPU_ACCESS_BUFFERABLE;

	HAL_MPU_ConfigRegion(&MPU_InitStruct);

	/* Enables the MPU */
	HAL_MPU_Enable(MPU_HFNMI_PRIVDEF);

	return DRV_SYSTEM_STATUS_OK;
}

drv_system_status_t	DRV_SYSTEM_InitCoreFunc()
{
	prvDRV_SYSTEM_CACHE_Enable();
	if(prvDRV_SYSTEM_CPU2_Wait(0xFFFF) != DRV_SYSTEM_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;
	if(HAL_Init() != HAL_OK) return DRV_SYSTEM_STATUS_ERROR;
	if(prvDRV_SYSTEM_MPU_Init() != DRV_SYSTEM_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;
	if(prvDRV_SYSTEM_CLOCK_Init() != DRV_SYSTEM_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;

	__HAL_RCC_HSEM_CLK_ENABLE();
	/*Take HSEM */
	HAL_HSEM_FastTake(HSEM_ID_0);
	/*Release HSEM in order to notify the CPU2(CM4)*/
	HAL_HSEM_Release(HSEM_ID_0,0);


	if(prvDRV_SYSTEM_CPU2_Wait(0xFFFF) != DRV_SYSTEM_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;


	return DRV_SYSTEM_STATUS_OK;
}
drv_system_status_t	DRV_SYSTEM_InitDrivers()
{
	if(DRV_GPIO_Init() != DRV_GPIO_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;
	if(DRV_UART_Init() != DRV_UART_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;
	if(DRV_SPI_Init() != DRV_SPI_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;
	if(DRV_AIN_Init(DRV_AIN_ADC_3, NULL) != DRV_AIN_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;

	if(DRV_Timer_Init() != DRV_TIMER_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;
	if(DRV_AOUT_Init() != DRV_AOUT_STATUS_OK) return DRV_SYSTEM_STATUS_ERROR;


	return DRV_SYSTEM_STATUS_OK;
}
