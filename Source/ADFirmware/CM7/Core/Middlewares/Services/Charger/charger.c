/**
 ******************************************************************************
 * @file    charger.c
 *
 * @brief   Charger service is responsible for configuring, controlling, and
 *          monitoring the battery charging process via the BQ25180 device.
 *          It runs as a FreeRTOS task and provides a thread-safe interface
 *          for setting charging parameters and processing interrupts.
 *
 * @author  Haris Turkmanovic
 * @date    April 2025
 ******************************************************************************
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "charger.h"
#include "system.h"
#include "logging.h"
#include "control.h"
#include "bq25180.h"
#include "at24cs01.h"
#include "drv_gpio.h"

#include "configuration.h"

/**
 * @defgroup SERVICES Service
 * @{
 */

/**
 * @defgroup CHARGER_SERVICE Charger service
 * @{
 */

/**
 * @defgroup CHARGER_DEFINES Charger task defines and default values
 * @{
 */
#define CHARGER_TASK_SET_CHARGING_STATUS              0x00000001 /**< Task flag: Set charging enable/disable */
#define CHARGER_TASK_SET_CURRENT_CHARGING_VALUE       0x00000002 /**< Task flag: Set charging current */
#define CHARGER_TASK_SET_CURRENT_TERMINATION_VALUE    0x00000004 /**< Task flag: Set termination current */
#define CHARGER_TASK_SET_VOLTAGE_TERMINATION_VALUE    0x00000008 /**< Task flag: Set termination voltage */
#define CHARGER_TASK_REG_READ                         0x00000010 /**< Task flag: Read a register */
#define CHARGER_TASK_PROCESS_INT                      0x00000020 /**< Task flag: Process interrupt */
#define CHARGER_TASK_SET_MAX_CURRENT_VALUE            0x00000040 /**< Task flag: Set maximum charging current */

#define CHARGER_DEFAULT_CURRENT_TERMINATION_VALUE     5      /**< Default termination current (%) */
#define CHARGER_DEFAULT_VOLTAGE_TERMINATION_VALUE     4.12   /**< Default termination voltage (V) */
#define CHARGER_DEFAULT_CURRENT_CHARGING_VALUE        100    /**< Default charging current (mA) */
#define CHARGER_DEFAULT_CURRENT_ILIM_VALUE            3      /**< Default input current limit (index value, e.g., 200 mA) */
#define CHARGER_DEFAULT_CHARGING_STATE                0      /**< Default charging state (disabled) */
#define CHARGER_DEFAULT_WD_STATE                      0      /**< Default watchdog state (disabled) */
/**
 * @}
 */

/**
 * @defgroup CHARGER_PRIVATE_STRUCTURES Charger private structures
 * @{
 */

/**
 * @brief Structure representing register content to be read
 */
typedef struct
{
    uint8_t addr; /**< Register address */
    uint8_t data; /**< Register data */
} charger_reg_content_t;

/**
 * @brief Structure holding charger configuration parameters
 */
typedef struct
{
    float terminationVoltage;                 /**< Target charge termination voltage (V) */
    bq25180_tcurrent_value_t terminationCurrent;               /**< Charge termination current (%) */
    uint16_t chargingCurrent;                 /**< Charging current (mA) */
    bq25180_ilim_value_t currentLimit;        /**< Input current limit setting */
    bq25180_charge_status chargingStatus;     /**< Charging enable/disable status */
    bq25180_wdg_status wdStatus;              /**< Watchdog timer enable/disable status */
    char hwSerial[CONF_CONFIGURATION_MAX_PARAM_VALUESIZE];
    char fwVersion[CONF_CONFIGURATION_MAX_PARAM_VALUESIZE];
} charger_charging_info_t;

/**
 * @brief Main charger task data structure
 */
typedef struct
{
    charger_state_t state;                    /**< Current state of the charger task */
    SemaphoreHandle_t initSig;                /**< Semaphore for signaling initialization completion */
    SemaphoreHandle_t guard;                  /**< Mutex for thread-safe parameter access */
    TaskHandle_t taskHandle;                  /**< FreeRTOS task handle */
    charger_charging_info_t chargingInfo;     /**< Current charging configuration */
    charger_reg_content_t regContent;         /**< Register read request */
    uint8_t chargerIntStatus;                /**< Latest charger interrupt flags */
    uint8_t adcIntStatus;                     /**< Latest ADC interrupt flags */
    uint8_t timerIntStatus;                   /**< Latest timer interrupt flags */
    charger_con_status_t connectionStatus;
    uint8_t intCounter;
} charger_data_t;
/**
 * @}
 */

/**
 * @defgroup CHARGER_PRIVATE_DATA Charger private data
 * @{
 */
/**
 * @brief Static instance of the charger service data
 */
static charger_data_t prvCHARGER_DATA;
/**
 * @}
 */
/**
 * @defgroup CHARGER_PRIVATE_FUNCTIONS Control service private functions
 * @{
 */
/**
 * @brief Interrupt callback function registered to the BQ25180 charger.
 *
 * This function is triggered by the charger (via ISR) when an interrupt condition occurs.
 * It notifies the charger task using FreeRTOS's `xTaskNotifyFromISR()` with the
 * `CHARGER_TASK_PROCESS_INT` flag. If a higher priority task was woken, context switch
 * is requested via `portYIELD_FROM_ISR`.
 *
 * @note This function is designed to be used only in interrupt context.
 */
static void prvCHARGER_CB()
{
	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED) return;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTaskNotifyFromISR(prvCHARGER_DATA.taskHandle, CHARGER_TASK_PROCESS_INT, eSetBits, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief Update charger connection status
 * @retval void
 */
static void prvCHARGER_UpdateConnectionStatus(void)
{
	charger_con_status_t previousConnectionStatus = prvCHARGER_DATA.connectionStatus;

	if(DRV_GPIO_Pin_ReadState(CHARGER_CONNECTION_PORT, CHARGER_CONNECTION_PIN) == DRV_GPIO_PIN_STATE_SET)
	{
		prvCHARGER_DATA.connectionStatus = CHARGER_CON_STATUS_DISCONNECTED;
		prvCHARGER_DATA.intCounter = 0;
	}
	else
	{
		prvCHARGER_DATA.connectionStatus = CHARGER_CON_STATUS_CONNECTED;
	}

	if(previousConnectionStatus != prvCHARGER_DATA.connectionStatus)
	{
		if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_CONNECTED)
		{
			LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO, "Charger connected\r\n");
			CONTROL_StatusLinkSendMessage("charger connection connected\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
			vTaskDelay(pdMS_TO_TICKS(500));
		}
		else
		{
			LOGGING_Write("Charger service", LOGGING_MSG_TYPE_WARNING, "Charger disconnected\r\n");
			CONTROL_StatusLinkSendMessage("charger connection disconnected\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
		}
	}
}


/**
 * @brief Charger management task function.
 *
 * This is the core task responsible for initializing the BQ25180 charger and handling
 * configuration commands and interrupts. The task transitions through multiple states:
 *
 * - `CHARGER_STATE_INIT`: Performs initialization steps such as:
 *    - Establishing communication with the charger
 *    - Configuring voltage/current/termination levels
 *    - Registering callbacks
 *
 * - `CHARGER_STATE_SERVICE`: Waits on notifications from other API calls or interrupts and
 *   applies requested settings to the charger (e.g. enabling/disabling charging, setting
 *   voltage/current parameters, reading registers, or handling charger interrupts).
 *
 * - `CHARGER_STATE_ERROR`: If any of the initialization steps or operations fail,
 *   enters an error state and reports it via `SYSTEM_ReportError`.
 *
 * @param[in] pvParameters Unused task parameter.
 */
static void prvCHARGER_TaskFunc(void* pvParameters)
{
	uint32_t notifyValue = 0;
	uint16_t intMask = 0xFFFF;
	uint8_t eepromPreset = 0;
	uint8_t defaultFlag = 0;
	int32_t intValue;
	int32_t retryCounter = 0;
	int errorCode = 0;

	drv_gpio_pin_init_conf_t protectionPinConfig;
	charger_con_status_t previousConnectionStatus;

	protectionPinConfig.mode = DRV_GPIO_PIN_MODE_INPUT;
	protectionPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

	prvCHARGER_DATA.connectionStatus = CHARGER_CON_STATUS_DISCONNECTED;


    /*This is rare case to initialize pins outside of INIT state because of INIT logic*/
    DRV_GPIO_Port_Init(CHARGER_CONNECTION_PORT);
    DRV_GPIO_Pin_Init(CHARGER_CONNECTION_PORT, CHARGER_CONNECTION_PIN, &protectionPinConfig);


	for(;;){
		switch(prvCHARGER_DATA.state)
		{
		case CHARGER_STATE_INIT:
			prvCHARGER_UpdateConnectionStatus();

			if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
			{
				xSemaphoreGive(prvCHARGER_DATA.initSig);
				continue;
			}


			/*If it is connected, continue from here*/
			if(BQ25180_Init() != BQ25180_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to initialize BQ25180\r\n");
				errorCode = 1;
				break;
			}

			if(BQ25180_Ping(1000) != BQ25180_STATUS_OK)
			{
				retryCounter +=1;
				if(retryCounter == 3)
				{
					retryCounter = 0;
					prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
					errorCode = 2;
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to establish connection with charger\r\n");
					break;

				}
				continue;
			}

			if(CONFIGURATION_CHARGER_TestBD(&eepromPreset, 1000) != CONFIGURATION_STATUS_OK)
			{
				retryCounter +=1;
				if(retryCounter == 3)
				{
					retryCounter = 0;
					prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to Communicate with charger EEPROM\r\n");
					errorCode = 3;
					break;
				}
				continue;
			}

			if(CONFIGURATION_CHARGER_UpdateFromBD(1000) != CONFIGURATION_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_WARNING,  "Unable to update charger parameters from charger's EEPROM\r\n");
			}

			CONFIGURATION_CHARGER_GetParameter_String("HW_SER", prvCHARGER_DATA.chargingInfo.hwSerial, CONF_CONFIGURATION_MAX_PARAM_VALUESIZE, &defaultFlag);
			CONFIGURATION_CHARGER_GetParameter_String("FW_VER", prvCHARGER_DATA.chargingInfo.fwVersion, CONF_CONFIGURATION_MAX_PARAM_VALUESIZE, &defaultFlag);


			BQ25180_GetChargerIntFlags(&prvCHARGER_DATA.chargerIntStatus, 1000);

			intMask &= ~(BQ25180_MASK_CHARGE_STATUS_CHANGED);

			if(BQ25180_SetChargerIntMask(intMask, 1000) != BQ25180_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to disable all charger interrupts \r\n");
				errorCode = 4;
				break;
			}


			if(BQ25180_RegCallback(prvCHARGER_CB) != BQ25180_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to register CB\r\n");
				errorCode = 5;
				break;
			}


			if(BQ25180_SetChargerTimerState(BQ25180_STIMER_VALUE_DISABLED, 1000) != BQ25180_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to register CB\r\n");
				errorCode = 6;
				break;
			}

			if(BQ25180_Charge_ChargeStatus_Set(prvCHARGER_DATA.chargingInfo.chargingStatus, 1000) != BQ25180_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to disable charger \r\n");
				errorCode = 7;
				break;
			}


			CONFIGURATION_CHARGER_GetParameter_Int("MAX_CUR", &intValue, &defaultFlag);
			prvCHARGER_DATA.chargingInfo.currentLimit = intValue;

			if(BQ25180_ILim_Set(prvCHARGER_DATA.chargingInfo.currentLimit, 1000) != BQ25180_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set charger input current limit\r\n");
				errorCode = 8;
				break;
			}

			if(BQ25180_WDG_SetStatus(prvCHARGER_DATA.chargingInfo.wdStatus, 1000) != BQ25180_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to disable WD\r\n");
				errorCode = 9;
				break;
			}

			CONFIGURATION_CHARGER_GetParameter_Float("TERM_VOLT", &prvCHARGER_DATA.chargingInfo.terminationVoltage, &defaultFlag);

			if(BQ25180_Charge_RegVoltage_Set(prvCHARGER_DATA.chargingInfo.terminationVoltage, 1000) != BQ25180_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set Termination voltage\r\n");
				errorCode = 10;
				break;
			}

			CONFIGURATION_CHARGER_GetParameter_Int("TERM_CUR", &intValue, &defaultFlag);
			prvCHARGER_DATA.chargingInfo.terminationCurrent = intValue;

			if(BQ25180_Charge_TermCurrent_Set(prvCHARGER_DATA.chargingInfo.terminationCurrent, 1000) != BQ25180_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set Termination current\r\n");
				errorCode = 11;
				break;
			}

			CONFIGURATION_CHARGER_GetParameter_Int("CH_CUR", &intValue, &defaultFlag);
			prvCHARGER_DATA.chargingInfo.chargingCurrent = (uint16_t)intValue;


			if(BQ25180_Charge_Current_Set(prvCHARGER_DATA.chargingInfo.chargingCurrent, 1000) != BQ25180_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set charging current \r\n");
				errorCode = 12;
				break;
			}


			LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Charger successfully initialized \r\n");

			prvCHARGER_DATA.state	= CHARGER_STATE_SERVICE;
			xSemaphoreGive(prvCHARGER_DATA.initSig);
			break;
		case CHARGER_STATE_SERVICE:
			notifyValue = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

			prvCHARGER_UpdateConnectionStatus();

			/* Return to initialization state when charger is disconnected. */
			if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
			{
				prvCHARGER_DATA.state = CHARGER_STATE_INIT;
				retryCounter = 0;
				continue;
			}

			if(notifyValue == 0) continue;


			if(notifyValue & CHARGER_TASK_SET_CHARGING_STATUS)
			{
				if(BQ25180_Charge_ChargeStatus_Set(prvCHARGER_DATA.chargingInfo.chargingStatus, 1000) != BQ25180_STATUS_OK)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set charging status \r\n");
				}
				else
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Charging status successfully set\r\n");
				}
				xSemaphoreGive(prvCHARGER_DATA.initSig);
			}
			if(notifyValue & CHARGER_TASK_SET_CURRENT_CHARGING_VALUE)
			{
				if(BQ25180_Charge_Current_Set(prvCHARGER_DATA.chargingInfo.chargingCurrent, 1000) != BQ25180_STATUS_OK)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set charging current \r\n");
				}
				else
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Charging current set\r\n");
					CONFIGURATION_CHARGER_SetParameter_Int("CH_CUR", prvCHARGER_DATA.chargingInfo.chargingCurrent, 1000);

				}
				xSemaphoreGive(prvCHARGER_DATA.initSig);
			}
			if(notifyValue & CHARGER_TASK_SET_CURRENT_TERMINATION_VALUE)
			{
				if(BQ25180_Charge_TermCurrent_Set(prvCHARGER_DATA.chargingInfo.terminationCurrent, 1000) != BQ25180_STATUS_OK)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set charging termination current \r\n");
				}
				else
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Charging termination current set\r\n");
					CONFIGURATION_CHARGER_SetParameter_Int("TERM_CUR", prvCHARGER_DATA.chargingInfo.terminationCurrent, 1000);

				}
				xSemaphoreGive(prvCHARGER_DATA.initSig);
			}
			if(notifyValue & CHARGER_TASK_SET_VOLTAGE_TERMINATION_VALUE)
			{
				if(BQ25180_Charge_RegVoltage_Set(prvCHARGER_DATA.chargingInfo.terminationVoltage, 1000) != BQ25180_STATUS_OK)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set charging termination voltage \r\n");
				}
				else
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Charging termination voltage set\r\n");
					CONFIGURATION_CHARGER_SetParameter_Float("TERM_VOLT", prvCHARGER_DATA.chargingInfo.terminationVoltage, 1000);
				}
				xSemaphoreGive(prvCHARGER_DATA.initSig);
			}
			if(notifyValue & CHARGER_TASK_REG_READ)
			{
				if(BQ25180_ReadReg(prvCHARGER_DATA.regContent.addr, &prvCHARGER_DATA.regContent.data, 1000) != BQ25180_STATUS_OK)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to read register \r\n");
				}
				else
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Register successfully read\r\n");
				}
				xSemaphoreGive(prvCHARGER_DATA.initSig);
			}
			if(notifyValue & CHARGER_TASK_PROCESS_INT)
			{
				//First int is not real interrupt
				if(prvCHARGER_DATA.intCounter == 0)
				{
					prvCHARGER_DATA.intCounter += 1;
					continue;
				}
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Interrupt detected \r\n");
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Read interrupt status \r\n");
				BQ25180_GetChargerIntFlags(&prvCHARGER_DATA.chargerIntStatus, 1000);
				//BQ25180_GetADCIntFlags(&prvCHARGER_DATA.adcIntStatus, 1000);
				//BQ25180_GetTimerIntFlags(&prvCHARGER_DATA.timerIntStatus, 1000);

				bq25180_charging_status_t cStatus = prvCHARGER_DATA.chargerIntStatus >> 5;

				if(cStatus == BQ25180_CHARGING_STATUS_DONE)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_WARNING,  "Charging done \r\n");
					CONTROL_StatusLinkSendMessage("charger charging done\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}

				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Charger Int:  0x%02X \r\n", prvCHARGER_DATA.chargerIntStatus);
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "ADC Int: 		0x%02X \r\n", prvCHARGER_DATA.adcIntStatus);
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Timer Int:    0x%02X \r\n", prvCHARGER_DATA.timerIntStatus);

			}
			if(notifyValue & CHARGER_TASK_SET_MAX_CURRENT_VALUE)
			{
				if(BQ25180_ILim_Set(prvCHARGER_DATA.chargingInfo.currentLimit, 1000) != BQ25180_STATUS_OK)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR, "Unable to set maximum charging current\r\n");
				}
				else
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO, "Maximum charging current set\r\n");
					CONFIGURATION_CHARGER_SetParameter_Int("MAX_CUR", prvCHARGER_DATA.chargingInfo.currentLimit, 1000);

				}

				xSemaphoreGive(prvCHARGER_DATA.initSig);
			}
			break;
		case CHARGER_STATE_UNDEF:
		case CHARGER_STATE_ERROR:
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(pdMS_TO_TICKS(1000));
			prvCHARGER_DATA.state	= CHARGER_STATE_INIT;
			break;
		}
	}
}

charger_status_t 	CHARGER_Init(uint32_t initTimeout)
{
	if(xTaskCreate(
			prvCHARGER_TaskFunc,
			CHARGER_TASK_NAME,
			CHARGER_TASK_STACK,
			NULL,
			CHARGER_TASK_PRIO,
			&prvCHARGER_DATA.taskHandle) != pdPASS) return CHARGER_STATUS_ERROR;

	prvCHARGER_DATA.initSig = xSemaphoreCreateBinary();

	if(prvCHARGER_DATA.initSig == NULL) return CHARGER_STATUS_ERROR;

	prvCHARGER_DATA.guard = xSemaphoreCreateMutex();

	if(prvCHARGER_DATA.guard == NULL) return CHARGER_STATUS_ERROR;

	prvCHARGER_DATA.chargingInfo.chargingCurrent 	= CHARGER_DEFAULT_CURRENT_CHARGING_VALUE;
	prvCHARGER_DATA.chargingInfo.terminationCurrent = BQ25180_TCURRENT_VALUE_20;
	prvCHARGER_DATA.chargingInfo.terminationVoltage = CHARGER_DEFAULT_VOLTAGE_TERMINATION_VALUE;
	prvCHARGER_DATA.chargingInfo.currentLimit 		= BQ25180_ILIM_VALUE_500;
	prvCHARGER_DATA.chargingInfo.chargingStatus 	= CHARGER_DEFAULT_CHARGING_STATE;
	prvCHARGER_DATA.chargingInfo.wdStatus 			= CHARGER_DEFAULT_WD_STATE;

	prvCHARGER_DATA.state = CHARGER_STATE_INIT;

	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t	CHARGER_SetChargingState(charger_charging_state_t state, uint32_t initTimeout)
{
	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	prvCHARGER_DATA.chargingInfo.chargingStatus = state;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(xTaskNotify(prvCHARGER_DATA.taskHandle,
			CHARGER_TASK_SET_CHARGING_STATUS,
			eSetBits) != pdPASS) return CHARGER_STATUS_ERROR;


	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t	CHARGER_GetChargingState(charger_charging_state_t* state, uint32_t initTimeout)
{
	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED) return CHARGER_STATUS_ERROR;

	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	*state = prvCHARGER_DATA.chargingInfo.chargingStatus;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t	CHARGER_SetChargingCurrent(uint16_t current, uint32_t initTimeout)
{
	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	prvCHARGER_DATA.chargingInfo.chargingCurrent = current;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(xTaskNotify(prvCHARGER_DATA.taskHandle,
			CHARGER_TASK_SET_CURRENT_CHARGING_VALUE,
			eSetBits) != pdPASS) return CHARGER_STATUS_ERROR;


	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t	CHARGER_GetChargingCurrent(uint16_t* current, uint32_t initTimeout)
{

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED) return CHARGER_STATUS_ERROR;

	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	*current = prvCHARGER_DATA.chargingInfo.chargingCurrent;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t	CHARGER_SetChargingTermCurrent(uint16_t current, uint32_t initTimeout)
{
	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	prvCHARGER_DATA.chargingInfo.terminationCurrent = current;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(xTaskNotify(prvCHARGER_DATA.taskHandle,
			CHARGER_TASK_SET_CURRENT_TERMINATION_VALUE,
			eSetBits) != pdPASS) return CHARGER_STATUS_ERROR;


	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}
charger_status_t	CHARGER_GetChargingTermCurrent(uint16_t* current, uint32_t initTimeout)
{

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED) return CHARGER_STATUS_ERROR;

	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	*current = prvCHARGER_DATA.chargingInfo.terminationCurrent;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}
charger_status_t	CHARGER_SetChargingTermVoltage(float voltage, uint32_t initTimeout)
{

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED) return CHARGER_STATUS_ERROR;

	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	prvCHARGER_DATA.chargingInfo.terminationVoltage = voltage;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(xTaskNotify(prvCHARGER_DATA.taskHandle,
			CHARGER_TASK_SET_VOLTAGE_TERMINATION_VALUE,
			eSetBits) != pdPASS) return CHARGER_STATUS_ERROR;


	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}
charger_status_t	CHARGER_GetChargingTermVoltage(float* voltage, uint32_t initTimeout)
{

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED) return CHARGER_STATUS_ERROR;

	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	*voltage = prvCHARGER_DATA.chargingInfo.terminationVoltage;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t CHARGER_GetConnectionStatus(charger_con_status_t* status, uint32_t initTimeout)
{

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED) return CHARGER_STATUS_ERROR;

	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	*status = prvCHARGER_DATA.connectionStatus;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;

}
charger_status_t	CHARGER_GetRegContent(uint8_t regAddr, uint8_t* regData, uint32_t initTimeout)
{

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED) return CHARGER_STATUS_ERROR;

	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	prvCHARGER_DATA.regContent.addr = regAddr;
	prvCHARGER_DATA.regContent.data = 0;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(xTaskNotify(prvCHARGER_DATA.taskHandle,
			CHARGER_TASK_REG_READ,
			eSetBits) != pdPASS) return CHARGER_STATUS_ERROR;


	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	*regData = prvCHARGER_DATA.regContent.data;

	return CHARGER_STATUS_OK;
}

charger_status_t CHARGER_GetSerial(char* serial, uint16_t size, uint32_t initTimeout)
{

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED) return CHARGER_STATUS_ERROR;

	if(serial == NULL || size == 0U) return CHARGER_STATUS_ERROR;

	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	strncpy(serial, prvCHARGER_DATA.chargingInfo.hwSerial, size - 1U);
	serial[size - 1U] = '\0';

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t CHARGER_GetFwVersion(char* version, uint16_t size, uint32_t initTimeout)
{
	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED) return CHARGER_STATUS_ERROR;

	if(version == NULL || size == 0U) return CHARGER_STATUS_ERROR;

	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	strncpy(version, prvCHARGER_DATA.chargingInfo.fwVersion, size - 1U);
	version[size - 1U] = '\0';

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}
charger_status_t CHARGER_SetChargingMaxCurrent(charger_max_charging_current_t current, uint32_t initTimeout)
{
	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED) return CHARGER_STATUS_ERROR;

	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	prvCHARGER_DATA.chargingInfo.currentLimit = current;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(xTaskNotify(prvCHARGER_DATA.taskHandle, CHARGER_TASK_SET_MAX_CURRENT_VALUE, eSetBits) != pdPASS) return CHARGER_STATUS_ERROR;

	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}
charger_status_t CHARGER_GetChargingMaxCurrent(charger_max_charging_current_t* current, uint32_t initTimeout)
{
	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED) return CHARGER_STATUS_ERROR;

	if(current == NULL) return CHARGER_STATUS_ERROR;

	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(prvCHARGER_DATA.connectionStatus == CHARGER_CON_STATUS_DISCONNECTED)
	{
		xSemaphoreGive(prvCHARGER_DATA.guard);
		return CHARGER_STATUS_ERROR;
	}

	*current = prvCHARGER_DATA.chargingInfo.currentLimit;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
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
