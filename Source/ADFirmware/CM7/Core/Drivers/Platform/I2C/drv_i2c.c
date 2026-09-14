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
    DMA_HandleTypeDef                  	txDMAHandler;
    DMA_HandleTypeDef                   triggerDMAHandler;
    uint8_t                            	txDMAAddress;
    uint32_t                            triggerCR2;
    drv_i2c_tx_dma_complete_callback_t 	txDMACompleteCallback;
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
		GPIO_InitStruct.Pull = GPIO_PULLUP;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		__HAL_RCC_I2C1_CLK_ENABLE();
	}
	else if(hi2c->Instance==I2C2)
	{
		/* USER CODE BEGIN I2C2_MspInit 0 */

		/* USER CODE END I2C2_MspInit 0 */

		/** Initializes the peripherals clock
		*/
		PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
		PeriphClkInitStruct.I2c123ClockSelection = RCC_I2C123CLKSOURCE_D2PCLK1;
		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
		{
		  Error_Handler();
		}

		__HAL_RCC_GPIOB_CLK_ENABLE();
		/**I2C2 GPIO Configuration
		PB10     ------> I2C2_SCL
		PB11     ------> I2C2_SDA
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		/* Peripheral clock enable */
		__HAL_RCC_I2C2_CLK_ENABLE();

		HAL_NVIC_SetPriority(I2C2_EV_IRQn, 5U, 0U);
		HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
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
	else if(hi2c->Instance == I2C2)
	{
	    __HAL_RCC_I2C2_CLK_DISABLE();

	    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);

	    HAL_NVIC_DisableIRQ(I2C2_EV_IRQn);
	    HAL_NVIC_DisableIRQ(DMA2_Stream2_IRQn);
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

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    uint32_t i;

    for(i = 0U; i < DRV_I2C_INSTANCES_MAX_NUMBER; i++)
    {
        if(hi2c == &prvDRV_I2C_INSTANCES[i].deviceHandler)
        {
            if(prvDRV_I2C_INSTANCES[i].txDMACompleteCallback != NULL)
            {
                prvDRV_I2C_INSTANCES[i].txDMACompleteCallback();
            }

            break;
        }
    }
}


/**
 * @brief Initialize DMA used for I2C transmission
 * @param handle: Pointer to I2C driver handle
 * @retval ::drv_i2c_status_t
 */
static drv_i2c_status_t prvDRV_I2C_DMAInit(drv_i2c_handle_t* handle)
{
    if(handle == NULL)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    if(handle->instance != DRV_I2C_INSTANCE_2)
    {
        return DRV_I2C_STATUS_OK;
    }

    __HAL_RCC_DMA2_CLK_ENABLE();

    handle->txDMAHandler.Instance = DMA2_Stream2;
    handle->txDMAHandler.Init.Request = DMA_REQUEST_I2C2_TX;
    handle->txDMAHandler.Init.Direction = DMA_MEMORY_TO_PERIPH;
    handle->txDMAHandler.Init.PeriphInc = DMA_PINC_DISABLE;
    handle->txDMAHandler.Init.MemInc = DMA_MINC_ENABLE;
    handle->txDMAHandler.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    handle->txDMAHandler.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    handle->txDMAHandler.Init.Mode = DMA_NORMAL;
    handle->txDMAHandler.Init.Priority = DMA_PRIORITY_HIGH;
    handle->txDMAHandler.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if(HAL_DMA_Init(&handle->txDMAHandler) != HAL_OK)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    handle->triggerDMAHandler.Instance = DMA2_Stream3;
    handle->triggerDMAHandler.Init.Request = DMA_REQUEST_TIM7_UP;
    handle->triggerDMAHandler.Init.Direction = DMA_MEMORY_TO_PERIPH;
    handle->triggerDMAHandler.Init.PeriphInc = DMA_PINC_DISABLE;
    handle->triggerDMAHandler.Init.MemInc = DMA_MINC_DISABLE;
    handle->triggerDMAHandler.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    handle->triggerDMAHandler.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    handle->triggerDMAHandler.Init.Mode = DMA_CIRCULAR;
    handle->triggerDMAHandler.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    handle->triggerDMAHandler.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if(HAL_DMA_Init(&handle->triggerDMAHandler) != HAL_OK)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    __HAL_LINKDMA(&handle->deviceHandler, hdmatx, handle->txDMAHandler);

    HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);

    return DRV_I2C_STATUS_OK;
}


__weak void DRV_I2C_TriggeredDMACompleteCallback(drv_i2c_instance_t instance)
{
    (void)instance;
}

static void prvDRV_I2C_TriggeredTXComplete(DMA_HandleTypeDef* hdma)
{
    drv_i2c_handle_t* handle = &prvDRV_I2C_INSTANCES[DRV_I2C_INSTANCE_2];

    if(hdma != &handle->txDMAHandler)
    {
        return;
    }

    CLEAR_BIT(handle->deviceHandler.Instance->CR1, I2C_CR1_TXDMAEN);
    (void)HAL_DMA_Abort(&handle->triggerDMAHandler);

    DRV_I2C_TriggeredDMACompleteCallback(DRV_I2C_INSTANCE_2);
}

void DMA2_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&prvDRV_I2C_INSTANCES[DRV_I2C_INSTANCE_2].txDMAHandler);
}

void I2C2_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&prvDRV_I2C_INSTANCES[DRV_I2C_INSTANCE_2].deviceHandler);
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
	handle->instance = instance;

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
		return DRV_I2C_STATUS_ERROR; //
	}

	handle->deviceHandler.Instance = peripheral;

	/* Default timing value kept from original driver */
	if(handle->deviceHandler.Instance == I2C2)
	{
		handle->deviceHandler.Init.Timing           = 0x10C0ECFF;
	}
	else
	{
		handle->deviceHandler.Init.Timing           = 0x10C0ECFF;
	}
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

	if(prvDRV_I2C_DMAInit(handle) != DRV_I2C_STATUS_OK)
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

drv_i2c_status_t DRV_I2C_Transmit(drv_i2c_instance_t instance,
                                  uint8_t addr,
                                  uint8_t* data,
                                  uint32_t size,
                                  uint32_t timeout)
{
    drv_i2c_handle_t* handle = NULL;
    drv_i2c_status_t status = DRV_I2C_STATUS_OK;

    if((prvDRV_I2C_IsValidInstance(instance) == 0U) || (data == NULL))
    {
        return DRV_I2C_STATUS_ERROR;
    }

    handle = &prvDRV_I2C_INSTANCES[instance];

    if((handle->initState != DRV_I2C_INITIALIZATION_STATUS_INIT) ||
       (handle->lock == NULL))
    {
        return DRV_I2C_STATUS_ERROR;
    }

    if(__get_IPSR() != 0U)
    {
        BaseType_t higherPriorityTaskWoken = pdFALSE;

        /*
         * ISR context.
         *
         * A semaphore cannot block from an ISR. If the I2C peripheral
         * is already in use, return an error immediately.
         */
        if(xSemaphoreTakeFromISR(handle->lock,
                                 &higherPriorityTaskWoken) != pdTRUE)
        {
            return DRV_I2C_STATUS_ERROR;
        }

        if(HAL_I2C_Master_Transmit(&handle->deviceHandler,
                                  addr,
                                  data,
                                  size,
                                  timeout) != HAL_OK)
        {
            status = DRV_I2C_STATUS_ERROR;
        }

        if(xSemaphoreGiveFromISR(handle->lock,
                                 &higherPriorityTaskWoken) != pdTRUE)
        {
            status = DRV_I2C_STATUS_ERROR;
        }

        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }
    else
    {
        /*
         * Task context.
         */
        if(xSemaphoreTake(handle->lock,
                          pdMS_TO_TICKS(timeout)) != pdTRUE)
        {
            return DRV_I2C_STATUS_ERROR;
        }

        if(HAL_I2C_Master_Transmit(&handle->deviceHandler,
                                  addr,
                                  data,
                                  size,
                                  timeout) != HAL_OK)
        {
            status = DRV_I2C_STATUS_ERROR;
        }

        if(xSemaphoreGive(handle->lock) != pdTRUE)
        {
            status = DRV_I2C_STATUS_ERROR;
        }
    }

    return status;
}
__weak void DRV_I2C_TransmitDMACompleteCallback(drv_i2c_instance_t instance)
{

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
drv_i2c_status_t DRV_I2C_TransmitDMA(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size)
{
    drv_i2c_handle_t* handle = NULL;

    if((prvDRV_I2C_IsValidInstance(instance) == 0U) || (data == NULL) || (size == 0U) || (size > UINT16_MAX))
    {
        return DRV_I2C_STATUS_ERROR;
    }

    if(instance != DRV_I2C_INSTANCE_2)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    handle = &prvDRV_I2C_INSTANCES[instance];

    if(handle->initState != DRV_I2C_INITIALIZATION_STATUS_INIT)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    if(HAL_I2C_GetState(&handle->deviceHandler) != HAL_I2C_STATE_READY)
    {
        return DRV_I2C_STATUS_ERROR;
    }
    handle->txDMAAddress = addr;

    if(HAL_I2C_Master_Transmit_DMA(&handle->deviceHandler, addr, data, (uint16_t)size) != HAL_OK)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    return DRV_I2C_STATUS_OK;
}



drv_i2c_status_t DRV_I2C_TransmitTriggeredDMA(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size, uint8_t transferSize)
{
    drv_i2c_handle_t* handle = NULL;

    if((prvDRV_I2C_IsValidInstance(instance) == 0U) || (data == NULL) || (size == 0U) || (transferSize == 0U) || ((size % transferSize) != 0U))
    {
        return DRV_I2C_STATUS_ERROR;
    }

    if(instance != DRV_I2C_INSTANCE_2)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    handle = &prvDRV_I2C_INSTANCES[instance];

    if(handle->initState != DRV_I2C_INITIALIZATION_STATUS_INIT)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    if((handle->txDMAHandler.State != HAL_DMA_STATE_READY) || (handle->triggerDMAHandler.State != HAL_DMA_STATE_READY))
    {
        return DRV_I2C_STATUS_ERROR;
    }

    if((handle->deviceHandler.Instance->ISR & I2C_ISR_BUSY) != 0U)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    handle->txDMAAddress = addr;

    handle->triggerCR2 = ((uint32_t)addr & I2C_CR2_SADD) |
                         ((uint32_t)transferSize << I2C_CR2_NBYTES_Pos) |
                         I2C_CR2_AUTOEND |
                         I2C_CR2_START;

    CLEAR_BIT(handle->deviceHandler.Instance->CR1, I2C_CR1_TXDMAEN);
    WRITE_REG(handle->deviceHandler.Instance->ICR, I2C_ICR_STOPCF | I2C_ICR_NACKCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF);
    MODIFY_REG(handle->deviceHandler.Instance->CR2,
               I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RELOAD | I2C_CR2_AUTOEND | I2C_CR2_RD_WRN | I2C_CR2_START | I2C_CR2_STOP,
               0U);

    handle->txDMAHandler.XferCpltCallback = prvDRV_I2C_TriggeredTXComplete;

    if(HAL_DMA_Start_IT(&handle->txDMAHandler, (uint32_t)data, (uint32_t)&handle->deviceHandler.Instance->TXDR, size) != HAL_OK)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    SET_BIT(handle->deviceHandler.Instance->CR1, I2C_CR1_TXDMAEN);

    if(HAL_DMA_Start(&handle->triggerDMAHandler, (uint32_t)&handle->triggerCR2, (uint32_t)&handle->deviceHandler.Instance->CR2, 1U) != HAL_OK)
    {
        CLEAR_BIT(handle->deviceHandler.Instance->CR1, I2C_CR1_TXDMAEN);
        (void)HAL_DMA_Abort(&handle->txDMAHandler);
        return DRV_I2C_STATUS_ERROR;
    }

    return DRV_I2C_STATUS_OK;
}

drv_i2c_status_t DRV_I2C_WaitIdle(drv_i2c_instance_t instance, uint32_t timeout)
{
    drv_i2c_handle_t* handle = NULL;
    uint32_t tickStart;

    if(prvDRV_I2C_IsValidInstance(instance) == 0U)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    handle = &prvDRV_I2C_INSTANCES[instance];

    if(handle->initState != DRV_I2C_INITIALIZATION_STATUS_INIT)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    tickStart = HAL_GetTick();

    while((HAL_I2C_GetState(&handle->deviceHandler) != HAL_I2C_STATE_READY) ||
          ((handle->deviceHandler.Instance->ISR & I2C_ISR_BUSY) != 0U))
    {
        if((HAL_GetTick() - tickStart) >= timeout)
        {
            return DRV_I2C_STATUS_BUSY;
        }
    }

    return DRV_I2C_STATUS_OK;
}

drv_i2c_status_t DRV_I2C_AbortDMA(drv_i2c_instance_t instance)
{
    drv_i2c_handle_t* handle = NULL;

    if(prvDRV_I2C_IsValidInstance(instance) == 0U)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    if(instance != DRV_I2C_INSTANCE_2)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    handle = &prvDRV_I2C_INSTANCES[instance];

    if(handle->initState != DRV_I2C_INITIALIZATION_STATUS_INIT)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    CLEAR_BIT(handle->deviceHandler.Instance->CR1, I2C_CR1_TXDMAEN);

    if(handle->triggerDMAHandler.State != HAL_DMA_STATE_READY)
    {
        (void)HAL_DMA_Abort(&handle->triggerDMAHandler);
    }

    if(handle->txDMAHandler.State != HAL_DMA_STATE_READY)
    {
        (void)HAL_DMA_Abort(&handle->txDMAHandler);
    }

    if((handle->deviceHandler.Instance->ISR & I2C_ISR_BUSY) != 0U)
    {
        SET_BIT(handle->deviceHandler.Instance->CR2, I2C_CR2_STOP);
    }

    WRITE_REG(handle->deviceHandler.Instance->ICR, I2C_ICR_STOPCF | I2C_ICR_NACKCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF);

    return DRV_I2C_STATUS_OK;
}

drv_i2c_status_t DRV_I2C_RegisterTxDMACompleteCallback(drv_i2c_instance_t instance, drv_i2c_tx_dma_complete_callback_t callback)
{
    if(prvDRV_I2C_IsValidInstance(instance) == 0U)
    {
        return DRV_I2C_STATUS_ERROR;
    }

    prvDRV_I2C_INSTANCES[instance].txDMACompleteCallback = callback;

    return DRV_I2C_STATUS_OK;
}
/**
 * @}
 */
/**
 * @}
 */
