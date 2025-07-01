/**
 ******************************************************************************
 * @file   	drv_system.h
 *
 * @brief  	System driver provides hardware abstraction layer for core system
 * 			initialization and management functions. This driver handles the
 * 			initialization of system core functionality including clock
 * 			configuration, peripheral setup, and driver subsystem initialization.
 * 			It serves as the primary entry point for system-wide hardware
 * 			initialization and provides centralized system status management.
 * 			All system driver interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_SYSTEM_SYSTEM_H_
#define CORE_DRIVERS_PLATFORM_SYSTEM_SYSTEM_H_

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup SYSTEM_DRIVER System Driver
 * @{
 */

/**
 * @defgroup SYSTEM_PUBLIC_TYPES System driver public data types
 * @{
 */

/**
 * @brief System driver return status
 */
typedef enum
{
	DRV_SYSTEM_STATUS_OK,			/*!< System operation successful */
	DRV_SYSTEM_STATUS_ERROR			/*!< System operation failed */
}drv_system_status_t;

/**
 * @}
 */

/**
 * @defgroup SYSTEM_PUBLIC_FUNCTIONS System driver interface functions
 * @{
 */

/**
 * @brief	Initialize core system functionality
 * @details	This function initializes the core system components including
 * 			system clock configuration, interrupt controller setup, and
 * 			essential hardware peripherals required for basic system operation.
 * 			Must be called before any other system functions.
 * @retval	::drv_system_status_t
 */
drv_system_status_t	DRV_SYSTEM_InitCoreFunc();

/**
 * @brief	Initialize all system drivers
 * @details	This function initializes all hardware driver subsystems including
 * 			peripheral drivers, communication interfaces, and I/O controllers.
 * 			Should be called after core system initialization is complete.
 * @retval	::drv_system_status_t
 */
drv_system_status_t	DRV_SYSTEM_InitDrivers();

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_DRIVERS_PLATFORM_SYSTEM_SYSTEM_H_ */