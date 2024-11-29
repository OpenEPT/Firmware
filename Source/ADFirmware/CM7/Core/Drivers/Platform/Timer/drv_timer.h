/*
 * timer.h
 *
 *  Created on: Oct 2, 2024
 *      Author: Haris
 */

#ifndef CORE_DRIVERS_PLATFORM_TIMER_DRV_TIMER_H_
#define CORE_DRIVERS_PLATFORM_TIMER_DRV_TIMER_H_

#include <stdint.h>


#include "FreeRTOS.h"
#include "semphr.h"

#include "globalConfig.h"

#include "stm32h7xx_hal.h"

#define DRV_TIMER_MAX_NUMBER_OF_TIMERS		CONF_DRV_TIMER_MAX_NUMBER_OF_TIMERS
#define DRV_TIMER_MAX_NUMBER_OF_CHANNELS	CONF_DRV_TIMER_MAX_NUMBER_OF_CHANNELS


typedef enum
{
	DRV_TIMER_STATUS_OK,
	DRV_TIMER_STATUS_ERROR
}drv_timer_status_t;

typedef enum
{
	DRV_TIMER_INIT_STATUS_NOINIT = 0,
	DRV_TIMER_INIT_STATUS_INIT
}drv_timer_init_status_t;

typedef enum
{
	DRV_TIMER_COUNTER_MODE_UP,
	DRV_TIMER_COUNTER_MODE_DOWN
}drv_timer_counter_mode_t;

typedef enum
{
	DRV_TIMER_DIV_1,
	DRV_TIMER_DIV_2,
	DRV_TIMER_DIV_4
}drv_timer_div_t;

typedef enum
{
	DRV_TIMER_PRELOAD_ENABLE,
	DRV_TIMER_PRELOAD_DISABLE
}drv_timer_preload_t;

typedef enum
{
	DRV_TIMER_CHANNEL_MODE_TIMMING,
	DRV_TIMER_CHANNEL_MODE_ACTIVE,
	DRV_TIMER_CHANNEL_MODE_INACTIVE,
	DRV_TIMER_CHANNEL_MODE_TOGGLE,
	DRV_TIMER_CHANNEL_MODE_PWM1
}drv_timer_channel_mode_t;

typedef enum
{
	DRV_TIMER_1 = 0,
	DRV_TIMER_2 = 1,
	DRV_TIMER_3 = 2,
	DRV_TIMER_4 = 3
}drv_timer_instance_t;

typedef enum
{
	DRV_TIMER_CHANNEL_1 = 0,
	DRV_TIMER_CHANNEL_2 = 1,
	DRV_TIMER_CHANNEL_3 = 2,
	DRV_TIMER_CHANNEL_4 = 3
}drv_timer_channel_t;

typedef struct
{
	uint16_t					prescaler;
	drv_timer_counter_mode_t	mode;
	uint16_t					period;
	drv_timer_div_t				div;
	drv_timer_preload_t			preload;
}drv_timer_config_t;

typedef struct
{
	drv_timer_channel_mode_t    mode;
}drv_timer_channel_config_t;

typedef struct
{
	drv_timer_channel_t			chId;
	drv_timer_channel_mode_t    mode;
	uint32_t					period;
	drv_timer_init_status_t     init;
}drv_timer_channel_handle_t;

typedef struct
{
	drv_timer_instance_t 	    timer;
	drv_timer_init_status_t 	initState;
	SemaphoreHandle_t			lock;
	void*						baseAddr;
	uint32_t					numberOfInitChannels;
	drv_timer_channel_handle_t	channels[DRV_TIMER_MAX_NUMBER_OF_CHANNELS];
}drv_timer_handle_t;

drv_timer_status_t DRV_Timer_Init();
drv_timer_status_t DRV_Timer_Init_Instance(drv_timer_instance_t instance, drv_timer_config_t* config);
drv_timer_status_t DRV_Timer_Channel_Init(drv_timer_instance_t instance, drv_timer_channel_t channel, drv_timer_channel_config_t* config);
drv_timer_status_t DRV_Timer_Channel_PWM_Start(drv_timer_instance_t instance, drv_timer_channel_t channel, uint32_t period, uint32_t timeout);



#endif /* CORE_DRIVERS_PLATFORM_TIMER_DRV_TIMER_H_ */
