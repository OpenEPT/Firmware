/**
 ******************************************************************************
 * @file    drv_i2c.c
 *
 * @brief   I2C driver implementation
 *          This file contains the implementation of the I2C driver for
 *          STM32H7 microcontrollers. It provides functionality for I2C
 *          communication in master mode with support for multiple instances.
 *
 * @author  elektronika
 * @date    Apr 10, 2025
 ******************************************************************************
 */

#include "main.h"
#include "drv_i2c.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include <string.h>

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
	drv_i2c_instance_t					instance;		/**< I2C instance identifier */
	drv_i2c_initialization_status_t		initState;		/**< Initialization state of the I2C instance */
	drv_i2c_config_t					config;			/**< Configuration parameters for the I2C instance */
	SemaphoreHandle_t					lock;			/**< Mutex for thread-safe access to the I2C instance */
	I2C_HandleTypeDef 					deviceHandler;	/**< HAL I2C handle */
} drv_i2c_handle_t;

/**
 * @}
 */

/**
 * @defgroup I2C_PRIVATE_DATA I2C driver private data
 * @{
 */

/** @brief Array of I2C driver handles, one for each supported I2C instance */
static drv_i2c_handle_t prvDRV_I2C_INSTANCES[DRV_I2C_INSTANCES_MAX_NUMBER];

/**
 * @}
 */

/**
 * @defgroup I2C_PRIVATE_FUNCTIONS I2C driver private functions
 * @{
 */

/**
 * @brief Validate I2C instance index
 * @param instance: I2C instance to validate
 * @retval 1 if valid, 0 otherwise
 */
static uint8_t prvDRV_I2C_IsValidInstance(drv_i2c_instance_t instance)
{
	return ((uint32_t)instance < DRV_I2C_INSTANCES_MAX_NUMBER) ? 1U : 0U;
}

/**
 * @brief Map driver instance to HAL I2C peripheral instance
 * @param instance: Driver I2C instance
 * @retval Pointer to HAL peripheral instance, or NULL if invalid
 */
static I2C_TypeDef* prvDRV_I2C_GetPeripheral(drv_i2c_instance_t instance)
{
	switch(instance)
	{
	case DRV_I2C_INSTANCE_1:
		return I2C1;

	case DRV_I2C_INSTANCE_2:
		return I2C2;

	case DRV_I2C_INSTANCE_4:
		return I2C4;

	default:
		return NULL;
	}
}

/**
 * @}
 */

void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

	if(hi2c->Instance == I2C1)
	{
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
		PeriphClkInitStruct.I2c123ClockSelection = RCC_I2C123CLKSOURCE_D2PCLK1;
		if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
		{
			Error_Handler();
		}

		__HAL_RCC_GPIOB_CLK_ENABLE();

		/**I2C1 GPIO Configuration
		PB6     ------> I2C1_SCL
		PB7     ------> I2C1_SDA
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		__HAL_RCC_I2C1_CLK_ENABLE();
	}
	else if(hi2c->Instance == I2C4)
	{
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C4;
		PeriphClkInitStruct.I2c4ClockSelection = RCC_I2C4CLKSOURCE_D3PCLK1;
		if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
		{
			Error_Handler();
		}

		__HAL_RCC_GPIOF_CLK_ENABLE();

		/**I2C4 GPIO Configuration
		PF14    ------> I2C4_SCL
		PF15    ------> I2C4_SDA
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C4;
		HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

		__HAL_RCC_I2C4_CLK_ENABLE();
	}
	else
	{
		/* Unsupported instance */
	}
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
	if(hi2c->Instance == I2C1)
	{
		__HAL_RCC_I2C1_CLK_DISABLE();

		/**I2C1 GPIO Configuration
		PB6     ------> I2C1_SCL
		PB7     ------> I2C1_SDA
		*/
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
	}
	else if(hi2c->Instance == I2C4)
	{
		__HAL_RCC_I2C4_CLK_DISABLE();

		/**I2C4 GPIO Configuration
		PF14    ------> I2C4_SCL
		PF15    ------> I2C4_SDA
		*/
		HAL_GPIO_DeInit(GPIOF, GPIO_PIN_14 | GPIO_PIN_15);
	}
	else
	{
		/* Unsupported instance */
	}
}

drv_i2c_status_t DRV_I2C_Init(void)
{
	memset(prvDRV_I2C_INSTANCES, 0, sizeof(prvDRV_I2C_INSTANCES));
	return DRV_I2C_STATUS_OK;
}

drv_i2c_status_t DRV_I2C_Instance_Init(drv_i2c_instance_t instance, drv_i2c_config_t* config)
{
	I2C_TypeDef* peripheral = NULL;
	drv_i2c_handle_t* handle = NULL;

	if((prvDRV_I2C_IsValidInstance(instance) == 0U) || (config == NULL))
	{
		return DRV_I2C_STATUS_ERROR;
	}

	handle = &prvDRV_I2C_INSTANCES[instance];

	if(handle->lock == NULL)
	{
		handle->lock = xSemaphoreCreateMutex();
		if(handle->lock == NULL)
		{
			return DRV_I2C_STATUS_ERROR;
		}
	}

	handle->instance = instance;
	handle->config = *config;

	peripheral = prvDRV_I2C_GetPeripheral(instance);
	if(peripheral == NULL)
	{
		return DRV_I2C_STATUS_ERROR;
	}

	handle->deviceHandler.Instance = peripheral;

	/* Default timing value kept from original driver */
	handle->deviceHandler.Init.Timing           = 0x10C0ECFF;
	handle->deviceHandler.Init.OwnAddress1      = 0;
	handle->deviceHandler.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
	handle->deviceHandler.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
	handle->deviceHandler.Init.OwnAddress2      = 0;
	handle->deviceHandler.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	handle->deviceHandler.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
	handle->deviceHandler.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

	if(HAL_I2C_Init(&handle->deviceHandler) != HAL_OK)
	{
		return DRV_I2C_STATUS_ERROR;
	}

	if(HAL_I2CEx_ConfigAnalogFilter(&handle->deviceHandler, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
	{
		(void)HAL_I2C_DeInit(&handle->deviceHandler);
		return DRV_I2C_STATUS_ERROR;
	}

	if(HAL_I2CEx_ConfigDigitalFilter(&handle->deviceHandler, 0) != HAL_OK)
	{
		(void)HAL_I2C_DeInit(&handle->deviceHandler);
		return DRV_I2C_STATUS_ERROR;
	}

	handle->initState = DRV_I2C_INITIALIZATION_STATUS_INIT;

	return DRV_I2C_STATUS_OK;
}

drv_i2c_status_t DRV_I2C_Instance_DeInit(drv_i2c_instance_t instance)
{
	drv_i2c_handle_t* handle = NULL;

	if(prvDRV_I2C_IsValidInstance(instance) == 0U)
	{
		return DRV_I2C_STATUS_ERROR;
	}

	handle = &prvDRV_I2C_INSTANCES[instance];

	if(handle->initState != DRV_I2C_INITIALIZATION_STATUS_INIT)
	{
		return DRV_I2C_STATUS_ERROR;
	}

	if(HAL_I2C_DeInit(&handle->deviceHandler) != HAL_OK)
	{
		return DRV_I2C_STATUS_ERROR;
	}

	handle->initState = DRV_I2C_INITIALIZATION_STATUS_NOINIT;

	return DRV_I2C_STATUS_OK;
}

drv_i2c_status_t DRV_I2C_Transmit(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size, uint32_t timeout)
{
	drv_i2c_handle_t* handle = NULL;
	drv_i2c_status_t status = DRV_I2C_STATUS_OK;

	if((prvDRV_I2C_IsValidInstance(instance) == 0U) || (data == NULL))
	{
		return DRV_I2C_STATUS_ERROR;
	}

	handle = &prvDRV_I2C_INSTANCES[instance];

	if((handle->initState != DRV_I2C_INITIALIZATION_STATUS_INIT) || (handle->lock == NULL))
	{
		return DRV_I2C_STATUS_ERROR;
	}

	if(xSemaphoreTake(handle->lock, pdMS_TO_TICKS(timeout)) != pdTRUE)
	{
		return DRV_I2C_STATUS_ERROR;
	}

	if(HAL_I2C_Master_Transmit(&handle->deviceHandler, addr, data, size, timeout) != HAL_OK)
	{
		status = DRV_I2C_STATUS_ERROR;
	}

	if(xSemaphoreGive(handle->lock) != pdTRUE)
	{
		status = DRV_I2C_STATUS_ERROR;
	}

	return status;
}

drv_i2c_status_t DRV_I2C_Receive(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size, uint32_t timeout)
{
	drv_i2c_handle_t* handle = NULL;
	drv_i2c_status_t status = DRV_I2C_STATUS_OK;

	if((prvDRV_I2C_IsValidInstance(instance) == 0U) || (data == NULL))
	{
		return DRV_I2C_STATUS_ERROR;
	}

	handle = &prvDRV_I2C_INSTANCES[instance];

	if((handle->initState != DRV_I2C_INITIALIZATION_STATUS_INIT) || (handle->lock == NULL))
	{
		return DRV_I2C_STATUS_ERROR;
	}

	if(xSemaphoreTake(handle->lock, pdMS_TO_TICKS(timeout)) != pdTRUE)
	{
		return DRV_I2C_STATUS_ERROR;
	}

	if(HAL_I2C_Master_Receive(&handle->deviceHandler, addr, data, size, timeout) != HAL_OK)
	{
		status = DRV_I2C_STATUS_ERROR;
	}

	if(xSemaphoreGive(handle->lock) != pdTRUE)
	{
		status = DRV_I2C_STATUS_ERROR;
	}

	return status;
}

/**
 * @}
 */
/**
 * @}
 */
