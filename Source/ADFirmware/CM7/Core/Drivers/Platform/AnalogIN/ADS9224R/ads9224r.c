/**
 ******************************************************************************
 * @file    ads9224r.c
 *
 * @brief   ADS9224R Dual-Channel, 24-bit, 155-kSPS ADC driver implementation
 *          providing hardware abstraction layer for external ADS9224R ADC. 
 *          This driver handles SPI/parallel communication protocols, dual
 *          SDO channels, configurable sampling rates, power management,
 *          data averaging, and DMA-based streaming with callback mechanisms
 *          for high-speed analog data acquisition from dual simultaneous
 *          sampling channels.
 *
 * @author  Haris Turkmanovic
 * @email   haris.turkmanovic@gmail.com
 * @date    October 2024
 ******************************************************************************
 */

#include "../../SPI/drv_spi.h"
#include "../../GPIO/drv_gpio.h"
#include "ads9224r.h"
#include "stm32h7xx_hal_conf.h"
#include "stm32h7xx_ll_dma.h"

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup ADS9224R_DRIVER ADS9224R External ADC Driver
 * @{
 */

/**
 * @defgroup ADS9224R_PRIVATE_STRUCTURES ADS9224R driver private structures
 * @{
 */
/** * @brief ADS9224R driver internal handle structure
 */
typedef struct
{
	ads9224r_init_state_t 	init;         /**< Initialization state */
	ads9224r_config_t		*config;      /**< Configuration parameters */
	ads9224r_acq_state_t    acqState;     /**< Acquisition state */
	bufferReceiveCallback	callback;     /**< Buffer receive callback function */
	ads9224r_sdo_t 			trigger;      /**< SDO channel trigger selection */
	ads9224r_op_state_t     opState;      /**< Operational state */
	uint32_t				timPrescaller; /**< Timer prescaler value */
	uint32_t				timPeriod;     /**< Timer period value */
	uint32_t				timPulse;      /**< Timer pulse width */
	uint32_t				samplingPeriod; /**< Calculated sampling period in microseconds */
}ads9224r_handle_t;

/**
 * @}
 */

/**
 * @defgroup ADS9224R_PRIVATE_DATA ADS9224R driver private data
 * @{
 */

/** @brief Timer handle for SPI clock generation */
static TIM_HandleTypeDef 	prvADS9224R_TIMER_SCLK_HANDLER;

/** @brief Timer handle for chip select signal generation */
static TIM_HandleTypeDef 	prvADS9224R_TIMER_CS_HANDLER;

/** @brief Timer handle for conversion start signal generation */
static TIM_HandleTypeDef 	prvADS9224R_TIMER_CONVST_HANDLER;

/** @brief Internal driver data structure instance */
static ads9224r_handle_t	prvADS9224R_DATA;

/** @brief SPI handle for SDO-B channel communication */
static SPI_HandleTypeDef 	prvADS9224R_SPI_S_SDOB_HANDLER;

/** @brief SPI handle for SDO-A channel communication */
static SPI_HandleTypeDef 	prvADS9224R_SPI_S_SDOA_HANDLER;

/** @brief DMA handle for SDO-B channel data transfer */
DMA_HandleTypeDef 			prvADS9224R_SPI_S_DMA_SDOB_HANDLER;

/** @brief DMA handle for SDO-A channel data transfer */
DMA_HandleTypeDef 			prvADS9224R_SPI_S_DMA_SDOA_HANDLER;

/**
 * @}
 */
void DMA1_Stream1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&prvADS9224R_SPI_S_DMA_SDOB_HANDLER);
}


void DMA2_Stream0_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&prvADS9224R_SPI_S_DMA_SDOA_HANDLER);
}

void SPI4_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&prvADS9224R_SPI_S_SDOB_HANDLER);
}

void SPI5_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&prvADS9224R_SPI_S_SDOA_HANDLER);
}

void		   		HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
	HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_1);
	if((prvADS9224R_DATA.trigger == ADS9224R_SDO_A) && (hspi->Instance == SPI5))
	{
		if(prvADS9224R_DATA.callback != 0) prvADS9224R_DATA.callback(0);
	}
	if((prvADS9224R_DATA.trigger == ADS9224R_SDO_B) && (hspi->Instance == SPI4))
	{
		if(prvADS9224R_DATA.callback != 0) prvADS9224R_DATA.callback(0);
	}
}
/**
 * @defgroup ADS9224R_PRIVATE_FUNCTIONS ADS9224R driver private functions
 * @{
 */

/**
 * @brief  DMA half transfer complete callback
 * @note   This callback is triggered when DMA half transfer is completed
 * @param  _hdma: Pointer to DMA handle
 */
static void		   		prvADS9224R_DMAHalfComplitedCallback1(DMA_HandleTypeDef *_hdma)
{
	//DRV_GPIO_Pin_SetStateFromISR(DRV_GPIO_PORT_B, 14, DRV_GPIO_PIN_STATE_RESET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
	HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_1);
//	ADS9224R_StopAcquisiton();

	if((prvADS9224R_DATA.trigger == ADS9224R_SDO_A) && (_hdma->Instance == DMA1_Stream1))
	{
		if(prvADS9224R_DATA.callback != 0) prvADS9224R_DATA.callback(0);
	}
	if((prvADS9224R_DATA.trigger == ADS9224R_SDO_B) && (_hdma->Instance == DMA2_Stream0))
	{
		if(prvADS9224R_DATA.callback != 0) prvADS9224R_DATA.callback(0);
	}
}

/**
 * @brief  Initialize SPI slave for SDO-B channel communication
 * @note   Configures SPI4 in slave mode with DMA for data reception
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_SPI_SLAVE_SDOB_Init(void)
{
	/* SPI4 parameter configuration*/
	prvADS9224R_SPI_S_SDOB_HANDLER.Instance = SPI4;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.Mode = SPI_MODE_SLAVE;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.DataSize = SPI_DATASIZE_8BIT;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.CLKPolarity = SPI_POLARITY_LOW;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.CLKPhase = SPI_PHASE_1EDGE;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.NSS = SPI_NSS_HARD_INPUT;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.FirstBit = SPI_FIRSTBIT_MSB;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.TIMode = SPI_TIMODE_DISABLE;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.CRCPolynomial = 0x0;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
	prvADS9224R_SPI_S_SDOB_HANDLER.Init.IOSwap = SPI_IO_SWAP_DISABLE;

	if (HAL_SPI_Init(&prvADS9224R_SPI_S_SDOB_HANDLER) != HAL_OK)  return ADS9224R_STATUS_ERROR;

    __HAL_RCC_DMA1_CLK_ENABLE();

    /* SPI4 DMA Init */
	/* SPI4_RX Init */
	prvADS9224R_SPI_S_DMA_SDOB_HANDLER.Instance = DMA1_Stream1;
	prvADS9224R_SPI_S_DMA_SDOB_HANDLER.Init.Request = DMA_REQUEST_SPI4_RX;
	prvADS9224R_SPI_S_DMA_SDOB_HANDLER.Init.Direction = DMA_PERIPH_TO_MEMORY;
	prvADS9224R_SPI_S_DMA_SDOB_HANDLER.Init.PeriphInc = DMA_PINC_DISABLE;
	prvADS9224R_SPI_S_DMA_SDOB_HANDLER.Init.MemInc = DMA_MINC_ENABLE;
	prvADS9224R_SPI_S_DMA_SDOB_HANDLER.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
	prvADS9224R_SPI_S_DMA_SDOB_HANDLER.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
	prvADS9224R_SPI_S_DMA_SDOB_HANDLER.Init.Mode = DMA_DOUBLE_BUFFER_M0;
	prvADS9224R_SPI_S_DMA_SDOB_HANDLER.Init.Priority = DMA_PRIORITY_VERY_HIGH;
	prvADS9224R_SPI_S_DMA_SDOB_HANDLER.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
	if (HAL_DMA_Init(&prvADS9224R_SPI_S_DMA_SDOB_HANDLER) != HAL_OK)  return ADS9224R_STATUS_ERROR;

	__HAL_LINKDMA(&prvADS9224R_SPI_S_SDOB_HANDLER, hdmarx, prvADS9224R_SPI_S_DMA_SDOB_HANDLER);

    /* DMA1_Stream0_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);

	return ADS9224R_STATUS_OK;
}

/**
 * @brief  Initialize SPI slave for SDO-A channel communication
 * @note   Configures SPI% in slave mode with DMA for data reception
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_SPI_SLAVE_SDOA_Init(void)
{

	/* SPI5 parameter configuration*/
	prvADS9224R_SPI_S_SDOA_HANDLER.Instance = SPI5;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.Mode = SPI_MODE_SLAVE;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.DataSize = SPI_DATASIZE_8BIT;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.CLKPolarity = SPI_POLARITY_LOW;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.CLKPhase = SPI_PHASE_1EDGE;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.NSS = SPI_NSS_HARD_INPUT;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.FirstBit = SPI_FIRSTBIT_MSB;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.TIMode = SPI_TIMODE_DISABLE;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.CRCPolynomial = 0x0;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
	prvADS9224R_SPI_S_SDOA_HANDLER.Init.IOSwap = SPI_IO_SWAP_DISABLE;
	if (HAL_SPI_Init(&prvADS9224R_SPI_S_SDOA_HANDLER) != HAL_OK) return ADS9224R_STATUS_ERROR;

    __HAL_RCC_DMA2_CLK_ENABLE();
    /* SPI5 DMA Init */
    /* SPI5_RX Init */
    prvADS9224R_SPI_S_DMA_SDOA_HANDLER.Instance = DMA2_Stream0;
    prvADS9224R_SPI_S_DMA_SDOA_HANDLER.Init.Request = DMA_REQUEST_SPI5_RX;
    prvADS9224R_SPI_S_DMA_SDOA_HANDLER.Init.Direction = DMA_PERIPH_TO_MEMORY;
    prvADS9224R_SPI_S_DMA_SDOA_HANDLER.Init.PeriphInc = DMA_PINC_DISABLE;
    prvADS9224R_SPI_S_DMA_SDOA_HANDLER.Init.MemInc = DMA_MINC_ENABLE;
    prvADS9224R_SPI_S_DMA_SDOA_HANDLER.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    prvADS9224R_SPI_S_DMA_SDOA_HANDLER.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    prvADS9224R_SPI_S_DMA_SDOA_HANDLER.Init.Mode = DMA_DOUBLE_BUFFER_M0;
    prvADS9224R_SPI_S_DMA_SDOA_HANDLER.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    prvADS9224R_SPI_S_DMA_SDOA_HANDLER.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&prvADS9224R_SPI_S_DMA_SDOA_HANDLER) != HAL_OK) return ADS9224R_STATUS_ERROR;

    __HAL_LINKDMA(&prvADS9224R_SPI_S_SDOA_HANDLER, hdmarx, prvADS9224R_SPI_S_DMA_SDOA_HANDLER);

    /* DMA2_Stream0_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

	return ADS9224R_STATUS_OK;
}




/**
 * @brief  Power up the ADS9224R device
 * @note   Configures GPIO pins and waits for ready signal from device
 * @param  timeout: Timeout value in milliseconds for the power-up sequence
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_PowerUp(uint32_t timeout)
{

	uint32_t curTick;

	/*CS Config*/
	drv_gpio_pin_init_conf_t 	outPinConfig;
	outPinConfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
	outPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

	drv_gpio_pin_init_conf_t 	inPinCOnfig;
	inPinCOnfig.mode = DRV_GPIO_PIN_MODE_INPUT;
	inPinCOnfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;


	/*Ready pin configuration*/
	DRV_GPIO_Port_Init(ADS9224R_READY_STROBE_PORT);
	DRV_GPIO_Pin_Init(ADS9224R_READY_STROBE_PORT, ADS9224R_READY_STROBE_PIN, &inPinCOnfig);

	/*Power down/ RST Config*/
	DRV_GPIO_Port_Init(ADS9224R_RESET_PD_PORT);
	DRV_GPIO_Pin_Init(ADS9224R_RESET_PD_PORT, ADS9224R_RESET_PD_PIN, 	&outPinConfig);


	/*Set high to power up device */
	DRV_GPIO_Pin_SetState(ADS9224R_RESET_PD_PORT, ADS9224R_RESET_PD_PIN, DRV_GPIO_PIN_STATE_SET);

	/*Ready first go high and than go down*/
	curTick = HAL_GetTick();
	while(DRV_GPIO_Pin_ReadState(ADS9224R_READY_STROBE_PORT, ADS9224R_READY_STROBE_PIN) != DRV_GPIO_PIN_STATE_SET)
	{
		if((HAL_GetTick() - curTick) > timeout ) return ADS9224R_STATUS_ERROR;
	}
	curTick = HAL_GetTick();
	while(DRV_GPIO_Pin_ReadState(ADS9224R_READY_STROBE_PORT, ADS9224R_READY_STROBE_PIN) != DRV_GPIO_PIN_STATE_RESET)
	{
		if((HAL_GetTick() - curTick) > timeout ) return ADS9224R_STATUS_ERROR;
	}
	prvADS9224R_DATA.opState = ADS9224R_OP_STATE_UP;
	return ADS9224R_STATUS_OK;
}

/**
 * @brief  Power down the ADS9224R device
 * @note   Configures GPIO pins and sets reset/power down pin to low
 * @param  timeout: Timeout value in milliseconds for the power-down sequence
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_PowerDown(uint32_t timeout)
{
	/*CS Config*/
	drv_gpio_pin_init_conf_t 	outPinConfig;
	outPinConfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
	outPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

	/*Power down/ RST Config*/
	DRV_GPIO_Port_Init(ADS9224R_RESET_PD_PORT);
	DRV_GPIO_Pin_Init(ADS9224R_RESET_PD_PORT, ADS9224R_RESET_PD_PIN, 	&outPinConfig);


	/*Set low to power down device */
	DRV_GPIO_Pin_SetState(ADS9224R_RESET_PD_PORT, ADS9224R_RESET_PD_PIN, DRV_GPIO_PIN_STATE_RESET);


	prvADS9224R_DATA.opState = ADS9224R_OP_STATE_DOWN;

	return ADS9224R_STATUS_OK;
}


/**
 * @brief  Initialize SPI master for ADS9224R configuration
 * @note   Sets up SPI3 in master mode with appropriate pins for device communication
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_CONF_SPI_Master_Init()
{
	if(prvADS9224R_DATA.opState == ADS9224R_OP_STATE_DOWN ||
		((prvADS9224R_DATA.opState == ADS9224R_OP_STATE_ACQ) && (prvADS9224R_DATA.acqState == ADS9224R_ACQ_STATE_ACTIVE)))return ADS9224R_STATUS_ERROR;

	drv_spi_config_t 			config;
	drv_gpio_pin_init_conf_t 	csCOnfig;

	config.mode 		= DRV_SPI_MODE_MASTER;
	config.phase		= DRV_SPI_PHASE_1EDGE;
	config.polarity 	= DRV_SPI_POLARITY_LOW;

	if(DRV_SPI_Instance_Init(DRV_SPI_INSTANCE3, &config) != DRV_SPI_STATUS_OK) return ADS9224R_STATUS_ERROR;


	csCOnfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
	csCOnfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

	/*Initialize CS*/
	DRV_GPIO_Port_Init(ADS9224R_CS_PORT);
	DRV_GPIO_Pin_Init(ADS9224R_CS_PORT, ADS9224R_CS_PIN, &csCOnfig);

	/*Set it to inactive state "1" */
	DRV_GPIO_Pin_SetState(ADS9224R_CS_PORT, ADS9224R_CS_PIN, DRV_GPIO_PIN_STATE_SET);

	return ADS9224R_STATUS_OK;
}

/**
 * @brief  Deinitialize SPI master used for ADS9224R configuration
 * @note   Cleans up SPI3 and CS pin resources
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_CONF_SPI_Master_DeInit()
{
	if(prvADS9224R_DATA.opState == ADS9224R_OP_STATE_DOWN ||
			((prvADS9224R_DATA.opState == ADS9224R_OP_STATE_ACQ) && (prvADS9224R_DATA.acqState == ADS9224R_ACQ_STATE_ACTIVE)))return ADS9224R_STATUS_ERROR;

	if(DRV_SPI_Instance_DeInit(DRV_SPI_INSTANCE3) != DRV_SPI_STATUS_OK) return ADS9224R_STATUS_ERROR;

	if(DRV_GPIO_Pin_DeInit(ADS9224R_CS_PORT, ADS9224R_CS_PIN) != DRV_GPIO_STATUS_OK)  return ADS9224R_STATUS_ERROR;

	return ADS9224R_STATUS_OK;
}

/**
 * @brief  Initialize the configuration state for ADS9224R device
 * @note   Configures GPIO pins and SPI for register access
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_CONF_SetState()
{
	if(prvADS9224R_DATA.opState == ADS9224R_OP_STATE_DOWN ||
			((prvADS9224R_DATA.opState == ADS9224R_OP_STATE_ACQ) && (prvADS9224R_DATA.acqState == ADS9224R_ACQ_STATE_ACTIVE)))return ADS9224R_STATUS_ERROR;

	if(prvADS9224R_DATA.opState == ADS9224R_OP_STATE_DOWN) 	return ADS9224R_STATUS_OK;

	/*CONVST Config*/
	drv_gpio_pin_init_conf_t 	outPinConfig;
	outPinConfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
	outPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

	drv_gpio_pin_init_conf_t 	inPinCOnfig;
	inPinCOnfig.mode = DRV_GPIO_PIN_MODE_INPUT;
	inPinCOnfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

	/*CONVST pin configuration*/
	DRV_GPIO_Port_Init(ADS9224R_CONVST_PORT);
	DRV_GPIO_Pin_Init(ADS9224R_CONVST_PORT, ADS9224R_CONVST_PIN, 	&outPinConfig);

	/*Ready pin configuration*/
	DRV_GPIO_Port_Init(ADS9224R_READY_STROBE_PORT);
	DRV_GPIO_Pin_Init(ADS9224R_READY_STROBE_PORT, ADS9224R_READY_STROBE_PIN, &inPinCOnfig);

	/* When ADS is operational, configure it*/
	/* Initialize SPI for configuration */
	if(prvADS9224R_CONF_SPI_Master_Init() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	/* If read or write is performed from configuration or status registers, CONVST must be high */
	DRV_GPIO_Pin_SetState(ADS9224R_CONVST_PORT, ADS9224R_CONVST_PIN, DRV_GPIO_PIN_STATE_SET);

	prvADS9224R_DATA.opState = ADS9224R_OP_STATE_CONFIG;
	return ADS9224R_STATUS_OK;
}

/**
 * @brief  Exit the configuration state for ADS9224R device
 * @note   Deinitializes GPIO pins and SPI used for register access
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_CONF_UnsetState()
{
	if(prvADS9224R_DATA.opState == ADS9224R_OP_STATE_DOWN ||
			((prvADS9224R_DATA.opState == ADS9224R_OP_STATE_ACQ) && (prvADS9224R_DATA.acqState == ADS9224R_ACQ_STATE_ACTIVE)))return ADS9224R_STATUS_ERROR;

	DRV_GPIO_Pin_DeInit(ADS9224R_CONVST_PORT, ADS9224R_CONVST_PIN);

	/*Ready pin configuration*/
	DRV_GPIO_Pin_DeInit(ADS9224R_READY_STROBE_PORT, ADS9224R_READY_STROBE_PIN);

	/* When ADS is operational, configure it*/
	/* Initialize SPI for configuration */
	if(prvADS9224R_CONF_SPI_Master_DeInit() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	prvADS9224R_DATA.opState = ADS9224R_OP_STATE_UP;

	return ADS9224R_STATUS_OK;
}

/**
 * @brief  Read a register from ADS9224R device using SPI
 * @param  reg: Register address to read from
 * @param  data: Pointer to store the read data
 * @param  timeout: Timeout value in milliseconds for the SPI operation
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_CONF_SPI_Master_ReadReg(uint8_t reg, uint8_t* data, uint32_t timeout)
{
	uint8_t txData[2];

	/* Prepare data */
	txData[0] = 0x20 | (reg & 0x0F);
	txData[1] = 0x00;

	/*CS go low*/
	DRV_GPIO_Pin_SetState(ADS9224R_CS_PORT, ADS9224R_CS_PIN, DRV_GPIO_PIN_STATE_RESET);

	/*Transmit command*/
	if(DRV_SPI_TransmitData(DRV_SPI_INSTANCE3, txData, 2, timeout) != DRV_SPI_STATUS_OK) return DRV_SPI_STATUS_ERROR;

	/*CS go high*/
	DRV_GPIO_Pin_SetState(ADS9224R_CS_PORT, ADS9224R_CS_PIN, DRV_GPIO_PIN_STATE_SET);

	/* Wait for ready signal */
	while(DRV_GPIO_Pin_ReadState(ADS9224R_READY_STROBE_PORT, ADS9224R_READY_STROBE_PIN) != DRV_GPIO_PIN_STATE_SET);

	/* CS go low */
	DRV_GPIO_Pin_SetState(ADS9224R_CS_PORT, ADS9224R_CS_PIN, DRV_GPIO_PIN_STATE_RESET);

	/*Receive data*/
	if(DRV_SPI_ReceiveData(DRV_SPI_INSTANCE3, data, 1, timeout) != DRV_SPI_STATUS_OK) return DRV_SPI_STATUS_ERROR;

	/*CS go high*/
	DRV_GPIO_Pin_SetState(ADS9224R_CS_PORT, ADS9224R_CS_PIN, DRV_GPIO_PIN_STATE_SET);

	return ADS9224R_STATUS_OK;
}


/**
 * @brief  Write a value to a register in ADS9224R device using SPI
 * @param  reg: Register address to write to
 * @param  data: Data to write to the register
 * @param  timeout: Timeout value in milliseconds for the SPI operation
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_CONF_SPI_Master_WriteReg(uint8_t reg, uint8_t data, uint32_t timeout)
{
	uint8_t txData[2];

	/* Prepare data */
	txData[0] = 0x10 | (reg & 0x0F);
	txData[1] = data;

	/*CS go low*/
	DRV_GPIO_Pin_SetState(ADS9224R_CS_PORT, ADS9224R_CS_PIN, DRV_GPIO_PIN_STATE_RESET);

	/*Transmit command*/
	if(DRV_SPI_TransmitData(DRV_SPI_INSTANCE3, txData, 2, timeout) != DRV_SPI_STATUS_OK) return DRV_SPI_STATUS_ERROR;

	/*CS go high*/
	DRV_GPIO_Pin_SetState(ADS9224R_CS_PORT, ADS9224R_CS_PIN, DRV_GPIO_PIN_STATE_SET);

	return ADS9224R_STATUS_OK;
}

/**
 * @brief  Initialize timer for SPI clock generation
 * @note   Configures TIM8 for PWM generation with appropriate timing for SPI clock
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_TIMER_SCLK_Init(void)
{

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_SlaveConfigTypeDef sSlaveConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_OC_InitTypeDef sConfigOC = {0};
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

	prvADS9224R_TIMER_SCLK_HANDLER.Instance = TIM8;
	prvADS9224R_TIMER_SCLK_HANDLER.Init.Prescaler = 0;
	prvADS9224R_TIMER_SCLK_HANDLER.Init.CounterMode = TIM_COUNTERMODE_UP;
	prvADS9224R_TIMER_SCLK_HANDLER.Init.Period = 6-1;
	prvADS9224R_TIMER_SCLK_HANDLER.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	prvADS9224R_TIMER_SCLK_HANDLER.Init.RepetitionCounter = 15;
	prvADS9224R_TIMER_SCLK_HANDLER.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&prvADS9224R_TIMER_SCLK_HANDLER) != HAL_OK) return ADS9224R_STATUS_ERROR;

	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&prvADS9224R_TIMER_SCLK_HANDLER, &sClockSourceConfig) != HAL_OK) return ADS9224R_STATUS_ERROR;
	if (HAL_TIM_PWM_Init(&prvADS9224R_TIMER_SCLK_HANDLER) != HAL_OK) return ADS9224R_STATUS_ERROR;
	if (HAL_TIM_OnePulse_Init(&prvADS9224R_TIMER_SCLK_HANDLER, TIM_OPMODE_SINGLE) != HAL_OK) return ADS9224R_STATUS_ERROR;

	sSlaveConfig.SlaveMode = TIM_SLAVEMODE_COMBINED_RESETTRIGGER;
	sSlaveConfig.InputTrigger = TIM_TS_ETRF;
	sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_NONINVERTED;
	sSlaveConfig.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;
	sSlaveConfig.TriggerFilter = 0;
	if (HAL_TIM_SlaveConfigSynchro(&prvADS9224R_TIMER_SCLK_HANDLER, &sSlaveConfig) != HAL_OK) return ADS9224R_STATUS_ERROR;

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_UPDATE;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&prvADS9224R_TIMER_SCLK_HANDLER, &sMasterConfig) != HAL_OK) return ADS9224R_STATUS_ERROR;

	sConfigOC.OCMode		 = TIM_OCMODE_PWM1;
	sConfigOC.Pulse			 = 3;
	sConfigOC.OCPolarity	 = TIM_OCPOLARITY_LOW;
	sConfigOC.OCFastMode	 = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState	 = TIM_OCIDLESTATE_SET;
	sConfigOC.OCNIdleState	 = TIM_OCNIDLESTATE_SET;
	if (HAL_TIM_PWM_ConfigChannel(&prvADS9224R_TIMER_SCLK_HANDLER, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) return ADS9224R_STATUS_ERROR;

	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 0;
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	sBreakDeadTimeConfig.BreakFilter = 0;
	sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
	sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
	sBreakDeadTimeConfig.Break2Filter = 0;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
	if (HAL_TIMEx_ConfigBreakDeadTime(&prvADS9224R_TIMER_SCLK_HANDLER, &sBreakDeadTimeConfig) != HAL_OK) return ADS9224R_STATUS_ERROR;

	 return ADS9224R_STATUS_OK;

}

/**
 * @brief  Initialize timer for chip select signal generation
 * @note   Configures TIM4 for PWM generation with appropriate timing for CS signal
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_TIMER_CS_Init(void)
{

	/* USER CODE BEGIN TIM4_Init 0 */

	/* USER CODE END TIM4_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_SlaveConfigTypeDef sSlaveConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_OC_InitTypeDef sConfigOC = {0};

	/* USER CODE BEGIN TIM4_Init 1 */

	/* USER CODE END TIM4_Init 1 */
	prvADS9224R_TIMER_CS_HANDLER.Instance = TIM4;
	prvADS9224R_TIMER_CS_HANDLER.Init.Prescaler = 0;
	prvADS9224R_TIMER_CS_HANDLER.Init.CounterMode = TIM_COUNTERMODE_UP;
	prvADS9224R_TIMER_CS_HANDLER.Init.Period = 102-1;
	prvADS9224R_TIMER_CS_HANDLER.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	prvADS9224R_TIMER_CS_HANDLER.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&prvADS9224R_TIMER_CS_HANDLER) != HAL_OK) return ADS9224R_STATUS_ERROR;

	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&prvADS9224R_TIMER_CS_HANDLER, &sClockSourceConfig) != HAL_OK) return ADS9224R_STATUS_ERROR;
	if (HAL_TIM_PWM_Init(&prvADS9224R_TIMER_CS_HANDLER) != HAL_OK) return ADS9224R_STATUS_ERROR;
	if (HAL_TIM_OnePulse_Init(&prvADS9224R_TIMER_CS_HANDLER, TIM_OPMODE_SINGLE) != HAL_OK) return ADS9224R_STATUS_ERROR;

	sSlaveConfig.SlaveMode = TIM_SLAVEMODE_COMBINED_RESETTRIGGER;
	sSlaveConfig.InputTrigger = TIM_TS_ETRF;
	sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_NONINVERTED;
	sSlaveConfig.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;
	sSlaveConfig.TriggerFilter = 0;
	if (HAL_TIM_SlaveConfigSynchro(&prvADS9224R_TIMER_CS_HANDLER, &sSlaveConfig) != HAL_OK) return ADS9224R_STATUS_ERROR;

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&prvADS9224R_TIMER_CS_HANDLER, &sMasterConfig) != HAL_OK) return ADS9224R_STATUS_ERROR;

	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 2;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
	sConfigOC.OCNIdleState = TIM_OCIDLESTATE_SET;
	sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	if (HAL_TIM_PWM_ConfigChannel(&prvADS9224R_TIMER_CS_HANDLER, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) return ADS9224R_STATUS_ERROR;

	return ADS9224R_STATUS_OK;

}

volatile uint8_t data1[200];
volatile uint8_t data2[200];
/**
 * @brief  Initialize timer for conversion start signal generation
 * @note   Configures TIM5 for PWM generation with appropriate timing for CONVST signal
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_TIMER_CONVST_Init(void)
{

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_OC_InitTypeDef sConfigOC = {0};

	prvADS9224R_TIMER_CONVST_HANDLER.Instance = TIM5;
	prvADS9224R_TIMER_CONVST_HANDLER.Init.Prescaler = prvADS9224R_DATA.timPrescaller;
	prvADS9224R_TIMER_CONVST_HANDLER.Init.CounterMode = TIM_COUNTERMODE_UP;
	prvADS9224R_TIMER_CONVST_HANDLER.Init.Period = prvADS9224R_DATA.timPeriod;
	prvADS9224R_TIMER_CONVST_HANDLER.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	prvADS9224R_TIMER_CONVST_HANDLER.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&prvADS9224R_TIMER_CONVST_HANDLER) != HAL_OK) return ADS9224R_STATUS_ERROR;
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&prvADS9224R_TIMER_CONVST_HANDLER, &sClockSourceConfig) != HAL_OK) return ADS9224R_STATUS_ERROR;
	if (HAL_TIM_PWM_Init(&prvADS9224R_TIMER_CONVST_HANDLER) != HAL_OK) return ADS9224R_STATUS_ERROR;

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&prvADS9224R_TIMER_CONVST_HANDLER, &sMasterConfig) != HAL_OK) return ADS9224R_STATUS_ERROR;

	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = prvADS9224R_DATA.timPulse;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCNIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNPolarity = TIM_OCNPOLARITY_LOW;
	if (HAL_TIM_PWM_ConfigChannel(&prvADS9224R_TIMER_CONVST_HANDLER, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) return ADS9224R_STATUS_ERROR;

    return ADS9224R_STATUS_OK;

}

/**
 * @brief  Deinitialize conversion start timer
 * @note   Cleans up resources used by TIM5 for CONVST signal generation
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_TIMER_CONVST_DeInit(void)
{


	if(HAL_TIM_Base_DeInit(&prvADS9224R_TIMER_CONVST_HANDLER) != HAL_OK) return ADS9224R_STATUS_OK;

	if(HAL_TIM_PWM_DeInit(&prvADS9224R_TIMER_CONVST_HANDLER) != HAL_OK) return ADS9224R_STATUS_OK;

    return ADS9224R_STATUS_OK;

}

/**
 * @brief  Deinitialize chip select timer
 * @note   Cleans up resources used by TIM4 for CS signal generation
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_TIMER_CS_DeInit(void)
{


	if(HAL_TIM_Base_DeInit(&prvADS9224R_TIMER_CS_HANDLER) != HAL_OK) return ADS9224R_STATUS_OK;

	if(HAL_TIM_OnePulse_DeInit(&prvADS9224R_TIMER_CS_HANDLER) != HAL_OK) return ADS9224R_STATUS_OK;

	if(HAL_TIM_PWM_DeInit(&prvADS9224R_TIMER_CS_HANDLER) != HAL_OK) return ADS9224R_STATUS_OK;

    return ADS9224R_STATUS_OK;

}

/**
 * @brief  Deinitialize SPI clock timer
 * @note   Cleans up resources used by TIM8 for SPI clock generation
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_TIMER_SCLK_DeInit(void)
{


	if(HAL_TIM_Base_DeInit(&prvADS9224R_TIMER_SCLK_HANDLER) != HAL_OK) return ADS9224R_STATUS_OK;

	if(HAL_TIM_OnePulse_DeInit(&prvADS9224R_TIMER_SCLK_HANDLER) != HAL_OK) return ADS9224R_STATUS_OK;

	if(HAL_TIM_PWM_DeInit(&prvADS9224R_TIMER_SCLK_HANDLER) != HAL_OK) return ADS9224R_STATUS_OK;

    return ADS9224R_STATUS_OK;

}


/**
 * @brief  Set acquisition state for ADC data capture
 * @note   Configures timers, SPI, and DMA for continuous data acquisition
 * @param  sdoaBuffer0: First buffer for SDO-A channel data
 * @param  sdoaBuffer1: Second buffer for SDO-A channel data (for double buffering)
 * @param  sdobBuffer0: First buffer for SDO-B channel data
 * @param  sdobBuffer1: Second buffer for SDO-B channel data (for double buffering)
 * @param  size: Size of each buffer in bytes
 * @retval ::ads9224r_status_t: ADS9224R_STATUS_OK if successful, otherwise ADS9224R_STATUS_ERROR
 */
static ads9224r_status_t prvADS9224R_ACQ_SetState(uint8_t* sdoaBuffer0, uint8_t* sdoaBuffer1, uint8_t* sdobBuffer0,uint8_t* sdobBuffer1, uint32_t size)
{
	uint32_t curTick = 0;
	if(prvADS9224R_DATA.opState == ADS9224R_OP_STATE_DOWN ||
			((prvADS9224R_DATA.opState == ADS9224R_OP_STATE_ACQ) && (prvADS9224R_DATA.acqState == ADS9224R_ACQ_STATE_ACTIVE)))return ADS9224R_STATUS_ERROR;

	if(prvADS9224R_DATA.opState == ADS9224R_OP_STATE_DOWN) 	return ADS9224R_STATUS_ERROR;


	/*Check if driver and device were in CONFIG state*/
	if(prvADS9224R_DATA.opState == ADS9224R_OP_STATE_CONFIG)
	{
		/*Clear config settings to release pins that in ACQ state are under timers' control*/
		if(prvADS9224R_CONF_UnsetState() != ADS9224R_STATUS_OK)  	return ADS9224R_STATUS_ERROR;
	}
	/* Ready is used to check
	 * if initialization of CONVST trigger conversion whereas CS is used to reset CONVST*/
	/*CONVST Config*/
	drv_gpio_pin_init_conf_t 	outPinConfig;
	outPinConfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
	outPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

	drv_gpio_pin_init_conf_t 	inPinCOnfig;
	inPinCOnfig.mode = DRV_GPIO_PIN_MODE_INPUT;
	inPinCOnfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

	/*CONVST pin configuration*/
	DRV_GPIO_Port_Init(ADS9224R_CS_PORT);
	DRV_GPIO_Pin_Init(ADS9224R_CS_PORT, ADS9224R_CS_PIN, 	&outPinConfig);

	/*Ready pin configuration*/
	DRV_GPIO_Port_Init(ADS9224R_READY_STROBE_PORT);
	DRV_GPIO_Pin_Init(ADS9224R_READY_STROBE_PORT, ADS9224R_READY_STROBE_PIN, &inPinCOnfig);

	/*Ready pin configuration*/
	DRV_GPIO_Port_Init(DRV_GPIO_PORT_B);
	DRV_GPIO_Pin_Init(DRV_GPIO_PORT_B, 14, &outPinConfig);

	DRV_GPIO_Port_Init(DRV_GPIO_PORT_B);
	DRV_GPIO_Pin_Init(DRV_GPIO_PORT_B, 2, &outPinConfig);
	DRV_GPIO_Pin_SetState(DRV_GPIO_PORT_B, 2, DRV_GPIO_PIN_STATE_RESET);

	DRV_GPIO_Port_Init(DRV_GPIO_PORT_D);
	DRV_GPIO_Pin_Init(DRV_GPIO_PORT_D, 1, &outPinConfig);



	if(prvADS9224R_TIMER_CONVST_Init() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;
	curTick = HAL_GetTick();

	/*Wait for 5ms to debaunce in case that initialization of CONVST trigger CONVST signal*/
	while((HAL_GetTick() - curTick) < 1000);


	if(DRV_GPIO_Pin_ReadState(ADS9224R_READY_STROBE_PORT, ADS9224R_READY_STROBE_PIN) == DRV_GPIO_PIN_STATE_SET)
	{
		/*If ready pin is set, CONVST trigger conversion. Pulling down CS pin will reset ready and prepare ADC for
		 * acquisition. */
		DRV_GPIO_Pin_SetState(ADS9224R_CS_PORT, ADS9224R_CS_PIN, DRV_GPIO_PIN_STATE_RESET);
		/*Return CS to inactive state */
		DRV_GPIO_Pin_SetState(ADS9224R_CS_PORT, ADS9224R_CS_PIN, DRV_GPIO_PIN_STATE_SET);

	}

	if(prvADS9224R_TIMER_CS_Init() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;
	if(prvADS9224R_TIMER_SCLK_Init() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;


	if(prvADS9224R_SPI_SLAVE_SDOA_Init() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;
	if(prvADS9224R_SPI_SLAVE_SDOB_Init() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	//if(HAL_DMA_RegisterCallback(&prvADS9224R_SPI_S_DMA_SDOA_HANDLER, HAL_DMA_XFER_CPLT_CB_ID, prvADS9224R_DMAHalfComplitedCallback0))return ADS9224R_STATUS_ERROR;
	if(HAL_DMA_RegisterCallback(&prvADS9224R_SPI_S_DMA_SDOA_HANDLER, HAL_DMA_XFER_M1CPLT_CB_ID, prvADS9224R_DMAHalfComplitedCallback1))return ADS9224R_STATUS_ERROR;
	//if(HAL_DMA_RegisterCallback(&prvADS9224R_SPI_S_DMA_SDOB_HANDLER, HAL_DMA_XFER_CPLT_CB_ID, prvADS9224R_DMAHalfComplitedCallback0))return ADS9224R_STATUS_ERROR;
	if(HAL_DMA_RegisterCallback(&prvADS9224R_SPI_S_DMA_SDOB_HANDLER, HAL_DMA_XFER_M1CPLT_CB_ID, prvADS9224R_DMAHalfComplitedCallback1))return ADS9224R_STATUS_ERROR;

	HAL_SPI_Receive_DMA(&prvADS9224R_SPI_S_SDOA_HANDLER, sdoaBuffer0, sdoaBuffer1, size);
	HAL_SPI_Receive_DMA(&prvADS9224R_SPI_S_SDOB_HANDLER, sdobBuffer0, sdobBuffer1, size);


//	HAL_DMAEx_MultiBufferStart_IT(&prvADS9224R_SPI_S_DMA_SDOA_HANDLER,(uint32_t)&prvADS9224R_SPI_S_SDOA_HANDLER.Instance->RXDR, (uint32_t)sdoaBuffer0, (uint32_t)sdoaBuffer1, size);
//	HAL_DMAEx_MultiBufferStart_IT(&prvADS9224R_SPI_S_DMA_SDOB_HANDLER,(uint32_t)&prvADS9224R_SPI_S_SDOB_HANDLER.Instance->RXDR, (uint32_t)sdobBuffer0, (uint32_t)sdobBuffer1, size);

//	HAL_SPI_Receive_DMA(hspi, pData, pData1, Size)



	prvADS9224R_DATA.opState = ADS9224R_OP_STATE_ACQ;
	prvADS9224R_DATA.acqState = ADS9224R_ACQ_STATE_INACTIVE;

	return ADS9224R_STATUS_OK;
}




ads9224r_status_t	ADS9224R_Init(ads9224r_config_t *ads9224r_config_t, uint32_t timeout)
{
	uint8_t registersContent[8] = {0};

	memset(&prvADS9224R_DATA, 0, sizeof(ads9224r_handle_t));


	if(prvADS9224R_PowerDown(1000) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	if(prvADS9224R_PowerUp(timeout) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	if(prvADS9224R_CONF_SetState() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	prvADS9224R_DATA.init = ADS9224R_INIT_STATE_INIT;

	/*default sampling period is max*/
	ADS9224R_SetSamplingRate(200-1, 0);

	for(uint8_t i =0; i < 8; i++)
	{
		if(prvADS9224R_CONF_SPI_Master_ReadReg(i, &registersContent[i], timeout) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;
	}

	if(ADS9224R_SetPatternState(ADS9224R_FPATTERN_STATE_ENABLED, timeout) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;
	if(ADS9224R_SetPatternState(ADS9224R_FPATTERN_STATE_DISABLED, timeout) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	return ADS9224R_STATUS_OK;

}

ads9224r_status_t	ADS9224R_SetPatternState(ads9224r_fpattern_state_t state, uint32_t timeout)
{
	/* Check if:
	 * 1. driver is not initialized
	 * 2. if it is in power down state
	 * 3. if it is in acq state and acquisiton is active */
	if(prvADS9224R_DATA.init == ADS9224R_INIT_STATE_NOINIT ||
			prvADS9224R_DATA.opState == ADS9224R_OP_STATE_DOWN ||
			((prvADS9224R_DATA.opState == ADS9224R_OP_STATE_ACQ) && (prvADS9224R_DATA.acqState == ADS9224R_ACQ_STATE_ACTIVE)))return ADS9224R_STATUS_ERROR;

	/* Check if driver and device is in configuration state*/
	if(prvADS9224R_DATA.opState != ADS9224R_OP_STATE_CONFIG)
	{
		if(prvADS9224R_CONF_SetState() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;
	}

	uint8_t regContentBit = 0;
	uint8_t dataRx = 0;



	/* Read register */
	if(prvADS9224R_CONF_SPI_Master_ReadReg(ADS9224R_REG_ADDR_OUTPUT_DATA_WORD_CFG, &dataRx, timeout) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	/* Modify */
	switch(state)
	{
	case ADS9224R_FPATTERN_STATE_DISABLED:
		dataRx &= ~ADS9224R_REG_MASK_OUTPUT_DATA_WORD_CFG_FIXED_PATTERN_DATA;
		break;
	case ADS9224R_FPATTERN_STATE_ENABLED:
		dataRx |= ADS9224R_REG_MASK_OUTPUT_DATA_WORD_CFG_FIXED_PATTERN_DATA;
		break;
	}

	/* Write back */
	if(prvADS9224R_CONF_SPI_Master_WriteReg(ADS9224R_REG_ADDR_OUTPUT_DATA_WORD_CFG, dataRx, timeout) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	dataRx = 0;

	/* Read */
	if(prvADS9224R_CONF_SPI_Master_ReadReg(ADS9224R_REG_ADDR_OUTPUT_DATA_WORD_CFG, &dataRx, timeout) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;


	/* Modify */
	switch(state)
	{
	case ADS9224R_FPATTERN_STATE_DISABLED:
		if((dataRx & ADS9224R_REG_MASK_OUTPUT_DATA_WORD_CFG_FIXED_PATTERN_DATA)) return ADS9224R_STATUS_ERROR;
		break;
	case ADS9224R_FPATTERN_STATE_ENABLED:
		if(!(dataRx & ADS9224R_REG_MASK_OUTPUT_DATA_WORD_CFG_FIXED_PATTERN_DATA)) return ADS9224R_STATUS_ERROR;
		break;
	}

	return ADS9224R_STATUS_OK;
}
ads9224r_status_t   ADS9224R_StartAcquisiton(uint8_t* sdoaBuffer0, uint8_t* sdoaBuffer1, uint8_t* sdobBuffer0, uint8_t* sdobBuffer1, uint32_t size)
{
	/* Check if:
	 * 1. driver is not initialized
	 * 2. if it is in power down state
	 * 3. if it is in acq state and acquisiton is active */
	if(prvADS9224R_DATA.init == ADS9224R_INIT_STATE_NOINIT || prvADS9224R_DATA.opState == ADS9224R_OP_STATE_DOWN) return ADS9224R_STATUS_ERROR;
	if((prvADS9224R_DATA.opState == ADS9224R_OP_STATE_ACQ) && (prvADS9224R_DATA.acqState == ADS9224R_ACQ_STATE_ACTIVE))return ADS9224R_STATUS_ERROR;
	if(prvADS9224R_DATA.opState != ADS9224R_OP_STATE_ACQ)
	{
		if(prvADS9224R_ACQ_SetState(sdoaBuffer0, sdoaBuffer1, sdobBuffer0, sdobBuffer1, size) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;
	}
	prvADS9224R_TIMER_CS_HANDLER.Instance->CNT = 0;
	prvADS9224R_TIMER_SCLK_HANDLER.Instance->CNT = 0;
	prvADS9224R_TIMER_CONVST_HANDLER.Instance->CNT = 0;
	HAL_TIM_PWM_Start(&prvADS9224R_TIMER_CS_HANDLER, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&prvADS9224R_TIMER_SCLK_HANDLER, TIM_CHANNEL_4);
	HAL_TIM_PWM_Start(&prvADS9224R_TIMER_CONVST_HANDLER, TIM_CHANNEL_4);

	prvADS9224R_DATA.acqState = ADS9224R_ACQ_STATE_ACTIVE;
	return ADS9224R_STATUS_OK;
}
ads9224r_status_t   ADS9224R_StopAcquisiton()
{
	uint8_t registersContent[8] = {0};

	if(prvADS9224R_DATA.init == ADS9224R_INIT_STATE_NOINIT || prvADS9224R_DATA.opState == ADS9224R_OP_STATE_DOWN) return ADS9224R_STATUS_ERROR;

	HAL_TIM_PWM_Stop(&prvADS9224R_TIMER_CS_HANDLER, TIM_CHANNEL_1);
	HAL_TIM_PWM_Stop(&prvADS9224R_TIMER_SCLK_HANDLER, TIM_CHANNEL_4);
	HAL_TIM_PWM_Stop(&prvADS9224R_TIMER_CONVST_HANDLER, TIM_CHANNEL_4);

//	HAL_SPI_DMAStop(&prvADS9224R_SPI_S_SDOA_HANDLER);
//	HAL_SPI_DMAStop(&prvADS9224R_SPI_S_SDOB_HANDLER);

	/*If previous transfer is stopped at the middle of the packet, this part  of the code
	 * return DMA to inital state*/
//	LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_0);
//	LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_0, 500);
//	LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_0);
//	LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_1);
//	LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_1, 500);
//	LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_1);


	prvADS9224R_DATA.acqState = ADS9224R_ACQ_STATE_INACTIVE;

	/*When stop is triggered, everything should be return to initial state*/
	/*Deinitialize all timers*/
	if(prvADS9224R_TIMER_CONVST_DeInit() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;
	if(prvADS9224R_TIMER_CS_DeInit() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;
	if(prvADS9224R_TIMER_SCLK_DeInit() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;


	HAL_SPI_DeInit(&prvADS9224R_SPI_S_SDOB_HANDLER);
	HAL_SPI_DeInit(&prvADS9224R_SPI_S_SDOA_HANDLER);

	HAL_DMA_DeInit(&prvADS9224R_SPI_S_DMA_SDOB_HANDLER);
	HAL_DMA_DeInit(&prvADS9224R_SPI_S_DMA_SDOA_HANDLER);


	/*Reset ADC*/
	if(prvADS9224R_PowerDown(1000) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	if(prvADS9224R_PowerUp(1000) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	/*Set to config state*/
	if(prvADS9224R_CONF_SetState() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	for(uint8_t i =0; i < 8; i++)
	{
		if(prvADS9224R_CONF_SPI_Master_ReadReg(i, &registersContent[i], 1000) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;
	}

	/*This part of the code was used during testing to confirm that it is possible to set and read registers*/
	/*
	if(ADS9224R_SetPatternState(ADS9224R_FPATTERN_STATE_ENABLED, 1000) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;
	if(ADS9224R_SetPatternState(ADS9224R_FPATTERN_STATE_DISABLED, 1000) != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;
	*/

	return ADS9224R_STATUS_OK;
}
ads9224r_status_t   ADS9224R_SetConfig(ads9224r_config_t *config)
{

	return ADS9224R_STATUS_OK;
}
ads9224r_status_t   ADS9224R_SetSamplingRate(uint32_t timPeriod, uint32_t timPrescaler)
{
	/* Check if:
	 * 1. driver is not initialized
	 * 2. if it is in power down state
	 * 3. if it is in acq state and acquisiton is active */
	if(prvADS9224R_DATA.init == ADS9224R_INIT_STATE_NOINIT ||
			prvADS9224R_DATA.opState == ADS9224R_OP_STATE_DOWN ||
			((prvADS9224R_DATA.opState == ADS9224R_OP_STATE_ACQ) && (prvADS9224R_DATA.acqState == ADS9224R_ACQ_STATE_ACTIVE)))return ADS9224R_STATUS_ERROR;

	/*Prevent from configuring sampling rate smaller than 1uS*/
	if(timPrescaler == 0 && timPeriod < 200-1)return ADS9224R_STATUS_ERROR;

//	prvDRV_AIN_ADC_CONFIG.samplingTime = prescaller/DRV_AIN_ADC_TIM_INPUT_CLK*period;
	prvADS9224R_DATA.timPrescaller = timPrescaler;
	prvADS9224R_DATA.timPeriod = timPeriod;
	prvADS9224R_DATA.timPulse =  (int)(0.05*(float)(timPeriod+1));
	if(prvADS9224R_DATA.timPulse == 0) prvADS9224R_DATA.timPulse = 1;
	prvADS9224R_DATA.samplingPeriod = (float)(timPrescaler + 1)/(float)200*(float)(timPeriod+1);

	//if(prvADS9224R_TIMER_CONVST_Init() != ADS9224R_STATUS_OK) return ADS9224R_STATUS_ERROR;

	prvADS9224R_TIMER_CONVST_HANDLER.Instance->PSC = timPrescaler;
	prvADS9224R_TIMER_CONVST_HANDLER.Instance->ARR = prvADS9224R_DATA.timPeriod;
	prvADS9224R_TIMER_CONVST_HANDLER.Instance->CCR4 = prvADS9224R_DATA.timPulse;
	return ADS9224R_STATUS_OK;
}
ads9224r_status_t   ADS9224R_RegisterCallback(bufferReceiveCallback callback, ads9224r_sdo_t trigger)
{
	if(prvADS9224R_DATA.init == ADS9224R_INIT_STATE_NOINIT || prvADS9224R_DATA.opState == ADS9224R_OP_STATE_DOWN) return ADS9224R_STATUS_ERROR;
	prvADS9224R_DATA.callback = callback;
	prvADS9224R_DATA.trigger = trigger;
	return ADS9224R_STATUS_OK;
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
