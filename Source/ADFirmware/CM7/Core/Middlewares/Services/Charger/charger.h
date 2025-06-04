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
 * @date	April 2025
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
	CHARGER_STATE_INIT,				/*!< Initialization state */
	CHARGER_STATE_SERVICE,			/*!< Charging service active */
	CHARGER_STATE_ERROR				/*!< Charger service in error state */
} charger_state_t;

/**
 * @brief Charging enable/disable state
 */
typedef enum {
	CHARGER_CHARGING_DISABLE = 0,	/*!< Charging is disabled */
	CHARGER_CHARGING_ENABLE			/*!< Charging is enabled */
} charger_charging_state_t;

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
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_MIDDLEWARES_SERVICES_CHARGER_CHARGER_H_ */
