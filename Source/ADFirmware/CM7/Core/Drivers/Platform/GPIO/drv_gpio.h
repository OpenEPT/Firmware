/**
 ******************************************************************************
 * @file   	drv_gpio.h
 *
 * @brief  	General Purpose Input/Output (GPIO) driver provides hardware
 * 			abstraction layer for STM32H7 GPIO peripherals. This driver
 * 			supports multi-port GPIO configuration, pin state control,
 * 			interrupt handling, thread-safe operations with FreeRTOS
 * 			semaphores, and real-time GPIO manipulation with ISR-safe
 * 			functions for embedded system pin control and monitoring.
 * 			All GPIO driver interface functions, defines, and types are
 * 			declared in this header file.
 *
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

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup GPIO_DRIVER GPIO Driver
 * @{
 */

/**
 * @defgroup GPIO_PUBLIC_DEFINES GPIO driver public defines
 * @{
 */
#define DRV_GPIO_PORT_MAX_NUMBER		CONF_GPIO_PORT_MAX_NUMBER		/*!< Maximum number of GPIO ports */
#define DRV_GPIO_PIN_MAX_NUMBER			CONF_GPIO_PIN_MAX_NUMBER		/*!< Maximum number of GPIO pins per port */
#define DRV_GPIO_INTERRUPTS_MAX_NUMBER	CONF_GPIO_INTERRUPTS_MAX_NUMBER	/*!< Maximum number of GPIO interrupts */
/**
 * @}
 */

/**
 * @defgroup GPIO_PUBLIC_TYPES GPIO driver public data types
 * @{
 */

/**
 * @brief GPIO driver return status
 */
typedef enum
{
	DRV_GPIO_STATUS_OK,				/*!< GPIO operation successful */
	DRV_GPIO_STATUS_ERROR			/*!< GPIO operation failed */
}drv_gpio_status_t;

/**
 * @brief GPIO pin identifier type
 */
typedef uint16_t drv_gpio_pin;

/**
 * @brief GPIO driver initialization status
 */
typedef enum
{
	DRV_GPIO_INIT_STATUS_INIT,		/*!< GPIO driver is initialized */
	DRV_GPIO_INIT_STATUS_NOINIT		/*!< GPIO driver is not initialized */
}drv_gpio_init_status_t;

/**
 * @brief Available GPIO ports
 */
typedef enum
{
	DRV_GPIO_PORT_A = 0,			/*!< GPIO Port A */
	DRV_GPIO_PORT_B	= 1,			/*!< GPIO Port B */
	DRV_GPIO_PORT_C	= 2,			/*!< GPIO Port C */
	DRV_GPIO_PORT_D	= 3,			/*!< GPIO Port D */
	DRV_GPIO_PORT_E	= 4,			/*!< GPIO Port E */
	DRV_GPIO_PORT_F	= 5,			/*!< GPIO Port F */
	DRV_GPIO_PORT_G	= 6,			/*!< GPIO Port G */
	DRV_GPIO_PORT_H	= 7				/*!< GPIO Port H */
}drv_gpio_port_t;

/**
 * @brief GPIO port initialization state
 */
typedef enum
{
	DRV_GPIO_PORT_INIT_STATUS_UNINITIALIZED = 0,	/*!< GPIO port is uninitialized */
	DRV_GPIO_PORT_INIT_STATUS_INITIALIZED			/*!< GPIO port is initialized */
}drv_gpio_port_init_state_t;

/**
 * @brief GPIO port handle structure
 */
typedef struct
{
	drv_gpio_port_t 			port;			/*!< GPIO port identifier */
	drv_gpio_port_init_state_t 	initState;		/*!< Port initialization state */
	SemaphoreHandle_t			lock;			/*!< FreeRTOS semaphore for thread safety */
	void*						portInstance;	/*!< Pointer to HAL GPIO port instance */
}drv_gpio_port_handle_t;

/**
 * @brief GPIO pin logic states
 */
typedef enum
{
	DRV_GPIO_PIN_STATE_RESET = 0,	/*!< Pin logic level low (0V) */
	DRV_GPIO_PIN_STATE_SET = 1		/*!< Pin logic level high (VDD) */
}drv_gpio_pin_state_t;

/**
 * @brief GPIO pin operating modes
 */
typedef enum
{
	DRV_GPIO_PIN_MODE_INPUT,				/*!< Input mode (floating) */
	DRV_GPIO_PIN_MODE_OUTPUT_PP,			/*!< Output push-pull mode */
	DRV_GPIO_PIN_MODE_OUTPUT_OD,			/*!< Output open-drain mode */
	DRV_GPIO_PIN_MODE_AF_PP,				/*!< Alternate function push-pull mode */
	DRV_GPIO_PIN_MODE_AF_OD,				/*!< Alternate function open-drain mode */
	DRV_GPIO_PIN_MODE_ANALOG,				/*!< Analog mode */
	DRV_GPIO_PIN_MODE_IT_RISING,			/*!< Interrupt on rising edge */
	DRV_GPIO_PIN_MODE_IT_FALLING,			/*!< Interrupt on falling edge */
	DRV_GPIO_PIN_MODE_IT_RISING_FALLING,	/*!< Interrupt on both edges */
	DRV_GPIO_PIN_MODE_EVT_RISING,			/*!< Event on rising edge */
	DRV_GPIO_PIN_MODE_EVT_FALLING,			/*!< Event on falling edge */
	DRV_GPIO_PIN_MODE_EVT_RISING_FALLING	/*!< Event on both edges */
}drv_gpio_pin_mode_t;

/**
 * @brief GPIO pin pull-up/pull-down configuration
 */
typedef enum
{
	DRV_GPIO_PIN_PULL_NOPULL 	= 0,	/*!< No pull-up or pull-down */
	DRV_GPIO_PIN_PULL_DOWN		= 1,	/*!< Pull-down resistor enabled */
	DRV_GPIO_PIN_PULL_UP		= 2		/*!< Pull-up resistor enabled */
}drv_gpio_pin_pull_t;

/**
 * @brief GPIO pin initialization configuration structure
 */
typedef struct
{
	drv_gpio_pin_pull_t pullState;		/*!< Pin pull-up/pull-down configuration */
	drv_gpio_pin_mode_t mode;			/*!< Pin operating mode */
}drv_gpio_pin_init_conf_t;

/**
 * @brief GPIO pin interrupt callback function pointer type
 * @param pin: GPIO pin that triggered the interrupt
 */
typedef void (*drv_gpio_pin_isr_callback)(drv_gpio_pin pin);

/**
 * @}
 */

/**
 * @defgroup GPIO_PUBLIC_FUNCTIONS GPIO driver interface functions
 * @{
 */

/**
 * @brief	Initialize GPIO driver system
 * @retval	::drv_gpio_status_t
 */
drv_gpio_status_t DRV_GPIO_Init(void);

/**
 * @brief	Initialize specific GPIO port
 * @param	port: GPIO port to initialize. See ::drv_gpio_port_t
 * @retval	::drv_gpio_status_t
 */
drv_gpio_status_t DRV_GPIO_Port_Init(drv_gpio_port_t port);

/**
 * @brief	Initialize specific GPIO pin with configuration
 * @param	port: GPIO port containing the pin. See ::drv_gpio_port_t
 * @param	pin: GPIO pin number to initialize. See ::drv_gpio_pin
 * @param	conf: Pointer to pin configuration structure. See ::drv_gpio_pin_init_conf_t
 * @retval	::drv_gpio_status_t
 */
drv_gpio_status_t DRV_GPIO_Pin_Init(drv_gpio_port_t port, drv_gpio_pin pin, drv_gpio_pin_init_conf_t* conf);

/**
 * @brief	Deinitialize specific GPIO pin
 * @param	port: GPIO port containing the pin. See ::drv_gpio_port_t
 * @param	pin: GPIO pin number to deinitialize. See ::drv_gpio_pin
 * @retval	::drv_gpio_status_t
 */
drv_gpio_status_t DRV_GPIO_Pin_DeInit(drv_gpio_port_t port, drv_gpio_pin pin);

/**
 * @brief	Set GPIO pin output state
 * @param	port: GPIO port containing the pin. See ::drv_gpio_port_t
 * @param	pin: GPIO pin number to control. See ::drv_gpio_pin
 * @param	state: Pin state to set. See ::drv_gpio_pin_state_t
 * @retval	::drv_gpio_status_t
 */
drv_gpio_status_t DRV_GPIO_Pin_SetState(drv_gpio_port_t port, drv_gpio_pin pin, drv_gpio_pin_state_t state);

/**
 * @brief	Set GPIO pin output state from interrupt service routine
 * @param	port: GPIO port containing the pin. See ::drv_gpio_port_t
 * @param	pin: GPIO pin number to control. See ::drv_gpio_pin
 * @param	state: Pin state to set. See ::drv_gpio_pin_state_t
 * @retval	::drv_gpio_status_t
 */
drv_gpio_status_t DRV_GPIO_Pin_SetStateFromISR(drv_gpio_port_t port, drv_gpio_pin pin, drv_gpio_pin_state_t state);

/**
 * @brief	Read current GPIO pin input state
 * @param	port: GPIO port containing the pin. See ::drv_gpio_port_t
 * @param	pin: GPIO pin number to read. See ::drv_gpio_pin
 * @retval	::drv_gpio_pin_state_t
 */
drv_gpio_pin_state_t DRV_GPIO_Pin_ReadState(drv_gpio_port_t port, drv_gpio_pin pin);

/**
 * @brief	Toggle GPIO pin output state from interrupt service routine
 * @param	port: GPIO port containing the pin. See ::drv_gpio_port_t
 * @param	pin: GPIO pin number to toggle. See ::drv_gpio_pin
 * @retval	::drv_gpio_status_t
 */
drv_gpio_status_t DRV_GPIO_Pin_ToggleFromISR(drv_gpio_port_t port, drv_gpio_pin pin);

/**
 * @brief	Enable GPIO pin interrupt with callback registration
 * @param	port: GPIO port containing the pin. See ::drv_gpio_port_t
 * @param	pin: GPIO pin number to enable interrupt. See ::drv_gpio_pin
 * @param	pri: Interrupt priority level
 * @param	callback: Interrupt callback function. See ::drv_gpio_pin_isr_callback
 * @retval	::drv_gpio_status_t
 */
drv_gpio_status_t DRV_GPIO_Pin_EnableInt(drv_gpio_port_t port, drv_gpio_pin pin, uint32_t pri, drv_gpio_pin_isr_callback callback);

/**
 * @brief	Register interrupt callback function for GPIO pin
 * @param	port: GPIO port containing the pin. See ::drv_gpio_port_t
 * @param	pin: GPIO pin number. See ::drv_gpio_pin
 * @param	callback: Interrupt callback function. See ::drv_gpio_pin_isr_callback
 * @param	priority: Interrupt priority level
 * @retval	::drv_gpio_status_t
 */
drv_gpio_status_t DRV_GPIO_RegisterCallback(drv_gpio_port_t port, drv_gpio_pin pin, drv_gpio_pin_isr_callback callback, uint32_t priority);

/**
 * @brief	Clear GPIO interrupt flag for specific pin
 * @param	GPIO_Pin: GPIO pin mask to clear interrupt flag
 */
void DRV_GPIO_ClearInterruptFlag(uint16_t GPIO_Pin);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_DRIVERS_PLATFORM_GPIO_GPIO_H_ */