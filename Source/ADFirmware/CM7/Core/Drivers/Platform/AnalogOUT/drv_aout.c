/*
 * analogout.c
 *
 *  Created on: Nov 5, 2023
 *      Author: Haris
 */

#include "drv_aout.h"


#include "stm32h7xx_hal.h"

static DAC_HandleTypeDef 		prvDRV_AOUT_DAC_HANDLER;
static drv_aout_active_status 	prvDRV_AOUT_DAC_ACTIVE_STATUS;

/**
* @brief DAC MSP Initialization
* This function configures the hardware resources used in this example
* @param hdac: DAC handle pointer
* @retval None
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

/**
* @brief DAC MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param hdac: DAC handle pointer
* @retval None
*/
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

static drv_aout_status_t prvDRV_AOUT_Init()
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
	if(prvDRV_AOUT_Init() != DRV_AOUT_STATUS_OK) return DRV_AOUT_STATUS_ERROR;
	prvDRV_AOUT_DAC_ACTIVE_STATUS = DRV_AOUT_ACTIVE_STATUS_DISABLED;
	HAL_DAC_Stop(&prvDRV_AOUT_DAC_HANDLER, DAC_CHANNEL_2);
	return DRV_AOUT_STATUS_OK;
}

drv_aout_status_t DRV_AOUT_SetEnable(drv_aout_active_status aStatus)
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
drv_aout_status_t DRV_AOUT_SetValue(uint32_t value)
{
	if(HAL_DAC_SetValue(&prvDRV_AOUT_DAC_HANDLER, DAC_CHANNEL_2, DAC_ALIGN_12B_R, value) != HAL_OK) return DRV_AOUT_STATUS_ERROR;
	return DRV_AOUT_STATUS_OK;
}

