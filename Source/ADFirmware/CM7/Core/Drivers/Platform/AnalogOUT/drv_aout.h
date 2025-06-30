/**
 ******************************************************************************
 * @file   	drv_aout.h
 *
 * @brief  	Analog Output (AOUT) driver provides hardware abstraction layer
 * 			for output enable/disable functionality, and vaule manipulation.
 * 			All AOUT driver interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2023
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_ANALOGOUT_DRV_AOUTH_
#define CORE_DRIVERS_PLATFORM_ANALOGOUT_DRV_AOUTH_

#include <stdint.h>

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup AOUT_DRIVER Analog Output Driver
 * @{
 */

/**
 * @defgroup AOUT_PUBLIC_TYPES AOUT driver public data types
 * @{
 */

/**
 * @brief AOUT driver return status
 */
typedef enum
{
	DRV_AOUT_STATUS_OK,				/*!< AOUT operation successful */
	DRV_AOUT_STATUS_ERROR			/*!< AOUT operation failed */
}drv_aout_status_t;

/**
 * @brief AOUT output enable/disable status
 */
typedef enum
{
	DRV_AOUT_ACTIVE_STATUS_DISABLED = 0,	/*!< Analog output disabled */
	DRV_AOUT_ACTIVE_STATUS_ENABLED			/*!< Analog output enabled */
}drv_aout_active_status_t;

/**
 * @}
 */

/**
 * @defgroup AOUT_PUBLIC_FUNCTIONS AOUT driver interface functions  
 * @{
 */

/**
 * @brief	Initialize analog output peripheral
 * @retval	::drv_aout_status_t
 */
drv_aout_status_t DRV_AOUT_Init(void);

/**
 * @brief	Enable or disable analog output
 * @param	aStatus: Output enable status. See ::drv_aout_active_status_t
 * @retval	::drv_aout_status_t
 */
drv_aout_status_t DRV_AOUT_SetEnable(drv_aout_active_status_t aStatus);

/**
 * @brief	Set analog output value
 * @param	value: Value
 * @retval	::drv_aout_status_t
 */
drv_aout_status_t DRV_AOUT_SetValue(uint32_t value);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_DRIVERS_PLATFORM_ANALOGOUT_DRV_AOUTH_ */