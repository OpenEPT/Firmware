/**
 ******************************************************************************
 * @file   	drv_gpio.h
 * @brief  	...
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_GPIO_GPIO_H_
#define CORE_DRIVERS_PLATFORM_GPIO_GPIO_H_

#include <stdint.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "globalConfig.h"

#include "stm32h7xx_hal.h"

#define DRV_GPIO_PORT_MAX_NUMBER		CONF_GPIO_PORT_MAX_NUMBER
#define DRV_GPIO_PIN_MAX_NUMBER			CONF_GPIO_PIN_MAX_NUMBER
#define DRV_GPIO_INTERRUPTS_MAX_NUMBER	CONF_GPIO_INTERRUPTS_MAX_NUMBER

typedef enum
{
	DRV_GPIO_STATUS_OK,
	DRV_GPIO_STATUS_ERROR
}drv_gpio_status_t;

typedef  uint16_t	drv_gpio_pin;

typedef enum
{
	DRV_GPIO_INIT_STATUS_INIT,
	DRV_GPIO_INIT_STATUS_NOINIT
}drv_gpio_init_status_t;

typedef enum
{
	DRV_GPIO_PORT_A = 0,
	DRV_GPIO_PORT_B	= 1,
	DRV_GPIO_PORT_C	= 2,
	DRV_GPIO_PORT_D	= 3,
	DRV_GPIO_PORT_E	= 4,
	DRV_GPIO_PORT_F	= 5,
	DRV_GPIO_PORT_G	= 6,
	DRV_GPIO_PORT_H	= 7
}drv_gpio_port_t;



typedef enum
{
	DRV_GPIO_PORT_INIT_STATUS_UNINITIALIZED = 0,
	DRV_GPIO_PORT_INIT_STATUS_INITIALIZED
}drv_gpio_port_init_state_t;

typedef struct
{
	drv_gpio_port_t 			port;
	drv_gpio_port_init_state_t 	initState;
	SemaphoreHandle_t			lock;
	void*						portInstance;
}drv_gpio_port_handle_t;


typedef enum
{
	DRV_GPIO_PIN_STATE_RESET = 0,
	DRV_GPIO_PIN_STATE_SET = 1
}drv_gpio_pin_state_t;

typedef enum
{
	DRV_GPIO_PIN_MODE_INPUT,
	DRV_GPIO_PIN_MODE_OUTPUT_PP,
	DRV_GPIO_PIN_MODE_OUTPUT_OD,
	DRV_GPIO_PIN_MODE_AF_PP,
	DRV_GPIO_PIN_MODE_AF_OD,
	DRV_GPIO_PIN_MODE_ANALOG,
	DRV_GPIO_PIN_MODE_IT_RISING,
	DRV_GPIO_PIN_MODE_IT_FALLING,
	DRV_GPIO_PIN_MODE_IT_RISING_FALLING,
	DRV_GPIO_PIN_MODE_EVT_RISING,
	DRV_GPIO_PIN_MODE_EVT_FALLING,
	DRV_GPIO_PIN_MODE_EVT_RISING_FALLING
}drv_gpio_pin_mode_t;

typedef enum
{
	DRV_GPIO_PIN_PULL_NOPULL 	= 0,
	DRV_GPIO_PIN_PULL_DOWN		= 1,
	DRV_GPIO_PIN_PULL_UP		= 2
}drv_gpio_pin_pull_t;

typedef struct
{
	drv_gpio_pin_pull_t pullState;
	drv_gpio_pin_mode_t mode;

}drv_gpio_pin_init_conf_t;

typedef void (*drv_gpio_pin_isr_callback)(drv_gpio_pin);


drv_gpio_status_t 		DRV_GPIO_Init();
drv_gpio_status_t 		DRV_GPIO_Port_Init(drv_gpio_port_t port);
drv_gpio_status_t 		DRV_GPIO_Pin_Init(drv_gpio_port_t port, drv_gpio_pin pin, drv_gpio_pin_init_conf_t* conf);
drv_gpio_status_t 		DRV_GPIO_Pin_DeInit(drv_gpio_port_t port, drv_gpio_pin pin);
drv_gpio_status_t 		DRV_GPIO_Pin_SetState(drv_gpio_port_t port, drv_gpio_pin pin, drv_gpio_pin_state_t state);
drv_gpio_status_t 		DRV_GPIO_Pin_SetStateFromISR(drv_gpio_port_t port, drv_gpio_pin pin, drv_gpio_pin_state_t state);
drv_gpio_pin_state_t 	DRV_GPIO_Pin_ReadState(drv_gpio_port_t port, drv_gpio_pin pin);
drv_gpio_status_t 		DRV_GPIO_Pin_ToogleFromISR(drv_gpio_port_t port, drv_gpio_pin pin);
drv_gpio_status_t 		DRV_GPIO_Pin_EnableInt(drv_gpio_port_t port, drv_gpio_pin pin, uint32_t pri, drv_gpio_pin_isr_callback callback);
drv_gpio_status_t 		DRV_GPIO_RegisterCallback(drv_gpio_port_t port, drv_gpio_pin pin, drv_gpio_pin_isr_callback callback, uint32_t priority);
void 					DRV_GPIO_ClearInterruptFlag(uint16_t GPIO_Pin);

#endif /* CORE_DRIVERS_PLATFORM_GPIO_GPIO_H_ */
