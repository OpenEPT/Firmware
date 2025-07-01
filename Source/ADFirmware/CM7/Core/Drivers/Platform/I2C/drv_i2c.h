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
}drv_i2c_initialization_status_t;

/**
 * @brief I2C driver return status
 */
typedef enum
{
	DRV_I2C_STATUS_OK,				/*!< I2C operation successful */
	DRV_I2C_STATUS_ERROR			/*!< I2C operation failed */
}drv_i2c_status_t;

/**
 * @brief Available I2C peripheral instances
 */
typedef enum
{
	DRV_I2C_INSTANCE_1 = 0			/*!< I2C peripheral instance 1 */
}drv_i2c_instance_t;

/**
 * @brief I2C configuration structure
 */
typedef struct
{
	uint32_t clkFreq;				/*!< I2C clock frequency in Hz */
}drv_i2c_config_t;

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
 * @brief	Transmit data to I2C slave device
 * @param	instance: I2C peripheral instance to use. See ::drv_i2c_instance_t
 * @param	addr: I2C slave device address (7-bit or 8-bit format)
 * @param	data: Pointer to data buffer to transmit
 * @param	size: Number of bytes to transmit
 * @param	timeout: Transmission timeout in milliseconds
 * @retval	::drv_i2c_status_t
 */
drv_i2c_status_t	DRV_I2C_Transmit(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size, uint32_t timeout);

/**
 * @brief	Receive data from I2C slave device
 * @param	instance: I2C peripheral instance to use. See ::drv_i2c_instance_t
 * @param	addr: I2C slave device address (7-bit or 8-bit format)
 * @param	data: Pointer to data buffer to store received data
 * @param	size: Number of bytes to receive
 * @param	timeout: Reception timeout in milliseconds
 * @retval	::drv_i2c_status_t
 */
drv_i2c_status_t	DRV_I2C_Receive(drv_i2c_instance_t instance, uint8_t addr, uint8_t* data, uint32_t size, uint32_t timeout);

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