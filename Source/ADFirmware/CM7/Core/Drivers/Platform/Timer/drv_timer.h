/**
 ******************************************************************************
 * @file   	drv_timer.h
 *
 * @brief  	Timer driver provides hardware abstraction layer for STM32 timer
 * 			peripherals including TIM1-TIM4. This driver supports timer
 * 			initialization, channel configuration, PWM generation, timing
 * 			control, and multi-channel operation with thread-safe access.
 * 			The driver manages timer instances, channel modes, prescaler
 * 			settings, and provides real-time timer control for various
 * 			timing and PWM applications.
 * 			All timer driver interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	October 2024
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_TIMER_DRV_TIMER_H_
#define CORE_DRIVERS_PLATFORM_TIMER_DRV_TIMER_H_

#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "globalConfig.h"
#include "stm32h7xx_hal.h"

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup TIMER_DRIVER Timer Driver
 * @{
 */

/**
 * @defgroup TIMER_PUBLIC_DEFINES Timer driver public defines
 * @{
 */
#define DRV_TIMER_MAX_NUMBER_OF_TIMERS		CONF_DRV_TIMER_MAX_NUMBER_OF_TIMERS		/*!< Maximum number of timer instances */
#define DRV_TIMER_MAX_NUMBER_OF_CHANNELS	CONF_DRV_TIMER_MAX_NUMBER_OF_CHANNELS	/*!< Maximum number of channels per timer */
/**
 * @}
 */

/**
 * @defgroup TIMER_PUBLIC_TYPES Timer driver public data types
 * @{
 */

/**
 * @brief Timer driver return status
 */
typedef enum
{
	DRV_TIMER_STATUS_OK,			/*!< Timer operation successful */
	DRV_TIMER_STATUS_ERROR			/*!< Timer operation failed */
}drv_timer_status_t;

/**
 * @brief Timer initialization status
 */
typedef enum
{
	DRV_TIMER_INIT_STATUS_NOINIT = 0,	/*!< Timer not initialized */
	DRV_TIMER_INIT_STATUS_INIT			/*!< Timer initialized */
}drv_timer_init_status_t;

/**
 * @brief Timer counter mode
 */
typedef enum
{
	DRV_TIMER_COUNTER_MODE_UP,		/*!< Up counting mode */
	DRV_TIMER_COUNTER_MODE_DOWN		/*!< Down counting mode */
}drv_timer_counter_mode_t;

/**
 * @brief Timer clock division ratios
 */
typedef enum
{
	DRV_TIMER_DIV_1,				/*!< Clock not divided */
	DRV_TIMER_DIV_2,				/*!< Clock divided by 2 */
	DRV_TIMER_DIV_4					/*!< Clock divided by 4 */
}drv_timer_div_t;

/**
 * @brief Timer auto-reload preload enable
 */
typedef enum
{
	DRV_TIMER_PRELOAD_ENABLE,		/*!< Auto-reload preload enabled */
	DRV_TIMER_PRELOAD_DISABLE		/*!< Auto-reload preload disabled */
}drv_timer_preload_t;

/**
 * @brief Timer channel operating modes
 */
typedef enum
{
	DRV_TIMER_CHANNEL_MODE_TIMMING,		/*!< Timing mode - no output */
	DRV_TIMER_CHANNEL_MODE_ACTIVE,		/*!< Active level on match */
	DRV_TIMER_CHANNEL_MODE_INACTIVE,	/*!< Inactive level on match */
	DRV_TIMER_CHANNEL_MODE_TOGGLE,		/*!< Toggle output on match */
	DRV_TIMER_CHANNEL_MODE_PWM1			/*!< PWM mode 1 */
}drv_timer_channel_mode_t;

/**
 * @brief Available timer instances
 */
typedef enum
{
	DRV_TIMER_1 = 0,				/*!< Timer 1 instance */
	DRV_TIMER_2 = 1,				/*!< Timer 2 instance */
	DRV_TIMER_3 = 2,				/*!< Timer 3 instance */
	DRV_TIMER_4 = 3					/*!< Timer 4 instance */
}drv_timer_instance_t;

/**
 * @brief Timer channel identifiers
 */
typedef enum
{
	DRV_TIMER_CHANNEL_1 = 0,		/*!< Timer channel 1 */
	DRV_TIMER_CHANNEL_2 = 1,		/*!< Timer channel 2 */
	DRV_TIMER_CHANNEL_3 = 2,		/*!< Timer channel 3 */
	DRV_TIMER_CHANNEL_4 = 3			/*!< Timer channel 4 */
}drv_timer_channel_t;

/**
 * @brief Timer configuration structure
 */
typedef struct
{
	uint16_t					prescaler;		/*!< Timer prescaler value */
	drv_timer_counter_mode_t	mode;			/*!< Counter mode. See ::drv_timer_counter_mode_t */
	uint16_t					period;			/*!< Timer period (auto-reload value) */
	drv_timer_div_t				div;			/*!< Clock division ratio. See ::drv_timer_div_t */
	drv_timer_preload_t			preload;		/*!< Auto-reload preload setting. See ::drv_timer_preload_t */
}drv_timer_config_t;

/**
 * @brief Timer channel configuration structure
 */
typedef struct
{
	drv_timer_channel_mode_t    mode;			/*!< Channel operating mode. See ::drv_timer_channel_mode_t */
}drv_timer_channel_config_t;

/**
 * @brief Timer channel handle structure
 */
typedef struct
{
	drv_timer_channel_t			chId;			/*!< Channel identifier. See ::drv_timer_channel_t */
	drv_timer_channel_mode_t    mode;			/*!< Channel mode. See ::drv_timer_channel_mode_t */
	uint32_t					period;			/*!< Channel period value */
	drv_timer_init_status_t     init;			/*!< Channel initialization status. See ::drv_timer_init_status_t */
}drv_timer_channel_handle_t;

/**
 * @brief Timer instance handle structure
 */
typedef struct
{
	drv_timer_instance_t 	    timer;			/*!< Timer instance. See ::drv_timer_instance_t */
	drv_timer_init_status_t 	initState;		/*!< Timer initialization state. See ::drv_timer_init_status_t */
	SemaphoreHandle_t			lock;			/*!< Thread-safe access semaphore */
	void*						baseAddr;		/*!< Timer base address pointer */
	uint32_t					numberOfInitChannels;	/*!< Number of initialized channels */
	drv_timer_channel_handle_t	channels[DRV_TIMER_MAX_NUMBER_OF_CHANNELS];	/*!< Channel handles array */
}drv_timer_handle_t;

/**
 * @}
 */

/**
 * @defgroup TIMER_PUBLIC_FUNCTIONS Timer driver interface functions
 * @{
 */

/**
 * @brief	Initialize timer driver subsystem
 * @details	This function initializes the timer driver subsystem, sets up
 * 			internal data structures, and prepares the driver for timer
 * 			instance initialization. Must be called before any timer
 * 			instance operations.
 * @retval	::drv_timer_status_t
 */
drv_timer_status_t DRV_Timer_Init();

/**
 * @brief	Initialize specific timer instance
 * @details	This function initializes a specific timer instance with the
 * 			provided configuration including prescaler, period, counter mode,
 * 			and clock division settings. The timer is configured but not started.
 * @param	instance: Timer instance to initialize. See ::drv_timer_instance_t
 * @param	config: Pointer to timer configuration structure. See ::drv_timer_config_t
 * @retval	::drv_timer_status_t
 */
drv_timer_status_t DRV_Timer_Init_Instance(drv_timer_instance_t instance, drv_timer_config_t* config);

/**
 * @brief	Initialize timer channel
 * @details	This function initializes a specific channel of a timer instance
 * 			with the provided configuration. The channel is configured with
 * 			the specified operating mode but not activated.
 * @param	instance: Timer instance. See ::drv_timer_instance_t
 * @param	channel: Timer channel to initialize. See ::drv_timer_channel_t
 * @param	config: Pointer to channel configuration structure. See ::drv_timer_channel_config_t
 * @retval	::drv_timer_status_t
 */
drv_timer_status_t DRV_Timer_Channel_Init(drv_timer_instance_t instance, drv_timer_channel_t channel, drv_timer_channel_config_t* config);

/**
 * @brief	Start PWM generation on timer channel
 * @details	This function starts PWM generation on the specified timer channel
 * 			with the given period and timeout settings. The channel must be
 * 			previously initialized in PWM mode.
 * @param	instance: Timer instance. See ::drv_timer_instance_t
 * @param	channel: Timer channel. See ::drv_timer_channel_t
 * @param	period: PWM period value (compare register value)
 * @param	timeout: Operation timeout in milliseconds
 * @retval	::drv_timer_status_t
 */
drv_timer_status_t DRV_Timer_Channel_PWM_Start(drv_timer_instance_t instance, drv_timer_channel_t channel, uint32_t period, uint32_t timeout);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_DRIVERS_PLATFORM_TIMER_DRV_TIMER_H_ */