/*
 * drv_spi.c
 *
 *  Created on: Jun 30, 2024
 *      Author: Pavle Lakic & Dimitrije Lilic
 */

#include "main.h"
#include "drv_spi.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include <string.h>

typedef struct drv_spi_handle_t
{
	drv_spi_instance_t					instance;
	drv_spi_initialization_status_t		initState;
	drv_spi_config_t					config;
	SemaphoreHandle_t					lock;
	SPI_HandleTypeDef 					deviceHandler;
}drv_spi_handle_t;

static drv_spi_handle_t prvDRV_SPI_INSTANCE;








/**
  * @brief  Initialize the SPI MSP.
  * @param  hspi: pointer to a SPI_HandleTypeDef structure that contains
  *               the configuration information for SPI module.
  * @retval None
  */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

	if(hspi->Instance==SPI3)
	{

		PeriphClkInitStruct.PeriphClockSelection =  RCC_PERIPHCLK_SPI3;
		PeriphClkInitStruct.PLL2.PLL2M = 4;
		PeriphClkInitStruct.PLL2.PLL2N = 10;
		PeriphClkInitStruct.PLL2.PLL2P = 2;
		PeriphClkInitStruct.PLL2.PLL2Q = 2;
		PeriphClkInitStruct.PLL2.PLL2R = 2;
		PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
		PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
		PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
		PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL2;

		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
		{
		  Error_Handler();
		}

		__HAL_RCC_SPI3_CLK_ENABLE();

		__HAL_RCC_GPIOB_CLK_ENABLE();
		__HAL_RCC_GPIOC_CLK_ENABLE();

		/**SPI3 GPIO Configuration
		PB2     ------> SPI3_MOSI
		PC10	------> SPI3_SCK
		PC11	------> SPI3_MISO
		*/

		GPIO_InitStruct.Pin = GPIO_PIN_2;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_PULLDOWN;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF7_SPI3;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	}
    if(hspi->Instance==SPI4)
    {
    /* USER CODE BEGIN SPI4_MspInit 0 */

    /* USER CODE END SPI4_MspInit 0 */

    /** Initializes the peripherals clock
    */
      PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI4;
      PeriphClkInitStruct.Spi45ClockSelection = RCC_SPI45CLKSOURCE_D2PCLK1;
      if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
      {
        Error_Handler();
      }

      /* Peripheral clock enable */
      __HAL_RCC_SPI4_CLK_ENABLE();

      __HAL_RCC_GPIOE_CLK_ENABLE();
      /**SPI4 GPIO Configuration
      PE2     ------> SPI4_SCK
      PE4     ------> SPI4_NSS
      PE6     ------> SPI4_MOSI
      */
      GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_4|GPIO_PIN_6;
      GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
      GPIO_InitStruct.Pull = GPIO_NOPULL;
      GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
      GPIO_InitStruct.Alternate = GPIO_AF5_SPI4;
      HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);


      /* SPI4 interrupt Init */
//      HAL_NVIC_SetPriority(SPI4_IRQn, 5, 0);
//      HAL_NVIC_EnableIRQ(SPI4_IRQn);

    /* USER CODE BEGIN SPI4_MspInit 1 */

    /* USER CODE END SPI4_MspInit 1 */
    }
    else if(hspi->Instance==SPI5)
    {
    /* USER CODE BEGIN SPI5_MspInit 0 */

    /* USER CODE END SPI5_MspInit 0 */

    /** Initializes the peripherals clock
    */
      PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI5;
      PeriphClkInitStruct.Spi45ClockSelection = RCC_SPI45CLKSOURCE_D2PCLK1;
      if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
      {
        Error_Handler();
      }

      /* Peripheral clock enable */
      __HAL_RCC_SPI5_CLK_ENABLE();

      __HAL_RCC_GPIOF_CLK_ENABLE();

      /* DMA interrupt init */
      /**SPI5 GPIO Configuration
      PF6     ------> SPI5_NSS
      PF7     ------> SPI5_SCK
      PF9     ------> SPI5_MOSI
      */
      GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_9;
      GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
      GPIO_InitStruct.Pull = GPIO_NOPULL;
      GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
      GPIO_InitStruct.Alternate = GPIO_AF5_SPI5;
      HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

      /* SPI5 interrupt Init */
//      HAL_NVIC_SetPriority(SPI5_IRQn, 5, 0);
//      HAL_NVIC_EnableIRQ(SPI5_IRQn);
    }
}

/**
  * @brief  De-Initialize the SPI MSP.
  * @param  hspi: pointer to a SPI_HandleTypeDef structure that contains
  *               the configuration information for SPI module.
  * @retval None
  */
void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)
{
	if(hspi->Instance==SPI3)
	{
		/* Peripheral clock disable */
		__HAL_RCC_SPI3_CLK_DISABLE();

		/**SPI3 GPIO Configuration
		PB2     ------> SPI3_MOSI
		PC10     ------> SPI3_SCK
		PC11     ------> SPI3_MISO
		*/
		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_2);

		HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10|GPIO_PIN_11);


	}
	else if(hspi->Instance==SPI4)
	{
		/* Peripheral clock disable */
		__HAL_RCC_SPI4_CLK_DISABLE();

		/**SPI4 GPIO Configuration
		PE2     ------> SPI4_SCK
		PE4     ------> SPI4_NSS
		PE6     ------> SPI4_MOSI
		*/
		HAL_GPIO_DeInit(GPIOE, GPIO_PIN_2|GPIO_PIN_4|GPIO_PIN_6);

		/* SPI4 interrupt DeInit */
		HAL_NVIC_DisableIRQ(SPI4_IRQn);
	}
	else if(hspi->Instance==SPI5)
	{
		/* Peripheral clock disable */
		__HAL_RCC_SPI5_CLK_DISABLE();

		/**SPI5 GPIO Configuration
		PF6     ------> SPI5_NSS
		PF7     ------> SPI5_SCK
		PF9     ------> SPI5_MOSI
		*/
		HAL_GPIO_DeInit(GPIOF, GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_9);

		/* SPI5 interrupt DeInit */
		HAL_NVIC_DisableIRQ(SPI5_IRQn);
	}
}







drv_spi_status_t	DRV_SPI_Init()
{
	memset(&prvDRV_SPI_INSTANCE, 0, sizeof(drv_spi_handle_t));
	return	DRV_SPI_STATUS_OK;
}

drv_spi_status_t	DRV_SPI_Instance_Init(drv_spi_instance_t instance, drv_spi_config_t* config)
{
	if(prvDRV_SPI_INSTANCE.lock != NULL) return DRV_SPI_STATUS_ERROR;

	prvDRV_SPI_INSTANCE.lock = xSemaphoreCreateMutex();

	if(prvDRV_SPI_INSTANCE.lock == NULL) return DRV_SPI_STATUS_ERROR;

	/*TODO Check what user can actually configure. */
	prvDRV_SPI_INSTANCE.deviceHandler.Instance = SPI3;
	switch(config->mode)
	{
	case DRV_SPI_MODE_MASTER:
		prvDRV_SPI_INSTANCE.deviceHandler.Init.Mode = SPI_MODE_MASTER;
		break;
	case DRV_SPI_MODE_SLAVE:
		prvDRV_SPI_INSTANCE.deviceHandler.Init.Mode = SPI_MODE_SLAVE;
		break;
	}
	prvDRV_SPI_INSTANCE.deviceHandler.Init.Direction = SPI_DIRECTION_2LINES;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.DataSize = SPI_DATASIZE_8BIT;
	switch(config->polarity)
	{
	case DRV_SPI_POLARITY_HIGH:
		prvDRV_SPI_INSTANCE.deviceHandler.Init.CLKPolarity = SPI_POLARITY_HIGH;
		break;
	case DRV_SPI_POLARITY_LOW:
		prvDRV_SPI_INSTANCE.deviceHandler.Init.CLKPolarity = SPI_POLARITY_LOW;
		break;
	}

	switch(config->phase)
	{
	case DRV_SPI_PHASE_1EDGE:
		prvDRV_SPI_INSTANCE.deviceHandler.Init.CLKPhase = SPI_PHASE_1EDGE;
		break;
	case DRV_SPI_PHASE_2EDGE:
		prvDRV_SPI_INSTANCE.deviceHandler.Init.CLKPhase = SPI_PHASE_2EDGE;
		break;
	}
	prvDRV_SPI_INSTANCE.deviceHandler.Init.NSS = SPI_NSS_SOFT;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.FirstBit = SPI_FIRSTBIT_MSB;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.TIMode = SPI_TIMODE_DISABLE;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.CRCPolynomial = 0x0;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
	prvDRV_SPI_INSTANCE.deviceHandler.Init.IOSwap = SPI_IO_SWAP_DISABLE;

	if (HAL_SPI_Init(&prvDRV_SPI_INSTANCE.deviceHandler) != HAL_OK) return DRV_SPI_STATUS_ERROR;

	prvDRV_SPI_INSTANCE.initState = DRV_SPI_INITIALIZATION_STATUS_INIT;

	return	DRV_SPI_STATUS_OK;
}

drv_spi_status_t	DRV_SPI_Instance_DeInit(drv_spi_instance_t instance)
{
	if(prvDRV_SPI_INSTANCE.lock == NULL) return DRV_SPI_STATUS_ERROR;

	/*TODO Check what user can actually configure. */
	prvDRV_SPI_INSTANCE.deviceHandler.Instance = SPI3;

	if (HAL_SPI_DeInit(&prvDRV_SPI_INSTANCE.deviceHandler) != HAL_OK) return DRV_SPI_STATUS_ERROR;

	prvDRV_SPI_INSTANCE.initState = DRV_SPI_INITIALIZATION_STATUS_NOINIT;

	return	DRV_SPI_STATUS_OK;
}
/*TODO check with another MCU for Transmit and Receive */
drv_spi_status_t	DRV_SPI_TransmitData(uint8_t* buffer, uint8_t size, uint32_t timeout)
{
	if(prvDRV_SPI_INSTANCE.initState != DRV_SPI_INITIALIZATION_STATUS_INIT || prvDRV_SPI_INSTANCE.lock == NULL) return DRV_SPI_STATUS_ERROR;

	if(xSemaphoreTake(prvDRV_SPI_INSTANCE.lock, pdMS_TO_TICKS(timeout)) != pdTRUE) return DRV_SPI_STATUS_ERROR;

	//transmit function here.
	if(HAL_SPI_Transmit(&prvDRV_SPI_INSTANCE.deviceHandler, buffer, size, timeout) != HAL_OK) return DRV_SPI_STATUS_ERROR;

	if(xSemaphoreGive(prvDRV_SPI_INSTANCE.lock) != pdTRUE) return DRV_SPI_STATUS_ERROR;

	return	DRV_SPI_STATUS_OK;
}

drv_spi_status_t	DRV_SPI_ReceiveData(uint8_t* buffer, uint8_t size, uint32_t timeout)
{
	if(prvDRV_SPI_INSTANCE.initState != DRV_SPI_INITIALIZATION_STATUS_INIT || prvDRV_SPI_INSTANCE.lock == NULL) return DRV_SPI_STATUS_ERROR;

	if(xSemaphoreTake(prvDRV_SPI_INSTANCE.lock, pdMS_TO_TICKS(timeout)) != pdTRUE) return DRV_SPI_STATUS_ERROR;

	//receive function here.
	if(HAL_SPI_Receive(&prvDRV_SPI_INSTANCE.deviceHandler, buffer, size, timeout) != HAL_OK)  return DRV_SPI_STATUS_ERROR;

	if(xSemaphoreGive(prvDRV_SPI_INSTANCE.lock) != pdTRUE) return DRV_SPI_STATUS_ERROR;

	return	DRV_SPI_STATUS_OK;
}

