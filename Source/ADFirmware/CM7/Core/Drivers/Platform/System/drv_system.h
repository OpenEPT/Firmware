/**
 ******************************************************************************
 * @file   	system.h
 * @brief  	...
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_SYSTEM_SYSTEM_H_
#define CORE_DRIVERS_PLATFORM_SYSTEM_SYSTEM_H_

typedef enum
{
	DRV_SYSTEM_STATUS_OK,
	DRV_SYSTEM_STATUS_ERROR
}drv_system_status_t;

drv_system_status_t	DRV_SYSTEM_InitCoreFunc();
drv_system_status_t	DRV_SYSTEM_InitDrivers();



#endif /* CORE_DRIVERS_PLATFORM_SYSTEM_SYSTEM_H_ */
