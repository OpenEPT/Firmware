/**
 ******************************************************************************
 * @file    drv_spi.c
 *
 * @brief   SPI driver implementation This file contains
 *          the implementation of the SPI driver for STM32H7 microcontrollers.
 *          It provides functionality for SPI communication in both master and 
 *          slave modes withDMA and interrupt support.
 *
 * @author  Pavle Lakic & Dimitrije Lilic
 * @date    Jun 30, 2024
 ******************************************************************************
 */



#include "main.h"
#include "drv_spi.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include <string.h>

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup SPI_DRIVER SPI Driver
 * @{
 */

 /**
 * @defgroup SPI_PRIVATE_STRUCTURES SPI driver private structures
 * @{
 */

/**
 * @brief   SPI driver handle structure
 * @details Contains all data needed to manage a SPI instance
 */
typedef struct drv_spi_handle_t
{
	drv_spi_instance_t					instance;   /**< SPI instance identifier */
	drv_spi_initialization_status_t		initState;  /**< Initialization state of the SPI instance */
	drv_spi_config_t					config;     /**< Configuration parameters for the SPI instance */
	SemaphoreHandle_t					lock;       /**< Mutex for thread-safe access to the SPI instance */
	SPI_HandleTypeDef 					deviceHandler; /**< HAL SPI handle */
}drv_spi_handle_t;

/**
 * @}
 */

/**
 * @defgroup SPI_PRIVATE_DATA SPI driver private data
 * @{
 */

/** @brief Array of SPI driver handles, one for each SPI instance */
static drv_spi_handle_t 		prvDRV_SPI_INSTANCES[DRV_SPI_INSTANCES_MAX_NUMBER];

/** @brief Array of SPI receive callback functions, one for each SPI instance */
static drv_spi_rx_isr_callback 	prvDRV_SPI_CALLBACKS[DRV_SPI_INSTANCES_MAX_NUMBER];

/** @brief DMA handle for SPI2 receive */
DMA_HandleTypeDef hdma_spi2_rx;

/** @brief DMA handle for SPI2 transmit */
DMA_HandleTypeDef hdma_spi2_tx;
/**
 * @}
 */

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if(hspi->Instance == SPI2) {
		if(prvDRV_SPI_CALLBACKS[DRV_SPI_INSTANCE2] != NULL) {
			prvDRV_SPI_CALLBACKS[DRV_SPI_INSTANCE2](0);
		}
	 }
}

// Make sure you have the IRQ handler in your main.c or stm32h7xx_it.c:
void SPI2_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&prvDRV_SPI_INSTANCES[DRV_SPI_INSTANCE2].deviceHandler);
}

void DMA1_Stream4_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_spi2_tx);
}

void DMA1_Stream5_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_spi2_rx);
}

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
		GPIO_InitStruct.Pull = GPIO_PULLDOWN;
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
    if(hspi->Instance==SPI2)
	{
        /* USER CODE BEGIN SPI2_MspInit 0 */

        /* USER CODE END SPI2_MspInit 0 */


		PeriphClkInitStruct.PeriphClockSelection =  RCC_PERIPHCLK_SPI2;
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


		__HAL_RCC_SPI2_CLK_ENABLE();

		__HAL_RCC_GPIOC_CLK_ENABLE();
		__HAL_RCC_GPIOB_CLK_ENABLE();
		/**SPI2 GPIO Configuration
		PC2_C     ------> SPI2_MISO
		PB10     ------> SPI2_SCK
		PB12     ------> SPI2_NSS
		PB15     ------> SPI2_MOSI
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_2;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_12|GPIO_PIN_15;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		/* SPI2 DMA Init */
		/* SPI2_RX Init */
		hdma_spi2_rx.Instance = DMA1_Stream5;
		hdma_spi2_rx.Init.Request = DMA_REQUEST_SPI2_RX;
		hdma_spi2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
		hdma_spi2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
		hdma_spi2_rx.Init.MemInc = DMA_MINC_ENABLE;
		hdma_spi2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_spi2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_spi2_rx.Init.Mode = DMA_CIRCULAR;
		hdma_spi2_rx.Init.Priority = DMA_PRIORITY_HIGH;
		hdma_spi2_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
		if (HAL_DMA_Init(&hdma_spi2_rx) != HAL_OK)
		{
		  Error_Handler();
		}

		__HAL_LINKDMA(hspi,hdmarx,hdma_spi2_rx);

		/* SPI2_TX Init */
		hdma_spi2_tx.Instance = DMA1_Stream4;
		hdma_spi2_tx.Init.Request = DMA_REQUEST_SPI2_TX;
		hdma_spi2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
		hdma_spi2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
		hdma_spi2_tx.Init.MemInc = DMA_MINC_ENABLE;
		hdma_spi2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_spi2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_spi2_tx.Init.Mode = DMA_CIRCULAR;
		hdma_spi2_tx.Init.Priority = DMA_PRIORITY_HIGH;
		hdma_spi2_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
		if (HAL_DMA_Init(&hdma_spi2_tx) != HAL_OK)
		{
		  Error_Handler();
		}

		__HAL_LINKDMA(hspi,hdmatx,hdma_spi2_tx);

	    /* NVIC configuration for DMA transfer complete interrupt (SPI1_TX) */
	    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 1);
	    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

	    /* NVIC configuration for DMA transfer complete interrupt (SPI1_RX) */
	    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 5, 0);
	    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);

//		/* SPI2 interrupt Init */
//		HAL_NVIC_SetPriority(SPI2_IRQn, 5, 0);
//		HAL_NVIC_EnableIRQ(SPI2_IRQn);
	}
}

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
	else if(hspi->Instance==SPI2)
	{
		/* Peripheral clock disable */
		__HAL_RCC_SPI2_CLK_DISABLE();

		/**SPI2 GPIO Configuration
		PC2_C    ------> SPI2_MISO
		PB10     ------> SPI2_SCK
		PB12     ------> SPI2_NSS
		PB15     ------> SPI2_MOSI
		*/
		HAL_GPIO_DeInit(GPIOC, GPIO_PIN_2);

		HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10|GPIO_PIN_12|GPIO_PIN_15);

		/* SPI2 interrupt DeInit */
		HAL_NVIC_DisableIRQ(SPI2_IRQn);
	}
}
drv_spi_status_t	DRV_SPI_Init()
{
	memset(prvDRV_SPI_INSTANCES, 0, CONF_SPI_INSTANCES_MAX_NUMBER*sizeof(drv_spi_handle_t));
	return	DRV_SPI_STATUS_OK;
}

drv_spi_status_t	DRV_SPI_Instance_Init(drv_spi_instance_t instance, drv_spi_config_t* config)
{
	if(prvDRV_SPI_INSTANCES[instance].lock == NULL){

		prvDRV_SPI_INSTANCES[instance].lock = xSemaphoreCreateMutex();

		if(prvDRV_SPI_INSTANCES[instance].lock == NULL) return DRV_SPI_STATUS_ERROR;
	}


	/*TODO Check what user can actually configure. */
	switch(instance)
		{
		case DRV_SPI_INSTANCE3:
			prvDRV_SPI_INSTANCES[instance].deviceHandler.Instance = SPI3;
			break;
		case DRV_SPI_INSTANCE2:
			prvDRV_SPI_INSTANCES[instance].deviceHandler.Instance = SPI2;
			break;
		}
	//prvDRV_SPI_INSTANCES.deviceHandler.Instance = SPI3;
	switch(config->mode)
	{
	case DRV_SPI_MODE_MASTER:
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.Mode = SPI_MODE_MASTER;
		break;
	case DRV_SPI_MODE_SLAVE:
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.Mode = SPI_MODE_SLAVE;
		break;
	}
	switch(config->polarity)
	{
	case DRV_SPI_POLARITY_HIGH:
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.CLKPolarity = SPI_POLARITY_HIGH;
		break;
	case DRV_SPI_POLARITY_LOW:
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.CLKPolarity = SPI_POLARITY_LOW;
		break;
	}

	switch(config->phase)
	{
	case DRV_SPI_PHASE_1EDGE:
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.CLKPhase = SPI_PHASE_1EDGE;
		break;
	case DRV_SPI_PHASE_2EDGE:
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.CLKPhase = SPI_PHASE_2EDGE;
		break;
	}
	if(config->mode == DRV_SPI_MODE_MASTER)
	{
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.Direction = SPI_DIRECTION_2LINES;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.DataSize = SPI_DATASIZE_8BIT;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.NSS = SPI_NSS_SOFT;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.FirstBit = SPI_FIRSTBIT_MSB;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.TIMode = SPI_TIMODE_DISABLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.CRCPolynomial = 0x0;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.IOSwap = SPI_IO_SWAP_DISABLE;
		if (HAL_SPI_Init(&prvDRV_SPI_INSTANCES[instance].deviceHandler) != HAL_OK) return DRV_SPI_STATUS_ERROR;
	}
	else
	{
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.Direction = SPI_DIRECTION_2LINES;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.DataSize = SPI_DATASIZE_8BIT;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.NSS = SPI_NSS_HARD_INPUT;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.FirstBit = SPI_FIRSTBIT_MSB;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.TIMode = SPI_TIMODE_DISABLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.CRCPolynomial = 0x0;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
		prvDRV_SPI_INSTANCES[instance].deviceHandler.Init.IOSwap = SPI_IO_SWAP_DISABLE;
		if (HAL_SPI_Init(&prvDRV_SPI_INSTANCES[instance].deviceHandler) != HAL_OK) return DRV_SPI_STATUS_ERROR;
	}


	prvDRV_SPI_INSTANCES[instance].initState = DRV_SPI_INITIALIZATION_STATUS_INIT;

	return	DRV_SPI_STATUS_OK;
}

drv_spi_status_t	DRV_SPI_Instance_DeInit(drv_spi_instance_t instance)
{
	if(prvDRV_SPI_INSTANCES[instance].lock == NULL) return DRV_SPI_STATUS_ERROR;

	/*TODO Check what user can actually configure. */
	switch(instance)
			{
			case DRV_SPI_INSTANCE3:
				prvDRV_SPI_INSTANCES[instance].deviceHandler.Instance = SPI3;
				break;
			case DRV_SPI_INSTANCE2:
				prvDRV_SPI_INSTANCES[instance].deviceHandler.Instance = SPI2;
				break;
			}

	if (HAL_SPI_DeInit(&prvDRV_SPI_INSTANCES[instance].deviceHandler) != HAL_OK) return DRV_SPI_STATUS_ERROR;

	prvDRV_SPI_INSTANCES[instance].initState = DRV_SPI_INITIALIZATION_STATUS_NOINIT;

	return	DRV_SPI_STATUS_OK;
}
/*TODO check with another MCU for Transmit and Receive */
drv_spi_status_t	DRV_SPI_TransmitData(drv_spi_instance_t instance, uint8_t* buffer, uint8_t size, uint32_t timeout)
{
	if(prvDRV_SPI_INSTANCES[instance].initState != DRV_SPI_INITIALIZATION_STATUS_INIT || prvDRV_SPI_INSTANCES[instance].lock == NULL) return DRV_SPI_STATUS_ERROR;

	if(xSemaphoreTake(prvDRV_SPI_INSTANCES[instance].lock, pdMS_TO_TICKS(timeout)) != pdTRUE) return DRV_SPI_STATUS_ERROR;

	//transmit function here.
	if(HAL_SPI_Transmit(&prvDRV_SPI_INSTANCES[instance].deviceHandler, buffer, size, timeout) != HAL_OK) return DRV_SPI_STATUS_ERROR;

	if(xSemaphoreGive(prvDRV_SPI_INSTANCES[instance].lock) != pdTRUE) return DRV_SPI_STATUS_ERROR;

	return	DRV_SPI_STATUS_OK;
}

drv_spi_status_t	DRV_SPI_ReceiveData(drv_spi_instance_t instance, uint8_t* buffer, uint8_t size, uint32_t timeout)
{
	if(prvDRV_SPI_INSTANCES[instance].initState != DRV_SPI_INITIALIZATION_STATUS_INIT || prvDRV_SPI_INSTANCES[instance].lock == NULL) return DRV_SPI_STATUS_ERROR;

	if(xSemaphoreTake(prvDRV_SPI_INSTANCES[instance].lock, pdMS_TO_TICKS(timeout)) != pdTRUE) return DRV_SPI_STATUS_ERROR;

	//receive function here.
	if(HAL_SPI_Receive(&prvDRV_SPI_INSTANCES[instance].deviceHandler, buffer, size, timeout) != HAL_OK)  return DRV_SPI_STATUS_ERROR;

	if(xSemaphoreGive(prvDRV_SPI_INSTANCES[instance].lock) != pdTRUE) return DRV_SPI_STATUS_ERROR;

	return	DRV_SPI_STATUS_OK;
}
drv_spi_status_t	DRV_SPI_EnableITData(drv_spi_instance_t instance, uint8_t* input, uint8_t* output, uint32_t size)
{
	if(prvDRV_SPI_INSTANCES[instance].initState != DRV_SPI_INITIALIZATION_STATUS_INIT || prvDRV_SPI_INSTANCES[instance].lock == NULL) return DRV_SPI_STATUS_ERROR;

	//if(HAL_SPI_Receive_IT(&prvDRV_SPI_INSTANCES[instance].deviceHandler, dataone, 3) != HAL_OK)  return DRV_SPI_STATUS_ERROR;
	//if(HAL_SPI_TransmitReceive_IT(&prvDRV_SPI_INSTANCES[DRV_SPI_INSTANCE2].deviceHandler, output , input, size) != HAL_OK)  return DRV_SPI_STATUS_ERROR;

	if(HAL_SPI_TransmitReceive_DMA(&prvDRV_SPI_INSTANCES[instance].deviceHandler, output, input, size) != HAL_OK)  return DRV_SPI_STATUS_ERROR;



	return	DRV_SPI_STATUS_OK;
}
drv_spi_status_t	DRV_SPI_Instance_RegisterRxCallback(drv_spi_instance_t instance, drv_spi_rx_isr_callback rxcb)
{
	prvDRV_SPI_CALLBACKS[instance] = rxcb;

	return	DRV_SPI_STATUS_OK;

}

/**
 * @}
 */
/**
 * @}
 */

