/**
 ******************************************************************************
 * @file	charger.h
 *
 * @brief	Charger service provides interface to control and monitor battery
 * 			charging process. It allows enabling/disabling charging, setting and
 * 			retrieving charging parameters like current, termination current, and
 * 			termination voltage.
 * 			All charger service interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	May 2026
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_CHARGER_CHARGER_H_
#define CORE_MIDDLEWARES_SERVICES_CHARGER_CHARGER_H_

#include "globalConfig.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup CHARGER_SERVICE Charger Service
 * @{
 */

/**
 * @defgroup CHARGER_PUBLIC_DEFINES Charger service public defines
 * @{
 */
#define CHARGER_TASK_NAME				CONF_CHARGER_TASK_NAME			/*!< Charger service task name */
#define CHARGER_TASK_PRIO				CONF_CHARGER_PRIO				/*!< Charger service task priority */
#define CHARGER_TASK_STACK				CONF_CHARGER_STACK_SIZE			/*!< Charger service task stack size */

#define CHARGER_CONNECTION_PORT 		CONF_CHARGER_CONNECTION_PORT
#define CHARGER_CONNECTION_PIN			CONF_CHARGER_CONNECTION_PIN

/**
 * @}
 */

/**
 * @defgroup CHARGER_PUBLIC_TYPES Charger service public data types
 * @{
 */

/**
 * @brief Charger service return status
 */
typedef enum {
	CHARGER_STATUS_OK,				/*!< Charger operation successful */
	CHARGER_STATUS_ERROR			/*!< Charger operation failed */
} charger_status_t;

/**
 * @brief Charger service state
 */
typedef enum {
	CHARGER_STATE_UNDEF = 0,				/*!< Initialization state */
	CHARGER_STATE_INIT,				/*!< Initialization state */
	CHARGER_STATE_SERVICE,			/*!< Charging service active */
	CHARGER_STATE_ERROR				/*!< Charger service in error state */
} charger_state_t;

/**
 * @brief Charger connection status
 */
typedef enum {
	CHARGER_CON_STATUS_DISCONNECTED = 0,	/*!< Charger is disconnected */
	CHARGER_CON_STATUS_CONNECTED = 1		/*!< Charger is connected */
} charger_con_status_t;


/**
 * @brief Charging enable/disable state
 */
typedef enum {
	CHARGER_CHARGING_DISABLE = 0,	/*!< Charging is disabled */
	CHARGER_CHARGING_ENABLE			/*!< Charging is enabled */
} charger_charging_state_t;

/**
 * @brief Charger maximum charging current
 */
typedef enum {
	CHARGER_ILIM_VALUE_50 	= 0,	/*!< Maximum charging current: 50 mA */
	CHARGER_ILIM_VALUE_100	= 1,	/*!< Maximum charging current: 100 mA */
	CHARGER_ILIM_VALUE_200	= 2,	/*!< Maximum charging current: 200 mA */
	CHARGER_ILIM_VALUE_300	= 3,	/*!< Maximum charging current: 300 mA */
	CHARGER_ILIM_VALUE_400	= 4,	/*!< Maximum charging current: 400 mA */
	CHARGER_ILIM_VALUE_500	= 5,	/*!< Maximum charging current: 500 mA */
	CHARGER_ILIM_VALUE_700	= 6,	/*!< Maximum charging current: 700 mA */
	CHARGER_ILIM_VALUE_1100	= 7		/*!< Maximum charging current: 1100 mA */
} charger_max_charging_current_t;

/**
 * @}
 */

/**
 * @defgroup CHARGER_PUBLIC_FUNCTIONS Charger service interface functions
 * @{
 */

/**
 * @brief	Initialize the charger service
 * @param	initTimeout: Timeout to complete initialization
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_Init(uint32_t initTimeout);

/**
 * @brief	Set the charging enable/disable state
 * @param	state: Charging state (enable/disable). See ::charger_charging_state_t
 * @param	initTimeout: Timeout for operation
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_SetChargingState(charger_charging_state_t state, uint32_t initTimeout);

/**
 * @brief	Get the current charging enable/disable state
 * @param	state: Pointer to variable to store retrieved state
 * @param	initTimeout: Timeout for operation
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_GetChargingState(charger_charging_state_t* state, uint32_t initTimeout);

/**
 * @brief	Set the charging current
 * @param	current: Charging current in mA
 * @param	initTimeout: Timeout for operation
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_SetChargingCurrent(uint16_t current, uint32_t initTimeout);

/**
 * @brief	Get the current charging current
 * @param	current: Pointer to variable to store current value
 * @param	initTimeout: Timeout for operation
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_GetChargingCurrent(uint16_t* current, uint32_t initTimeout);

/**
 * @brief	Set the termination current
 * @param	current: Termination current in mA
 * @param	initTimeout: Timeout for operation
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_SetChargingTermCurrent(uint16_t current, uint32_t initTimeout);

/**
 * @brief	Get the termination current
 * @param	current: Pointer to variable to store termination current
 * @param	initTimeout: Timeout for operation
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_GetChargingTermCurrent(uint16_t* current, uint32_t initTimeout);

/**
 * @brief	Set the termination voltage
 * @param	voltage: Termination voltage in Volts
 * @param	initTimeout: Timeout for operation
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_SetChargingTermVoltage(float voltage, uint32_t initTimeout);

/**
 * @brief	Get the termination voltage
 * @param	voltage: Pointer to variable to store termination voltage
 * @param	initTimeout: Timeout for operation
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_GetChargingTermVoltage(float* voltage, uint32_t initTimeout);

/**
 * @brief	Read content of internal charger register
 * @param	regAddr: Register address
 * @param	regData: Pointer to variable to store register content
 * @param	initTimeout: Timeout for operation
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_GetRegContent(uint8_t regAddr, uint8_t* regData, uint32_t initTimeout);

/**
 * @brief   Get charger hardware serial number
 * @param   serial: Pointer to buffer where the serial number will be stored
 * @param   size: Size of the destination buffer in bytes
 * @param   initTimeout: Timeout for accessing charger data in milliseconds
 * @retval  ::charger_status_t
 */
charger_status_t CHARGER_GetSerial(char* serial, uint16_t size, uint32_t initTimeout);

/**
 * @brief   Get charger firmware version
 * @param   version: Pointer to buffer where the firmware version will be stored
 * @param   size: Size of the destination buffer in bytes
 * @param   initTimeout: Timeout for accessing charger data in milliseconds
 * @retval  ::charger_status_t
 */
charger_status_t CHARGER_GetFwVersion(char* version, uint16_t size, uint32_t initTimeout);
/**
 * @brief	Get charger connection status
 * @param	status: Pointer to variable to store charger connection status. See ::charger_con_status_t
 * @param	initTimeout: Timeout for operation
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_GetConnectionStatus(charger_con_status_t* status, uint32_t initTimeout);

/**
 * @brief	Set the maximum charging current
 * @param	current: Maximum charging current value. See ::charger_max_charging_current_t
 * @param	initTimeout: Timeout for operation
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_SetChargingMaxCurrent(charger_max_charging_current_t current, uint32_t initTimeout);

/**
 * @brief	Get the maximum charging current
 * @param	current: Pointer to variable to store maximum charging current. See ::charger_max_charging_current_t
 * @param	initTimeout: Timeout for operation
 * @retval	::charger_status_t
 */
charger_status_t CHARGER_GetChargingMaxCurrent(charger_max_charging_current_t* current, uint32_t initTimeout);



/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_MIDDLEWARES_SERVICES_CHARGER_CHARGER_H_ */
