/**
 ******************************************************************************
 * @file   	ads9224r.h
 *
 * @brief  	ADS9224R Dual-Channel, 24-bit, 155-kSPS ADC driver provides 
 * 			hardware abstraction layer for external ADS9224R ADC peripheral.
 * 			This driver supports SPI/parallel communication protocols, dual
 * 			SDO channels, configurable sampling rates, power management,
 * 			data averaging, and DMA-based streaming with callback mechanisms
 * 			for high-speed analog data acquisition from dual simultaneous
 * 			sampling channels.
 * 			All ADS9224R driver interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	October 2024
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_ANALOGIN_ADS9224R_ADS9224R_H_
#define CORE_DRIVERS_PLATFORM_ANALOGIN_ADS9224R_ADS9224R_H_

#include "drv_gpio.h"
#include <string.h>

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup ADS9224R_DRIVER ADS9224R External ADC Driver
 * @{
 */

/**
 * @defgroup ADS9224R_PLATFORM_DEFINES ADS9224R platform specific defines
 * @{
 */
#define ADS9224R_TIMERS_INPUT_CLK					200		/*!< Timer input clock frequency in MHz */

#define ADS9224R_READY_STROBE_PORT					DRV_GPIO_PORT_G		/*!< Ready strobe GPIO port */
#define ADS9224R_READY_STROBE_PIN					10					/*!< Ready strobe GPIO pin */
#define ADS9224R_RESET_PD_PORT						DRV_GPIO_PORT_G		/*!< Reset/Power-down GPIO port */
#define ADS9224R_RESET_PD_PIN						8					/*!< Reset/Power-down GPIO pin */
#define ADS9224R_CONVST_PORT						DRV_GPIO_PORT_A		/*!< Conversion start GPIO port */
#define ADS9224R_CONVST_PIN							3					/*!< Conversion start GPIO pin */
#define ADS9224R_CS_PORT							DRV_GPIO_PORT_D		/*!< Chip select GPIO port */
#define ADS9224R_CS_PIN								12					/*!< Chip select GPIO pin */
/**
 * @}
 */

/**
 * @defgroup ADS9224R_REGISTER_DEFINES ADS9224R device register addresses and masks
 * @{
 */
#define ADS9224R_REG_ADDR_DEVICE_STATUS				0x00	/*!< Device status register address */
#define ADS9224R_REG_ADDR_POWER_DOWN_CNFG			0x01	/*!< Power down configuration register address */
#define ADS9224R_REG_ADDR_PROTOCOL_CNFG				0x02	/*!< Protocol configuration register address */
#define ADS9224R_REG_ADDR_BUS_WIDTH					0x03	/*!< Bus width register address */
#define ADS9224R_REG_ADDR_CRT_CFG					0x04	/*!< CRT configuration register address */
#define ADS9224R_REG_ADDR_OUTPUT_DATA_WORD_CFG		0x05	/*!< Output data word configuration register address */
#define ADS9224R_REG_ADDR_DATA_AVG_CFG				0x06	/*!< Data averaging configuration register address */
#define ADS9224R_REG_ADDR_REFBY2_OFFSET				0x07	/*!< Reference by 2 offset register address */

#define ADS9224R_REG_MASK_OUTPUT_DATA_WORD_CFG_DATA_RIGHT_ALIGNED		0x01	/*!< Data right aligned mask */
#define ADS9224R_REG_MASK_OUTPUT_DATA_WORD_CFG_FIXED_PATTERN_DATA		0x02	/*!< Fixed pattern data mask */
#define ADS9224R_REG_MASK_OUTPUT_DATA_WORD_CFG_PARALLEL_MODE_DATA_FORMAT	0x10	/*!< Parallel mode data format mask */
#define ADS9224R_REG_MASK_OUTPUT_DATA_WORD_CFG_READY_MASK				0x20	/*!< Ready signal mask */
/**
 * @}
 */

/**
 * @defgroup ADS9224R_PUBLIC_TYPES ADS9224R driver public data types
 * @{
 */

/**
 * @brief ADS9224R driver return status
 */
typedef enum
{
	ADS9224R_STATUS_OK = 0,			/*!< ADS9224R operation successful */
	ADS9224R_STATUS_ERROR			/*!< ADS9224R operation failed */
}ads9224r_status_t;

/**
 * @brief ADS9224R Serial Data Output (SDO) channels
 */
typedef enum
{
	ADS9224R_SDO_A,					/*!< SDO channel A */
	ADS9224R_SDO_B					/*!< SDO channel B */
}ads9224r_sdo_t;

/**
 * @brief ADS9224R initialization state
 */
typedef enum
{
	ADS9224R_INIT_STATE_NOINIT = 0,	/*!< Device not initialized */
	ADS9224R_INIT_STATE_INIT		/*!< Device initialized */
}ads9224r_init_state_t;

/**
 * @brief ADS9224R data acquisition state
 */
typedef enum
{
	ADS9224R_ACQ_STATE_INACTIVE = 0,	/*!< Data acquisition inactive */
	ADS9224R_ACQ_STATE_ACTIVE			/*!< Data acquisition active */
}ads9224r_acq_state_t;

/**
 * @brief ADS9224R operational state
 */
typedef enum
{
	ADS9224R_OP_STATE_DOWN = 0,		/*!< Device powered down */
	ADS9224R_OP_STATE_UP,			/*!< Device powered up */
	ADS9224R_OP_STATE_CONFIG,		/*!< Device in configuration mode */
	ADS9224R_OP_STATE_ACQ			/*!< Device in acquisition mode */
}ads9224r_op_state_t;

/**
 * @brief ADS9224R SDO bus width configuration
 */
typedef enum
{
	ADS9224R_SDO_WIDTH_ONE_SDO = 0,	/*!< Single SDO line */
	ADS9224R_SDO_WIDTH_DUAL_SDO,	/*!< Dual SDO lines */
	ADS9224R_SDO_WIDTH_QUAD_SDO		/*!< Quad SDO lines */
}ads9224r_sdo_width_t;

/**
 * @brief ADS9224R power down state
 */
typedef enum
{
	ADS9224R_POWER_DOWN_STATE_ACTIVE,	/*!< Power down active (component powered down) */
	ADS9224R_POWER_DOWN_STATE_INACTIVE	/*!< Power down inactive (component powered up) */
}ads9224r_power_down_state_t;

/**
 * @brief ADS9224R data transfer frame zones
 */
typedef enum
{
	ADS9224R_DATA_TRANSFER_FRAME_ZONE_1 = 0,	/*!< Data transfer frame zone 1 */
	ADS9224R_DATA_TRANSFER_FRAME_ZONE_2			/*!< Data transfer frame zone 2 */
}ads9224r_data_transfer_frame_t;

/**
 * @brief ADS9224R SDO protocol configuration
 */
typedef enum
{
	ADS9224R_SDO_PROTOCOL_SPI_SDR = 0,		/*!< SPI Single Data Rate */
	ADS9224R_SDO_PROTOCOL_SPI_DDR,			/*!< SPI Double Data Rate */
	ADS9224R_SDO_PROTOCOL_SPI_SDR_CRT,		/*!< SPI SDR with CRT */
	ADS9224R_SDO_PROTOCOL_SPI_DDR_CRT,		/*!< SPI DDR with CRT */
	ADS9224R_SDO_PROTOCOL_SPI_PARALLEL_1,	/*!< SPI Parallel mode 1 */
	ADS9224R_SDO_PROTOCOL_SPI_PARALLEL_2,	/*!< SPI Parallel mode 2 */
	ADS9224R_SDO_PROTOCOL_SPI_PARALLEL_3,	/*!< SPI Parallel mode 3 */
	ADS9224R_SDO_PROTOCOL_SPI_PARALLEL_4	/*!< SPI Parallel mode 4 */
}ads9224r_sdo_protocol_t;

/**
 * @brief ADS9224R SPI clock polarity
 */
typedef enum
{
	ADS9224R_CPOL_LOW = 0,			/*!< Clock polarity low (idle state low) */
	ADS9224R_CPOL_HIGH				/*!< Clock polarity high (idle state high) */
}ads9224r_cpol_t;

/**
 * @brief ADS9224R SPI clock phase
 */
typedef enum
{
	ADS9224_CPHA_LOW = 0,			/*!< Clock phase low (data captured on first edge) */
	ADS9224_CPHA_HIGH				/*!< Clock phase high (data captured on second edge) */
}ads9224r_cpha_t;

/**
 * @brief ADS9224R CRT (Conversion Rate Timer) clock source
 */
typedef enum
{
	ADS9224R_CRT_CLK_SERIAL = 0,	/*!< CRT clock from serial interface */
	ADS9224R_CRT_CLK_INTCLK,		/*!< CRT clock from internal clock */
	ADS9224R_CRT_CLK_INTCLK_1_2,	/*!< CRT clock from internal clock divided by 2 */
	ADS9224R_CRT_CLK_INTCLK_1_4		/*!< CRT clock from internal clock divided by 4 */
}ads9224r_crt_clk_t;

/**
 * @brief ADS9224R parallel mode data organization
 */
typedef enum
{
	ADS9224R_PARALLEL_MODE_AA = 0,	/*!< Parallel mode AA (channel A data on both SDOs) */
	ADS9224R_PARALLEL_MODE_AB		/*!< Parallel mode AB (channel A on SDO_A, channel B on SDO_B) */
}ads9224r_parallel_mode_t;

/**
 * @brief ADS9224R data alignment in output word
 */
typedef enum
{
	ADS9224R_DATA_ALIGNED_LEFT = 0,	/*!< Data left-aligned in output word */
	ADS9224R_DATA_ALIGNED_RIGHT		/*!< Data right-aligned in output word */
}ads9224r_data_aligned_t;

/**
 * @brief ADS9224R fixed pattern test mode state
 */
typedef enum
{
	ADS9224R_FPATTERN_STATE_DISABLED = 0,	/*!< Fixed pattern test mode disabled */
	ADS9224R_FPATTERN_STATE_ENABLED			/*!< Fixed pattern test mode enabled */
}ads9224r_fpattern_state_t;

/**
 * @brief ADS9224R data averaging configuration
 */
typedef enum
{
	ADS9224R_AVG_DISABLE = 0,		/*!< Data averaging disabled */
	ADS9224R_AVG_2,					/*!< Average 2 samples */
	ADS9224R_AVG_4					/*!< Average 4 samples */
}ads9224r_avg_t;

/**
 * @brief ADS9224R power down configuration structure
 */
typedef struct
{
	ads9224r_power_down_state_t pdRefBy2;	/*!< Reference by 2 power down state */
	ads9224r_power_down_state_t pdAdcB;		/*!< ADC channel B power down state */
	ads9224r_power_down_state_t pdAdcA;		/*!< ADC channel A power down state */
	ads9224r_power_down_state_t pdRef;		/*!< Reference power down state */
}ads9224r_power_down_config_t;

/**
 * @brief ADS9224R protocol configuration structure
 */
typedef struct
{
	ads9224r_sdo_protocol_t sdoProtocol;	/*!< SDO protocol selection */
	ads9224r_cpol_t			cPol;			/*!< SPI clock polarity */
	ads9224r_cpha_t			cpha;			/*!< SPI clock phase */
	ads9224r_sdo_width_t	busWidth;		/*!< SDO bus width configuration */
	ads9224r_crt_clk_t		crt;			/*!< CRT clock source selection */
}ads9224r_protocol_config_t;

/**
 * @brief ADS9224R complete device configuration structure
 */
typedef struct
{
	ads9224r_data_transfer_frame_t 	frame;			/*!< Data transfer frame configuration */
	ads9224r_power_down_config_t	pd;				/*!< Power down configuration */
	ads9224r_protocol_config_t		protocol;		/*!< Protocol configuration */
	uint32_t						samplingRate;	/*!< Sampling rate in Hz */
}ads9224r_config_t;

/**
 * @brief Buffer receive callback function pointer type
 * @param buffAddr: Buffer address containing received data
 */
typedef void (*bufferReceiveCallback)(uint32_t buffAddr);

/**
 * @}
 */

/**
 * @defgroup ADS9224R_PUBLIC_FUNCTIONS ADS9224R driver interface functions
 * @{
 */

/**
 * @brief	Initialize ADS9224R device with specified configuration
 * @param	ads9224r_config_t: Pointer to ADS9224R configuration structure. See ::ads9224r_config_t
 * @param	timeout: Timeout value in milliseconds for initialization
 * @retval	::ads9224r_status_t
 */
ads9224r_status_t ADS9224R_Init(ads9224r_config_t *ads9224r_config_t, uint32_t timeout);

/**
 * @brief	Set fixed pattern test mode state
 * @param	state: Fixed pattern state to set. See ::ads9224r_fpattern_state_t
 * @param	timeout: Timeout value in milliseconds for operation
 * @retval	::ads9224r_status_t
 */
ads9224r_status_t ADS9224R_SetPatternState(ads9224r_fpattern_state_t state, uint32_t timeout);

/**
 * @brief	Start data acquisition with dual buffer configuration
 * @param	sdoaBuffer0: Pointer to SDO-A buffer 0
 * @param	sdoaBuffer1: Pointer to SDO-A buffer 1
 * @param	sdobBuffer0: Pointer to SDO-B buffer 0
 * @param	sdobBuffer1: Pointer to SDO-B buffer 1
 * @param	size: Size of each buffer in bytes
 * @retval	::ads9224r_status_t
 */
ads9224r_status_t ADS9224R_StartAcquisiton(uint8_t* sdoaBuffer0, uint8_t* sdoaBuffer1, uint8_t* sdobBuffer0, uint8_t* sdobBuffer1, uint32_t size);

/**
 * @brief	Stop data acquisition
 * @retval	::ads9224r_status_t
 */
ads9224r_status_t ADS9224R_StopAcquisiton(void);

/**
 * @brief	Set device configuration
 * @param	config: Pointer to configuration structure. See ::ads9224r_config_t
 * @retval	::ads9224r_status_t
 */
ads9224r_status_t ADS9224R_SetConfig(ads9224r_config_t *config);

/**
 * @brief	Get current device configuration
 * @param	config: Pointer to configuration structure to fill. See ::ads9224r_config_t
 * @retval	::ads9224r_status_t
 */
ads9224r_status_t ADS9224R_GetConfig(ads9224r_config_t *config);

/**
 * @brief	Set sampling rate using timer configuration
 * @param	timPeriod: Timer period value
 * @param	timPrescaler: Timer prescaler value
 * @retval	::ads9224r_status_t
 */
ads9224r_status_t ADS9224R_SetSamplingRate(uint32_t timPeriod, uint32_t timPrescaler);

/**
 * @brief	Register callback function for buffer receive events
 * @param	callback: Callback function pointer. See ::bufferReceiveCallback
 * @param	trigger: SDO channel that triggers the callback. See ::ads9224r_sdo_t
 * @retval	::ads9224r_status_t
 */
ads9224r_status_t ADS9224R_RegisterCallback(bufferReceiveCallback callback, ads9224r_sdo_t trigger);

/**
 * @brief	Submit buffer address for DMA transfer
 * @param	buffAddr: Buffer memory address for data storage
 * @retval	::ads9224r_status_t
 */
ads9224r_status_t ADS9224R_SubmitBuffer(uint32_t buffAddr);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_DRIVERS_PLATFORM_ANALOGIN_ADS9224R_ADS9224R_H_ */