/**
 ******************************************************************************
 * @file   	drv_i2c.h
 *
 * @brief  	Inter-Integrated Circuit (I2C) driver provides hardware abstraction
 * 			layer for STM32 I2C peripherals. This driver supports multi-instance
 * 			I2C configuration, master mode communication, configurable clock
 * 			frequency, blocking transmit and receive operations with timeout
 * 			control for reliable communication with I2C slave devices in
 * 			embedded systems.
 * 			All I2C driver interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	April 2025
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_I2C_DRV_I2C_H_
#define CORE_DRIVERS_PLATFORM_I2C_DRV_I2C_H_

#include <stdint.h>
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
 * @defgroup I2C_PUBLIC_DEFINES I2C driver public defines
 * @{
 */
#define DRV_I2C_INSTANCES_MAX_NUMBER	(4U)	/*!< Maximum number of I2C instances supported */
/**
 * @}
 */

/**
 * @defgroup I2C_PUBLIC_TYPES I2C driver public data types
 * @{
 */

/**
 * @brief I2C driver initialization status
 */
typedef enum
{
	DRV_I2C_INITIALIZATION_STATUS_NOINIT	= 0,	/*!< I2C driver is not initialized */
	DRV_I2C_INITIALIZATION_STATUS_INIT		= 1		/*!< I2C driver is initialized */
} drv_i2c_initialization_status_t;

/**
 * @brief I2C driver return status
 */
typedef enum
{
	DRV_I2C_STATUS_OK,				/*!< I2C operation successful */
	DRV_I2C_STATUS_ERROR,			/*!< I2C operation failed */
	DRV_I2C_STATUS_BUSY				/*!< I2C peripheral or bus is busy */
} drv_i2c_status_t;

/**
 * @brief Available I2C peripheral instances
 * @note  Mapping in this implementation:
 *        - DRV_I2C_INSTANCE_1 -> I2C1
 *        - DRV_I2C_INSTANCE_2 -> I2C4
 */
typedef enum
{
	DRV_I2C_INSTANCE_1 = 0,			/*!< I2C peripheral instance 1 */
	DRV_I2C_INSTANCE_2 = 1,			/*!< I2C peripheral instance 1 */
	DRV_I2C_INSTANCE_4 = 3			/*!< I2C peripheral instance 2 */
} drv_i2c_instance_t;

/**
 * @brief I2C configuration structure
 */
typedef struct
{
	uint32_t clkFreq;				/*!< Reserved for future use */
} drv_i2c_config_t;

/**
 * @}
 */

/**
 * @defgroup I2C_PUBLIC_FUNCTIONS I2C driver interface functions
 * @{
 */

/**
 * @brief	Initialize I2C driver system
 * @retval	::drv_i2c_status_t
 */
drv_i2c_status_t	DRV_I2C_Init(void);

/**
 * @brief	Initialize specific I2C peripheral instance
 * @param	instance: I2C peripheral instance to initialize. See ::drv_i2c_instance_t
 * @param	config: Pointer to I2C configuration structure. See ::drv_i2c_config_t
 * @retval	::drv_i2c_status_t
 */
drv_i2c_status_t	DRV_I2C_Instance_Init(drv_i2c_instance_t instance, drv_i2c_config_t* config);

/**
 * @brief	Deinitialize specific I2C peripheral instance
 * @param	instance: I2C peripheral instance to deinitialize. See ::drv_i2c_instance_t
 * @retval	::drv_i2c_status_t
 */
drv_i2c_status_t	DRV_I2C_Instance_DeInit(drv_i2c_instance_t instance);

/**
 * @brief	Transmit data to I2C slave device
 * @param	instance: I2C peripheral instance to use. See ::drv_i2c_instance_t
 * @param	addr: I2C slave device address (7-bit address shifted left by 1 for HAL API)
 * @param	data: Pointer to data buffer to transmit
 * @param	size: Number of bytes to transmit
 * @param	timeout: Transmission timeout in milliseconds
 * @retval	::drv_i2c_status_t
 */
drv_i2c_status_t	DRV_I2C_Transmit(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size, uint32_t timeout);

/**
 * @brief	Receive data from I2C slave device
 * @param	instance: I2C peripheral instance to use. See ::drv_i2c_instance_t
 * @param	addr: I2C slave device address (7-bit address shifted left by 1 for HAL API)
 * @param	data: Pointer to data buffer to store received data
 * @param	size: Number of bytes to receive
 * @param	timeout: Reception timeout in milliseconds
 * @retval	::drv_i2c_status_t
 */
drv_i2c_status_t	DRV_I2C_Receive(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size, uint32_t timeout);


/**
 * @brief	Transmit data to I2C slave device using DMA
 * @param	instance: I2C peripheral instance to use. See ::drv_i2c_instance_t
 * @param	addr: I2C slave device address (7-bit address shifted left by 1 for HAL API)
 * @param	data: Pointer to data buffer to transmit
 * @param	size: Number of bytes to transmit
 * @retval	::drv_i2c_status_t
 */
drv_i2c_status_t 	DRV_I2C_TransmitDMA(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size);


/**
 * @brief   Prepare continuous TX DMA and timer-triggered I2C transactions
 * @param   instance: I2C peripheral instance
 * @param   addr: I2C slave address shifted left by one
 * @param   data: Complete serialized TX buffer
 * @param   size: Total number of bytes in TX buffer
 * @param   transferSize: Number of bytes transmitted per timer trigger
 * @retval  ::drv_i2c_status_t
 */
drv_i2c_status_t DRV_I2C_TransmitTriggeredDMA(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size, uint8_t transferSize);

drv_i2c_status_t DRV_I2C_IsTriggeredDMAComplete(drv_i2c_instance_t instance, uint8_t* complete);

/**
 * @brief	Abort active I2C DMA transmission
 * @param	instance: I2C peripheral instance to use. See ::drv_i2c_instance_t
 * @retval	::drv_i2c_status_t
 */
/**
 * @brief	Wait until I2C peripheral and bus become idle
 * @param	instance: I2C peripheral instance to use. See ::drv_i2c_instance_t
 * @param	timeout: Maximum time to wait in milliseconds
 * @retval	::DRV_I2C_STATUS_OK if idle, ::DRV_I2C_STATUS_BUSY on timeout
 */
drv_i2c_status_t 	DRV_I2C_WaitIdle(drv_i2c_instance_t instance, uint32_t timeout);

drv_i2c_status_t 	DRV_I2C_AbortDMA(drv_i2c_instance_t instance);

typedef void (*drv_i2c_tx_dma_complete_callback_t)(void);

drv_i2c_status_t DRV_I2C_RegisterTxDMACompleteCallback(drv_i2c_instance_t instance, drv_i2c_tx_dma_complete_callback_t callback);


/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_DRIVERS_PLATFORM_I2C_DRV_I2C_H_ */
