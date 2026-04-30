/**
 ******************************************************************************
 * @file    drv_aout.c
 * @brief   Analog Output driver implementation This file contains the 
 *          implementation of the Analog Output driver for STM32H7 microcontrollers.
 *          It provides functionality for controlling the DAC peripheral.
 *
 * @author  Haris
 * @email   haris.turkmanovic@gmail.com
 * @date    Nov 5, 2023
 ******************************************************************************
 */

#include "drv_aout.h"
#include "stm32h7xx_hal.h"
#include "DAC6578/dac6578.h"
/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup AOUT_DRIVER AOUT Driver
 * @{
 */

/**
 * @defgroup AOUT_PRIVATE_DATA AOUT driver private data
 * @{
 */
/** @brief DAC handle for hardware access */
static DAC_HandleTypeDef 		prvDRV_AOUT_DAC_HANDLER;

/** @brief Status of the DAC output (enabled/disabled) */
static drv_aout_active_status_t 	prvDRV_AOUT_DAC_ACTIVE_STATUS;
/**
 * @}
 */
void HAL_DAC_MspInit(DAC_HandleTypeDef* hdac)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	if(hdac->Instance==DAC1)
	{
		/* USER CODE BEGIN DAC1_MspInit 0 */

		/* USER CODE END DAC1_MspInit 0 */
		/* Peripheral clock enable */
		__HAL_RCC_DAC12_CLK_ENABLE();

		__HAL_RCC_GPIOA_CLK_ENABLE();
		/**DAC1 GPIO Configuration
		PA5     ------> DAC1_OUT2
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_5;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* DAC1 interrupt Init */
		HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 15, 0);
		HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
	}

}

void HAL_DAC_MspDeInit(DAC_HandleTypeDef* hdac)
{
	if(hdac->Instance==DAC1)
	{
		/* USER CODE BEGIN DAC1_MspDeInit 0 */

		/* USER CODE END DAC1_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_DAC12_CLK_DISABLE();

		/**DAC1 GPIO Configuration
		PA5     ------> DAC1_OUT2
		*/
		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5);

		/* DAC1 interrupt DeInit */
		HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn);
		/* USER CODE BEGIN DAC1_MspDeInit 1 */

		/* USER CODE END DAC1_MspDeInit 1 */
	}

}

/**
 * @defgroup AOUT_PRIVATE_FUNCTIONS AOUT driver private functions
 * @{
 */

/**
 * @brief   Internal DAC initialization
 * @details This private function initializes the DAC hardware and configures channel 2
 *          with default settings.
 *
 * @return  DRV_AOUT_STATUS_OK if successful, DRV_AOUT_STATUS_ERROR otherwise
 */
static drv_aout_status_t prvDRV_AOUT_Init_Internal()
{
	DAC_ChannelConfTypeDef sConfig = {0};

	prvDRV_AOUT_DAC_HANDLER.Instance = DAC1;
	if (HAL_DAC_Init(&prvDRV_AOUT_DAC_HANDLER) != HAL_OK) return DRV_AOUT_STATUS_ERROR;

	/** DAC channel OUT2 config	*/
	sConfig.DAC_SampleAndHold	 = DAC_SAMPLEANDHOLD_DISABLE;
	sConfig.DAC_Trigger			 = DAC_TRIGGER_NONE;
	sConfig.DAC_OutputBuffer	 = DAC_OUTPUTBUFFER_ENABLE;
	sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_DISABLE;
	sConfig.DAC_UserTrimming	 = DAC_TRIMMING_FACTORY;
	if (HAL_DAC_ConfigChannel(&prvDRV_AOUT_DAC_HANDLER, &sConfig, DAC_CHANNEL_2) != HAL_OK) return DRV_AOUT_STATUS_ERROR;


	return DRV_AOUT_STATUS_OK;
}

drv_aout_status_t DRV_AOUT_Init()
{
	/*Set internal DAC*/
	if(prvDRV_AOUT_Init_Internal() != DRV_AOUT_STATUS_OK) return DRV_AOUT_STATUS_ERROR;
	prvDRV_AOUT_DAC_ACTIVE_STATUS = DRV_AOUT_ACTIVE_STATUS_DISABLED;
	if(HAL_DAC_SetValue(&prvDRV_AOUT_DAC_HANDLER, DAC_CHANNEL_2, DAC_ALIGN_12B_R, 0) != HAL_OK) return DRV_AOUT_STATUS_ERROR;
	HAL_DAC_Stop(&prvDRV_AOUT_DAC_HANDLER, DAC_CHANNEL_2);

	/*Set external DAC*/
	//if(DAC6578_Init() != DAC6578_STATUS_OK) return DRV_AOUT_STATUS_ERROR;
	return DRV_AOUT_STATUS_OK;
}

drv_aout_status_t DRV_AOUT_SetEnable(drv_aout_active_status_t aStatus)
{
	switch(aStatus)
	{
	case DRV_AOUT_ACTIVE_STATUS_DISABLED:
		prvDRV_AOUT_DAC_ACTIVE_STATUS = DRV_AOUT_ACTIVE_STATUS_DISABLED;
		HAL_DAC_Stop(&prvDRV_AOUT_DAC_HANDLER, DAC_CHANNEL_2);
		break;
	case DRV_AOUT_ACTIVE_STATUS_ENABLED:
		prvDRV_AOUT_DAC_ACTIVE_STATUS = DRV_AOUT_ACTIVE_STATUS_ENABLED;
		HAL_DAC_Start(&prvDRV_AOUT_DAC_HANDLER, DAC_CHANNEL_2);
		break;
	}
	return DRV_AOUT_STATUS_OK;
}
drv_aout_status_t DRV_AOUT_SetValue(uint32_t value, drv_aout_channel_t channel)
{
	uint8_t returnValue = 0;
	switch(channel)
	{
	case DRV_AOUT_CHANNEL_A:
		if(DAC6578_SetChannelValue(DRV_AOUT_CHANNEL_A, value, 1000) != DAC6578_STATUS_OK)
		{
			returnValue = 1;
		}
		break;
	case DRV_AOUT_CHANNEL_B:
		if(DAC6578_SetChannelValue(DRV_AOUT_CHANNEL_B, value, 1000) != DAC6578_STATUS_OK)
		{
			returnValue = 1;
		}
		break;
	case DRV_AOUT_CHANNEL_C:
		if(DAC6578_SetChannelValue(DRV_AOUT_CHANNEL_C, value, 1000) != DAC6578_STATUS_OK)
		{
			returnValue = 1;
		}
		break;
	case DRV_AOUT_CHANNEL_D:
//		if(DAC6578_SetChannelValue(DRV_AOUT_CHANNEL_D, value, 1000) != DAC6578_STATUS_OK)
//		{
//			returnValue = 1;
//		}
		if(HAL_DAC_SetValue(&prvDRV_AOUT_DAC_HANDLER, DAC_CHANNEL_2, DAC_ALIGN_12B_R, value) != HAL_OK) return DRV_AOUT_STATUS_ERROR;
		break;
	case DRV_AOUT_CHANNEL_E:
	case DRV_AOUT_CHANNEL_F:
	case DRV_AOUT_CHANNEL_G:
	case DRV_AOUT_CHANNEL_H:
		returnValue = 1;
		break;
	}
	return returnValue > 0 ? DRV_AOUT_STATUS_ERROR : DRV_AOUT_STATUS_OK;
}
drv_aout_status_t DRV_AOUT_SetVoltage(float voltage, drv_aout_channel_t channel)
{
    uint16_t dval = DRV_AOUT_ConvertFloatToDigital(voltage);
    return DRV_AOUT_SetValue(dval, channel);
}
uint16_t DRV_AOUT_ConvertFloatToDigital(float value)
{
    return DAC6578_FLOAT_TO_DVALUE(value);
}
/**
 * @}
 */
/**
 * @}
 */
/**
 * @}
 */

