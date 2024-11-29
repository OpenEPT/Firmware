/*
 * analogout.h
 *
 *  Created on: Nov 5, 2023
 *      Author: Haris
 */

#ifndef CORE_DRIVERS_PLATFORM_ANALOGOUT_DRV_AOUTH_
#define CORE_DRIVERS_PLATFORM_ANALOGOUT_DRV_AOUTH_
#include <stdint.h>

typedef enum
{
	DRV_AOUT_STATUS_OK,
	DRV_AOUT_STATUS_ERROR,
}drv_aout_status_t;

typedef enum
{
	DRV_AOUT_ACTIVE_STATUS_DISABLED = 0,
	DRV_AOUT_ACTIVE_STATUS_ENABLED
}drv_aout_active_status;


drv_aout_status_t DRV_AOUT_Init();
drv_aout_status_t DRV_AOUT_SetEnable(drv_aout_active_status aStatus);
drv_aout_status_t DRV_AOUT_SetValue(uint32_t value);


#endif /* CORE_DRIVERS_PLATFORM_ANALOGOUT_DRV_AOUTH_ */
