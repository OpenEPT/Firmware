/**
 ******************************************************************************
 * @file   	drv_spi.h
 *
 * @brief  	Serial Peripheral Interface (SPI) driver provides hardware
 * 			abstraction layer for STM32 SPI peripherals. This driver supports
 * 			multi-instance SPI configuration, master and slave modes,
 * 			configurable clock polarity and phase, data size selection,
 * 			blocking and interrupt-driven communication with timeout control,
 * 			and callback mechanisms for real-time SPI data exchange in
 * 			embedded systems.
 * 			All SPI driver interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Pavle Lakic & Dimitrije Lilic
 * @date	June 2024
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_SPI_DRV_SPI_H_
#define CORE_DRIVERS_PLATFORM_SPI_DRV_SPI_H_

#include <stdint.h>
#include "globalConfig.h"

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup SPI_DRIVER SPI Driver
 * @{
 */

/**
 * @defgroup SPI_PUBLIC_DEFINES SPI driver public defines
 * @{
 */
#define DRV_SPI_INSTANCES_MAX_NUMBER	CONF_SPI_INSTANCES_MAX_NUMBER	/*!< Maximum number of SPI instances */
/**
 * @}
 */

/**
 * @defgroup SPI_PUBLIC_TYPES SPI driver public data types
 * @{
 */

/**
 * @brief SPI driver initialization status
 */
typedef enum drv_spi_initialization_status_t
{
	DRV_SPI_INITIALIZATION_STATUS_NOINIT	= 0,	/*!< SPI driver is not initialized */
	DRV_SPI_INITIALIZATION_STATUS_INIT		= 1		/*!< SPI driver is initialized */
}drv_spi_initialization_status_t;

/**
 * @brief SPI driver return status
 */
typedef enum drv_spi_status_t
{
	DRV_SPI_STATUS_OK,				/*!< SPI operation successful */
	DRV_SPI_STATUS_ERROR			/*!< SPI operation failed */
}drv_spi_status_t;

/**
 * @brief Available SPI peripheral instances
 */
typedef enum drv_spi_instance_t
{
	DRV_SPI_INSTANCE3 = 0,			/*!< SPI peripheral instance 3 */
	DRV_SPI_INSTANCE2 = 1			/*!< SPI peripheral instance 2 */
}drv_spi_instance_t;

/**
 * @brief SPI operating modes
 */
typedef enum drv_spi_mode_t
{
	DRV_SPI_MODE_SLAVE = 0,			/*!< SPI slave mode */
	DRV_SPI_MODE_MASTER = 1			/*!< SPI master mode */
}drv_spi_mode_t;

/**
 * @brief SPI communication directions
 */
typedef enum drv_spi_direction_t
{
	DRV_SPI_DIRECTION_2LINES = 0,			/*!< Full duplex (2 lines) */
	DRV_SPI_DIRECTION_2LINES_TXONLY = 1,	/*!< Transmit only (2 lines) */
	DRV_SPI_DIRECTION_2LINES_RXONLY = 2,	/*!< Receive only (2 lines) */
	DRV_SPI_DIRECTION_1LINE = 3				/*!< Half duplex (1 line) */
}drv_spi_direction_t;

/**
 * @brief SPI data frame sizes
 */
typedef enum drv_spi_size_t
{
	DRV_SPI_DATA_SIZE_8BITS = 0,	/*!< 8-bit data frame */
	DRV_SPI_DATA_SIZE_16BITS = 1	/*!< 16-bit data frame */
}drv_spi_size_t;

/**
 * @brief SPI clock polarity settings
 */
typedef enum drv_spi_polarity_t
{
	DRV_SPI_POLARITY_LOW = 0,		/*!< Clock polarity low (idle state) */
	DRV_SPI_POLARITY_HIGH = 1		/*!< Clock polarity high (idle state) */
}drv_spi_polarity_t;

/**
 * @brief SPI clock phase settings
 */
typedef enum drv_spi_phase_t
{
	DRV_SPI_PHASE_1EDGE = 0,		/*!< Data capture on first clock edge */
	DRV_SPI_PHASE_2EDGE = 1			/*!< Data capture on second clock edge */
}drv_spi_phase_t;

/**
 * @brief SPI clock prescaler values
 */
typedef enum drv_spi_clock_prescaler_t
{
	DRV_SPI_BAUDRATEPRESCALER_2 = 0,	/*!< Clock divided by 2 */
	DRV_SPI_BAUDRATEPRESCALER_4 = 1,	/*!< Clock divided by 4 */
	DRV_SPI_BAUDRATEPRESCALER_8 = 2,	/*!< Clock divided by 8 */
	DRV_SPI_BAUDRATEPRESCALER_16 = 3,	/*!< Clock divided by 16 */
	DRV_SPI_BAUDRATEPRESCALER_32 = 4,	/*!< Clock divided by 32 */
	DRV_SPI_BAUDRATEPRESCALER_64 = 5,	/*!< Clock divided by 64 */
	DRV_SPI_BAUDRATEPRESCALER_128 = 6,	/*!< Clock divided by 128 */
	DRV_SPI_BAUDRATEPRESCALER_256 = 7	/*!< Clock divided by 256 */
}drv_spi_clock_prescaler_t;

/**
 * @brief SPI bit transmission order
 */
typedef enum drv_spi_first_bit_t
{
	DRV_SPI_FIRSTBIT_MSB = 0,		/*!< Most significant bit first */
	DRV_SPI_FIRSTBIT_LSB = 1		/*!< Least significant bit first */
}drv_spi_first_bit_t;

/**
 * @brief SPI FIFO threshold settings
 */
typedef enum drv_spi_fifo_t
{
	DRV_SPI_FIFO_THRESHOLD_01DATA = 0	/*!< FIFO threshold for 1 data */
}drv_spi_fifo_t;

/**
 * @brief SPI configuration structure
 */
typedef struct drv_spi_config_t
{
	drv_spi_mode_t		mode;		/*!< SPI operating mode */
	drv_spi_polarity_t	polarity;	/*!< Clock polarity setting */
	drv_spi_phase_t		phase;		/*!< Clock phase setting */
}drv_spi_config_t;

/**
 * @brief SPI receive interrupt callback function pointer type
 * @param data: Pointer to received data buffer
 */
typedef void (*drv_spi_rx_isr_callback)(uint8_t* data);

/**
 * @}
 */

/**
 * @defgroup SPI_PUBLIC_FUNCTIONS SPI driver interface functions
 * @{
 */

/**
 * @brief	Initialize SPI driver system
 * @retval	::drv_spi_status_t
 */
drv_spi_status_t	DRV_SPI_Init();

/**
 * @brief	Initialize specific SPI peripheral instance
 * @param	instance: SPI peripheral instance to initialize. See ::drv_spi_instance_t
 * @param	config: Pointer to SPI configuration structure. See ::drv_spi_config_t
 * @retval	::drv_spi_status_t
 */
drv_spi_status_t	DRV_SPI_Instance_Init(drv_spi_instance_t instance, drv_spi_config_t* config);

/**
 * @brief	Deinitialize specific SPI peripheral instance
 * @param	instance: SPI peripheral instance to deinitialize. See ::drv_spi_instance_t
 * @retval	::drv_spi_status_t
 */
drv_spi_status_t	DRV_SPI_Instance_DeInit(drv_spi_instance_t instance);

/**
 * @brief	Transmit data via SPI in blocking mode
 * @param	instance: SPI peripheral instance to use. See ::drv_spi_instance_t
 * @param	buffer: Pointer to data buffer to transmit
 * @param	size: Number of bytes to transmit
 * @param	timeout: Transmission timeout in milliseconds
 * @retval	::drv_spi_status_t
 */
drv_spi_status_t	DRV_SPI_TransmitData(drv_spi_instance_t instance, uint8_t* buffer, uint8_t size, uint32_t timeout);

/**
 * @brief	Receive data via SPI in blocking mode
 * @param	instance: SPI peripheral instance to use. See ::drv_spi_instance_t
 * @param	buffer: Pointer to data buffer to store received data
 * @param	size: Number of bytes to receive
 * @param	timeout: Reception timeout in milliseconds
 * @retval	::drv_spi_status_t
 */
drv_spi_status_t	DRV_SPI_ReceiveData(drv_spi_instance_t instance, uint8_t* buffer, uint8_t size, uint32_t timeout);

/**
 * @brief	Register receive interrupt callback function
 * @param	instance: SPI peripheral instance. See ::drv_spi_instance_t
 * @param	rxcb: Receive callback function pointer. See ::drv_spi_rx_isr_callback
 * @retval	::drv_spi_status_t
 */
drv_spi_status_t	DRV_SPI_Instance_RegisterRxCallback(drv_spi_instance_t instance, drv_spi_rx_isr_callback rxcb);

/**
 * @brief	Enable interrupt-driven data transfer
 * @param	instance: SPI peripheral instance. See ::drv_spi_instance_t
 * @param	input: Pointer to input data buffer
 * @param	output: Pointer to output data buffer
 * @param	size: Number of bytes to transfer
 * @retval	::drv_spi_status_t
 */
drv_spi_status_t	DRV_SPI_EnableITData(drv_spi_instance_t instance, uint8_t* input, uint8_t* output, uint32_t size);

/**
 * @}
 */

/**
 * @defgroup SPI_HAL_FUNCTIONS SPI HAL initialization functions
 * @{
 */

/**
 * @brief	Initialize SPI4 peripheral using HAL
 */
void MX_SPI4_Init(void);

/**
 * @brief	Initialize SPI5 peripheral using HAL
 */
void MX_SPI5_Init(void);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_DRIVERS_PLATFORM_SPI_DRV_SPI_H_ */