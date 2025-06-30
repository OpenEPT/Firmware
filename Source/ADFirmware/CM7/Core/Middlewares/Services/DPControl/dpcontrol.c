/**
 ******************************************************************************
 * @file    dpcontrol.c
 *
 * @brief   Discharge Profile Control (DPControl) service is responsible for
 *          managing output DAC values, enabling/disabling load and battery
 *          lines, controlling the power path, and monitoring protection events
 *          such as under-voltage, over-voltage, and over-current conditions.
 *          The service runs as a FreeRTOS task and uses GPIO and DAC drivers
 *          for interacting with hardware.
 *
 * @author  Haris Turkmanovic
 * @date    November 2023
 ******************************************************************************
 */

#include <stdarg.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "dpcontrol.h"
#include "logging.h"
#include "system.h"
#include "control.h"
#include "drv_aout.h"
#include "drv_gpio.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup DPCONTROL_SERVICE DPControl Service
 * @{
 */

/**
 * @defgroup DPCONTROL_DEFINES DPControl internal defines
 * @{
 */
#define DPCONTROL_MASK_SET_VALUE              0x00000001 /**< Set DAC value */
#define DPCONTROL_MASK_SET_ACTIVE_STATUS      0x00000002 /**< Set DAC active status */
#define DPCONTROL_MASK_SET_LOAD_STATE         0x00000004 /**< Set load state */
#define DPCONTROL_MASK_SET_BAT_STATE          0x00000008 /**< Set battery state */
#define DPCONTROL_MASK_SET_PPATH_STATE        0x00000010 /**< Set power path state */
#define DPCONTROL_MASK_SET_UV_DETECTED        0x00000020 /**< Under-voltage detected */
#define DPCONTROL_MASK_TRGER_LATCH            0x00000040 /**< Trigger latch pin */
#define DPCONTROL_MASK_SET_OV_DETECTED        0x00000080 /**< Over-voltage detected */
#define DPCONTROL_MASK_SET_OC_DETECTED        0x00000100 /**< Over-current detected */
/**
 * @}
 */

/**
 * @defgroup DPCONTROL_STRUCTURES DPControl internal structures
 * @{
 */

/**
 * @brief Structure for DAC configuration data
 */
typedef struct
{
    uint32_t data;                     /**< DAC value */
    dpcontrol_dac_status_t active;    /**< DAC enable/disable status */
} dpcontrol_aout_data_t;

/**
 * @brief Internal data structure for DPControl service
 */
typedef struct
{
    dpcontrol_state_t state;                      /**< Current task state */
    QueueHandle_t txMsgQueue;                     /**< Message queue (unused in this file) */
    SemaphoreHandle_t initSig;                    /**< Semaphore to signal initialization complete */
    SemaphoreHandle_t guard;                      /**< Mutex for shared data protection */
    TaskHandle_t taskHandle;                      /**< Handle to the FreeRTOS task */
    dpcontrol_aout_data_t aoutData;               /**< DAC value and active status */
    dpcontrol_load_state_t loadState;             /**< Load state */
    dpcontrol_bat_state_t batState;               /**< Battery state */
    dpcontrol_ppath_state_t pathState;            /**< Power path state */
    dpcontrol_protection_state_t underVoltage;    /**< Under-voltage protection flag */
    dpcontrol_protection_state_t overVoltage;     /**< Over-voltage protection flag */
    dpcontrol_protection_state_t overCurrent;     /**< Over-current protection flag */
} dpcontrol_data_t;
/**
 * @}
 */

/**
 * @defgroup DPCONTROL_PRIVATE_DATA DPControl private data
 * @{
 */
/**
 * @brief Static instance of the DPControl service data
 */
static dpcontrol_data_t prvDPCONTROL_DATA;
/**
 * @}
 */
/**
 * @defgroup DPCONTROL_PRIVATE_FUNCTIONS DPControl private functions
 * @{
 */
/**
 * @brief GPIO interrupt callback for Under Voltage detection.
 *
 * Triggered on rising/falling edge of the under-voltage GPIO. Notifies the main task
 * using `xTaskNotifyFromISR` with `DPCONTROL_MASK_SET_UV_DETECTED` flag.
 *
 * @param[in] pin GPIO pin that caused the interrupt
 */
static void prvDPCONTROL_UnderVoltageCB(drv_gpio_pin pin)
{
	BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

	xTaskNotifyFromISR(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_UV_DETECTED, eSetBits, &pxHigherPriorityTaskWoken);

	portYIELD_FROM_ISR( pxHigherPriorityTaskWoken );
}

/**
 * @brief GPIO interrupt callback for Over Voltage detection.
 *
 * Triggered on rising/falling edge of the over-voltage GPIO. Notifies the main task
 * using `xTaskNotifyFromISR` with `DPCONTROL_MASK_SET_OV_DETECTED` flag.
 *
 * @param[in] pin GPIO pin that caused the interrupt
 */
static void prvDPCONTROL_OverVoltageCB(drv_gpio_pin pin)
{
	BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

	xTaskNotifyFromISR(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_OV_DETECTED, eSetBits, &pxHigherPriorityTaskWoken);

	portYIELD_FROM_ISR( pxHigherPriorityTaskWoken );
}

/**
 * @brief GPIO interrupt callback for Over Current detection.
 *
 * Triggered on rising/falling edge of the over-current GPIO. Notifies the main task
 * using `xTaskNotifyFromISR` with `DPCONTROL_MASK_SET_OC_DETECTED` flag.
 *
 * @param[in] pin GPIO pin that caused the interrupt
 */
static void prvDPCONTROL_OverCurrentCB(drv_gpio_pin pin)
{
	BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

	xTaskNotifyFromISR(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_OC_DETECTED, eSetBits, &pxHigherPriorityTaskWoken);

	portYIELD_FROM_ISR( pxHigherPriorityTaskWoken );
}

/**
 * @brief Main task function for Discharge Profile Control service.
 *
 * This task handles:
 * - Initialization of GPIOs for controlling load, battery, and power path
 * - DAC value and enable control
 * - Registration and handling of protection interrupts (under/over-voltage, over-current)
 * - Processing notifications for various control commands
 * - Sending status updates through the control link
 *
 * The task operates in three states:
 * - `DPCONTROL_STATE_INIT`: Initializes all control and protection hardware
 * - `DPCONTROL_STATE_SERVICE`: Waits for commands via `xTaskNotifyWait` and executes them
 * - `DPCONTROL_STATE_ERROR`: Signals a low-level system error and suspends indefinitely
 *
 * @param[in] pvParameters Unused task parameter
 */
static void prvDPCONTROL_TaskFunc(void* pvParameters){
	uint32_t	value;
	uint32_t	aoutValue;
	dpcontrol_dac_status_t 	activeStatus;
	dpcontrol_load_state_t		loadState;
	dpcontrol_bat_state_t		batState;
	dpcontrol_ppath_state_t		ppathState;
	drv_gpio_pin_init_conf_t 	controlPinConfig;
	drv_gpio_pin_init_conf_t 	protectionPinConfig;
	drv_gpio_pin_init_conf_t 	latchPinConfig;

	for(;;){
		switch(prvDPCONTROL_DATA.state)
		{
		case DPCONTROL_STATE_INIT:
			controlPinConfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
			controlPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

			/*Initialize Load Disable pin*/
			if(DRV_GPIO_Port_Init(DPCONTROL_LOAD_DISABLE_PORT) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to initialize load control port\r\n");
			}
			if(DRV_GPIO_Pin_Init(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, &controlPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to initialize load control pin\r\n");
			}

			switch(prvDPCONTROL_DATA.loadState)
			{
			case DPCONTROL_LOAD_STATE_DISABLE:
				if(DRV_GPIO_Pin_SetState(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to disable load\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Load successfully disabled\r\n");
				}
				break;
			case DPCONTROL_LOAD_STATE_ENABLE:
				if(DRV_GPIO_Pin_SetState(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to enable load\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Load successfully enabled\r\n");
				}
				break;
			}


			/*Initialize Bat Disable pin*/
			if(DRV_GPIO_Port_Init(DPCONTROL_BAT_DISABLE_PORT) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to initialize battery control port\r\n");
			}
			if(DRV_GPIO_Pin_Init(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, &controlPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to initialize battery control pin\r\n");
			}
			switch(prvDPCONTROL_DATA.batState)
			{
			case DPCONTROL_LOAD_STATE_DISABLE:
				if(DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to disable Battery\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Battery successfully disabled\r\n");
				}
				break;
			case DPCONTROL_LOAD_STATE_ENABLE:
				if(DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to enable Battery\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Battery successfully enabled\r\n");
				}
				break;
			}

			/*Initialize PPath Disable pin*/
			if(DRV_GPIO_Port_Init(DPCONTROL_GPIO_DISABLE_PORT) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to initialize ppath control port\r\n");
			}
			if(DRV_GPIO_Pin_Init(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, &controlPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to initialize ppath control pin\r\n");
			}
			switch(prvDPCONTROL_DATA.pathState)
			{
			case DPCONTROL_PPATH_STATE_DISABLE:
				if(DRV_GPIO_Pin_SetState(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to disable Power Path\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Power Path successfully disabled\r\n");
				}
				break;
			case DPCONTROL_PPATH_STATE_ENABLE:
				if(DRV_GPIO_Pin_SetState(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to enable Power Path\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Power Path successfully enabled\r\n");
				}
				break;
			}

			latchPinConfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
			latchPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

			if(DRV_GPIO_Port_Init(DPCONTROL_LATCH_PORT) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to initialize latch port\r\n");
			}
			if(DRV_GPIO_Pin_Init(DPCONTROL_LATCH_PORT, DPCONTROL_LATCH_PIN, &latchPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to initialize latch pin\r\n");
			}

			/* Initialize Under voltage protection interrupt pin*/
			protectionPinConfig.mode = DRV_GPIO_PIN_MODE_IT_RISING_FALLING;
			protectionPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;
			if (DRV_GPIO_Pin_Init(CONF_DPCONTROL_UV_PORT, CONF_DPCONTROL_UV_PIN, &protectionPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_ERROR,  "Unable to register Under Voltage GPIO\r\n");
			}

			if (DRV_GPIO_RegisterCallback(CONF_DPCONTROL_UV_PORT, CONF_DPCONTROL_UV_PIN, prvDPCONTROL_UnderVoltageCB, CONF_DPCONTROL_UV_ISR_PRIO) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_ERROR,  "Unable to register Under Voltage callback\r\n");
			}
			drv_gpio_pin_state_t pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_UV_PORT, CONF_DPCONTROL_UV_PIN);
			prvDPCONTROL_DATA.underVoltage = pinState;
			if(prvDPCONTROL_DATA.underVoltage == DPCONTROL_PROTECTION_STATE_ENABLE)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_WARNING,  "Under voltage protection is active\r\n");
			}

			/* Initialize Over voltage protection interrupt pin*/
			protectionPinConfig.mode = DRV_GPIO_PIN_MODE_IT_RISING_FALLING;
			protectionPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;
			if (DRV_GPIO_Pin_Init(CONF_DPCONTROL_OV_PORT, CONF_DPCONTROL_OV_PIN, &protectionPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_ERROR,  "Unable to register Over Voltage GPIO\r\n");
			}

			if (DRV_GPIO_RegisterCallback(CONF_DPCONTROL_OV_PORT, CONF_DPCONTROL_OV_PIN, prvDPCONTROL_OverVoltageCB, CONF_DPCONTROL_OV_ISR_PRIO) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_ERROR,  "Unable to register Over Voltage callback\r\n");
			}
			pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_OV_PORT, CONF_DPCONTROL_OV_PIN);
			prvDPCONTROL_DATA.overVoltage = pinState;
			if(prvDPCONTROL_DATA.overVoltage == DPCONTROL_PROTECTION_STATE_ENABLE)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_WARNING,  "Over voltage protection is active\r\n");
			}

			/* Initialize Over Current protection interrupt pin*/
			protectionPinConfig.mode = DRV_GPIO_PIN_MODE_IT_RISING_FALLING;
			protectionPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;
			if (DRV_GPIO_Pin_Init(CONF_DPCONTROL_OC_PORT, CONF_DPCONTROL_OC_PIN, &protectionPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_ERROR,  "Unable to register Over Voltage GPIO\r\n");
			}

			if (DRV_GPIO_RegisterCallback(CONF_DPCONTROL_OC_PORT, CONF_DPCONTROL_OC_PIN, prvDPCONTROL_OverCurrentCB, CONF_DPCONTROL_OC_ISR_PRIO) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_ERROR,  "Unable to register Over Voltage callback\r\n");
			}
			pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_OC_PORT, CONF_DPCONTROL_OC_PIN);
			prvDPCONTROL_DATA.overCurrent = pinState;
			if(prvDPCONTROL_DATA.overCurrent == DPCONTROL_PROTECTION_STATE_ENABLE)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_WARNING,  "Over Current protection is active\r\n");
			}

			if(DRV_AOUT_SetValue(prvDPCONTROL_DATA.aoutData.data) != DRV_AOUT_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to set DAC value\r\n");
			}
			LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Discharge Profile Control service successfully initialized\r\n");
			prvDPCONTROL_DATA.state	= DPCONTROL_STATE_SERVICE;
			xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			break;
		case DPCONTROL_STATE_SERVICE:
			xTaskNotifyWait(0x0, 0xFFFFFFFF, &value, portMAX_DELAY);
			if(value & DPCONTROL_MASK_SET_VALUE)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				aoutValue = prvDPCONTROL_DATA.aoutData.data;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				if(DRV_AOUT_SetValue(aoutValue) != DRV_AOUT_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to set DAC value\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "DAC value %d successfully set\r\n", aoutValue);
					xSemaphoreGive(prvDPCONTROL_DATA.initSig);
				}
			}
			if(value & DPCONTROL_MASK_SET_ACTIVE_STATUS)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				activeStatus = prvDPCONTROL_DATA.aoutData.active;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				if(DRV_AOUT_SetEnable(activeStatus) != DRV_AOUT_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to set active status\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Active status (%d) successfully set\r\n", activeStatus);
					xSemaphoreGive(prvDPCONTROL_DATA.initSig);
				}
			}
			if(value & DPCONTROL_MASK_SET_LOAD_STATE)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				loadState = prvDPCONTROL_DATA.loadState;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				switch(loadState)
				{
				case DPCONTROL_LOAD_STATE_DISABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to disable load\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Load successfully disabled\r\n");
					}
					break;
				case DPCONTROL_LOAD_STATE_ENABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to enable load\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Load successfully enabled\r\n");
					}
					break;
				}
				xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			}
			if(value & DPCONTROL_MASK_SET_BAT_STATE)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				batState = prvDPCONTROL_DATA.batState;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				switch(batState)
				{
				case DPCONTROL_BAT_STATE_DISABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to disable battery\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Battery successfully disabled\r\n");
					}
					break;
				case DPCONTROL_BAT_STATE_ENABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to enable battery\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Battery successfully enabled\r\n");
					}
					break;
				}
				xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			}
			if(value & DPCONTROL_MASK_SET_PPATH_STATE)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				ppathState = prvDPCONTROL_DATA.pathState;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				switch(ppathState)
				{
				case DPCONTROL_PPATH_STATE_ENABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to enable power path\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Power path successfully enabled\r\n");
					}
					break;
				case DPCONTROL_PPATH_STATE_DISABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to disable power path\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Power path successfully disabled\r\n");
					}
					break;
				}
				xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			}
			if(value & DPCONTROL_MASK_TRGER_LATCH)
			{
				if(DRV_GPIO_Pin_SetState(DPCONTROL_LATCH_PORT, DPCONTROL_LATCH_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to reset latch\r\n");
				}
				else
				{
					vTaskDelay(pdMS_TO_TICKS(5));
					if(DRV_GPIO_Pin_SetState(DPCONTROL_LATCH_PORT, DPCONTROL_LATCH_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING,  "Unable to reset latch\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Latch successfully reset\r\n");
					}
				}
				//xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			}

			if(value & DPCONTROL_MASK_SET_UV_DETECTED)
			{
				drv_gpio_pin_state_t pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_UV_PORT, CONF_DPCONTROL_UV_PIN);
				if (pinState == DRV_GPIO_PIN_STATE_SET)
				{
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Under Voltage protection enabled\r\n");
					CONTROL_StatusLinkSendMessage("uvoltage enabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}
				else
				{
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Under Voltage protection disabled\r\n");
					CONTROL_StatusLinkSendMessage("uvoltage disabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}
			}
			if(value & DPCONTROL_MASK_SET_OV_DETECTED)
			{
				drv_gpio_pin_state_t pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_OV_PORT, CONF_DPCONTROL_OV_PIN);
				if (pinState == DRV_GPIO_PIN_STATE_SET)
				{
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Over Voltage protection enabled\r\n");
					CONTROL_StatusLinkSendMessage("ovoltage enabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}
				else
				{
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Over Voltage protection disabled\r\n");
					CONTROL_StatusLinkSendMessage("ovoltage disabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}
			}
			if(value & DPCONTROL_MASK_SET_OC_DETECTED)
			{
				drv_gpio_pin_state_t pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_OC_PORT, CONF_DPCONTROL_OC_PIN);
				if (pinState == DRV_GPIO_PIN_STATE_SET)
				{
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Over Current protection enabled\r\n");
					CONTROL_StatusLinkSendMessage("ocurrent enabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}
				else
				{
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Over Current protection disabled\r\n");
					CONTROL_StatusLinkSendMessage("ocurrent disabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}
			}
			break;
		case DPCONTROL_STATE_ERROR:
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(portMAX_DELAY);
			break;
		}
	}
}

dpcontrol_status_t DPCONTROL_Init(uint32_t initTimeout)
{
	memset(&prvDPCONTROL_DATA, 0, sizeof(dpcontrol_data_t));

	prvDPCONTROL_DATA.loadState = DPCONTROL_LOAD_STATE_DISABLE;
	prvDPCONTROL_DATA.batState = DPCONTROL_BAT_STATE_ENABLE;
	prvDPCONTROL_DATA.pathState = DPCONTROL_PPATH_STATE_ENABLE;
	prvDPCONTROL_DATA.aoutData.data = 85; //100mA

	prvDPCONTROL_DATA.initSig = xSemaphoreCreateBinary();

	if(prvDPCONTROL_DATA.initSig == NULL) return DPCONTROL_STATUS_ERROR;

	prvDPCONTROL_DATA.guard = xSemaphoreCreateMutex();

	if(prvDPCONTROL_DATA.guard == NULL) return DPCONTROL_STATUS_ERROR;

	prvDPCONTROL_DATA.state = DPCONTROL_STATE_INIT;

	if(xTaskCreate(
			prvDPCONTROL_TaskFunc,
			DPCONTROL_TASK_NAME,
			DPCONTROL_TASK_STACK,
			NULL,
			DPCONTROL_TASK_PRIO,
			&prvDPCONTROL_DATA.taskHandle) != pdPASS) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}


dpcontrol_status_t 	DPCONTROL_SetValue(uint32_t value, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.aoutData.data = value;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_VALUE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;
	return DPCONTROL_STATUS_OK;
}

dpcontrol_status_t 	DPCONTROL_GetValue(uint32_t* value, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*value = prvDPCONTROL_DATA.aoutData.data;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}

dpcontrol_status_t 	DPCONTROL_SetDACStatus(dpcontrol_dac_status_t activeStatus, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.aoutData.active = activeStatus;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_ACTIVE_STATUS, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;
	return DPCONTROL_STATUS_OK;

}
dpcontrol_status_t 	DPCONTROL_GetDACStatus(dpcontrol_dac_status_t* activeStatus, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*activeStatus = prvDPCONTROL_DATA.aoutData.active;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}

dpcontrol_status_t  DPCONTROL_SetLoadState(dpcontrol_load_state_t state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.loadState = state;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_LOAD_STATE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_GetLoadState(dpcontrol_load_state_t* state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*state = prvDPCONTROL_DATA.loadState;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}

dpcontrol_status_t  DPCONTROL_SetBatState(dpcontrol_bat_state_t state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.batState = state;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_BAT_STATE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_GetBatState(dpcontrol_bat_state_t* state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*state = prvDPCONTROL_DATA.batState;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_SetPPathState(dpcontrol_ppath_state_t state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.pathState = state;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_PPATH_STATE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_GetPPathState(dpcontrol_ppath_state_t* state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*state = prvDPCONTROL_DATA.pathState;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_GetUVoltageState(dpcontrol_protection_state_t* state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*state = prvDPCONTROL_DATA.underVoltage;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_GetOVoltageState(dpcontrol_protection_state_t* state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*state = prvDPCONTROL_DATA.overVoltage;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_GetOCurrentState(dpcontrol_protection_state_t* state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*state = prvDPCONTROL_DATA.overCurrent;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_LatchTriger(uint32_t timeout)
{
	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_TRGER_LATCH, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	//if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */
