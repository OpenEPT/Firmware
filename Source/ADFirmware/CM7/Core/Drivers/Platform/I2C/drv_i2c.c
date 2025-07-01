/**
 ******************************************************************************
 * @file    drv_i2c.c
 *
 * @brief   I2C driver implementation This file contains the implementation of 
 *          the I2C driver for STM32H7 microcontrollers.
 *          It provides functionality for I2C communication in master mode.
 *
 * @author  elektronika
 * @date    Apr 10, 2025
 ******************************************************************************
 */

#include "main.h"
#include "drv_i2c.h"
#include "FreeRTOS.h"
#include "semphr.h"
/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup I2C_DRIVER I2C Driver
 * @{
 */

 /**
 * @defgroup I2C_PRIVATE_STRUCTURES I2C driver private structures
 * @{
 */

typedef struct drv_i2c_handle_t
{
	drv_i2c_instance_t					instance;   /**< I2C instance identifier */
	drv_i2c_initialization_status_t		initState;  /**< Initialization state of the I2C instance */
	drv_i2c_config_t					config;     /**< Configuration parameters for the I2C instance */
	SemaphoreHandle_t					lock;       /**< Mutex for thread-safe access to the I2C instance */
	I2C_HandleTypeDef 					deviceHandler; /**< HAL I2C handle */
}drv_i2c_handle_t;

/**
 * @}
 */

/**
 * @defgroup I2C_PRIVATE_DATA I2C driver private data
 * @{
 */

/** @brief I2C driver handle for I2C1 instance */
static drv_i2c_handle_t prvDRV_I2C1_INSTANCE;

/**
 * @}
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
	if(hi2c->Instance==I2C1)
	{
		/* USER CODE BEGIN I2C1_MspInit 0 */

		/* USER CODE END I2C1_MspInit 0 */

		/** Initializes the peripherals clock
		*/
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
		PeriphClkInitStruct.I2c123ClockSelection = RCC_I2C123CLKSOURCE_D2PCLK1;
		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) return;

		__HAL_RCC_GPIOB_CLK_ENABLE();
		/**I2C1 GPIO Configuration
		PB6     ------> I2C1_SCL
		PB7     ------> I2C1_SDA
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		/* Peripheral clock enable */
		__HAL_RCC_I2C1_CLK_ENABLE();
	}

}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
	if(hi2c->Instance==I2C1)
	{
		/* USER CODE BEGIN I2C1_MspDeInit 0 */

		/* USER CODE END I2C1_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_I2C1_CLK_DISABLE();

		/**I2C1 GPIO Configuration
		PB6     ------> I2C1_SCL
		PB7     ------> I2C1_SDA
		*/
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6);

		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_7);
	}
}

drv_i2c_status_t 	DRV_I2C_Init()
{
	memset(&prvDRV_I2C1_INSTANCE, 0, sizeof(drv_i2c_handle_t));
	return DRV_I2C_STATUS_OK;
}
drv_i2c_status_t	DRV_I2C_Instance_Init(drv_i2c_instance_t instance, drv_i2c_config_t* config)
{

	prvDRV_I2C1_INSTANCE.instance = instance;
	switch(instance)
	{
	case DRV_I2C_INSTANCE_1:
		prvDRV_I2C1_INSTANCE.deviceHandler.Instance = I2C1;
		break;
	default:
		return DRV_I2C_STATUS_ERROR;
	}
	prvDRV_I2C1_INSTANCE.deviceHandler.Init.Timing = 0x10C0ECFF;
	prvDRV_I2C1_INSTANCE.deviceHandler.Init.OwnAddress1 = 0;
	prvDRV_I2C1_INSTANCE.deviceHandler.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	prvDRV_I2C1_INSTANCE.deviceHandler.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	prvDRV_I2C1_INSTANCE.deviceHandler.Init.OwnAddress2 = 0;
	prvDRV_I2C1_INSTANCE.deviceHandler.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	prvDRV_I2C1_INSTANCE.deviceHandler.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	prvDRV_I2C1_INSTANCE.deviceHandler.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&prvDRV_I2C1_INSTANCE.deviceHandler) != HAL_OK) return DRV_I2C_STATUS_ERROR;
	if (HAL_I2CEx_ConfigAnalogFilter(&prvDRV_I2C1_INSTANCE.deviceHandler, I2C_ANALOGFILTER_ENABLE) != HAL_OK)  return DRV_I2C_STATUS_ERROR;
	if (HAL_I2CEx_ConfigDigitalFilter(&prvDRV_I2C1_INSTANCE.deviceHandler, 0) != HAL_OK) return DRV_I2C_STATUS_ERROR;

	return DRV_I2C_STATUS_OK;
}
drv_i2c_status_t	DRV_I2C_Transmit(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size, uint32_t timeout)
{
	if(HAL_I2C_Master_Transmit(&prvDRV_I2C1_INSTANCE.deviceHandler, addr, data, size, timeout) != HAL_OK) return DRV_I2C_STATUS_ERROR;
	return DRV_I2C_STATUS_OK;
}
drv_i2c_status_t	DRV_I2C_Receive(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size, uint32_t timeout)
{
	if(HAL_I2C_Master_Receive(&prvDRV_I2C1_INSTANCE.deviceHandler, addr, data, size, timeout) != HAL_OK) return DRV_I2C_STATUS_ERROR;
	return DRV_I2C_STATUS_OK;
}


/**
 * @}
 */
/**
 * @}
 */
