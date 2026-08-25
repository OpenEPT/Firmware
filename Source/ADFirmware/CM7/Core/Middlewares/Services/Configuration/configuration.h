/**
 ********************************************************************************
 * @file	configuration.h
 *
 * @brief	Configuration service provides interface for managing device and
 * 			charger configuration parameters. It supports loading and storing
 * 			device configuration from the file system, accessing and updating
 * 			individual parameters, and managing charger configuration stored
 * 			in the charger board EEPROM.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	April 2026
 ********************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_CONFIGURATION_CONFIGURATION_H_
#define CORE_MIDDLEWARES_SERVICES_CONFIGURATION_CONFIGURATION_H_

#include <stdint.h>

#include "globalConfig.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup CONFIGURATION_SERVICE Configuration Service
 * @{
 */

/**
 * @defgroup CONFIGURATION_PUBLIC_DEFINES Configuration service public defines
 * @{
 */

#define CONFIGURATION_TASK_NAME      			CONF_CONFIGURATION_TASK_NAME				/*!< Configuration service task name */
#define CONFIGURATION_TASK_PRIO      			CONF_CONFIGURATION_TASK_PRIO				/*!< Configuration service task priority */
#define CONFIGURATION_TASK_STACK     			CONF_CONFIGURATION_TASK_STACK_SIZE			/*!< Configuration service task stack size */
#define CONFIGURATION_FILE_PATH      			CONF_CONFIGURATION_FILE_PATH				/*!< Device configuration file path */
#define CONFIGURATION_FILE_MAX_SIZE  			CONF_CONFIGURATION_FILE_MAX_SIZE			/*!< Maximum configuration file size */
#define CONFIGURATION_MAX_PARAMS     			CONF_CONFIGURATION_MAX_PARAMS				/*!< Maximum number of configuration parameters */
#define CONFIGURATION_MAX_PARAM_VALUESIZE		CONF_CONFIGURATION_MAX_PARAM_VALUESIZE		/*!< Maximum configuration parameter value size */

/**
 * @}
 */

/**
 * @defgroup CONFIGURATION_PUBLIC_TYPES Configuration service public data types
 * @{
 */

/**
 * @brief Configuration service return status
 */
typedef enum
{
    CONFIGURATION_STATUS_OK = 0,		/*!< Configuration operation successful */
    CONFIGURATION_STATUS_ERROR			/*!< Configuration operation failed */
} configuration_status_t;

/**
 * @brief Configuration service state
 */
typedef enum
{
    CONFIGURATION_STATE_INIT = 0,		/*!< Configuration service initialization state */
    CONFIGURATION_STATE_SERVICE,		/*!< Configuration service active state */
    CONFIGURATION_STATE_ERROR			/*!< Configuration service error state */
} configuration_state_t;

/**
 * @}
 */

/**
 * @defgroup CONFIGURATION_PUBLIC_FUNCTIONS Configuration service interface functions
 * @{
 */

/**
 * @brief	Initialize the Configuration service
 * @param	initTimeout: Timeout to complete initialization in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_Init(uint32_t initTimeout);

/**
 * @brief	Update device configuration parameters from the file system
 * @param	initTimeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_UpdateFromFS(uint32_t initTimeout);

/**
 * @brief	Store current device configuration parameters to the file system
 * @param	initTimeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_StoreToFS(uint32_t initTimeout);

/**
 * @brief	Get device configuration parameter by key
 * @param	key: Parameter name
 * @param	parameter: Pointer to buffer where parameter value will be stored
 * @param	paramSize: Pointer to variable where parameter value size will be stored
 * @param	defaultFlag: Pointer to variable indicating whether default value is used
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_GetParameter(const char* key, char* parameter, uint16_t* paramSize, uint8_t* defaultFlag);

/**
 * @brief	Get integer device configuration parameter
 * @param	key: Parameter name
 * @param	value: Pointer to variable where parameter value will be stored
 * @param	defaultFlag: Pointer to variable indicating whether default value is used
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_GetParameter_Int(const char* key, int32_t* value, uint8_t* defaultFlag);

/**
 * @brief	Get floating-point device configuration parameter
 * @param	key: Parameter name
 * @param	value: Pointer to variable where parameter value will be stored
 * @param	defaultFlag: Pointer to variable indicating whether default value is used
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_GetParameter_Float(const char* key, float* value, uint8_t* defaultFlag);

/**
 * @brief	Get string device configuration parameter
 * @param	key: Parameter name
 * @param	buffer: Pointer to destination buffer
 * @param	bufferSize: Size of destination buffer in bytes
 * @param	defaultFlag: Pointer to variable indicating whether default value is used
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_GetParameter_String(const char* key, char* buffer, uint16_t bufferSize, uint8_t* defaultFlag);

/**
 * @brief	Set integer device configuration parameter
 * @param	key: Parameter name
 * @param	value: New parameter value
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_SetParameter_Int(const char* key, int32_t value, uint32_t timeout);

/**
 * @brief	Set floating-point device configuration parameter
 * @param	key: Parameter name
 * @param	value: New parameter value
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_SetParameter_Float(const char* key, float value, uint32_t timeout);

/**
 * @brief	Set string device configuration parameter
 * @param	key: Parameter name
 * @param	value: Pointer to new parameter value
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_SetParameter_String(const char* key, const char* value, uint32_t timeout);

/**
 * @brief	Update device configuration parameter value
 * @param	key: Parameter name
 * @param	parameter: Pointer to new parameter value
 * @param	paramSize: Parameter value size in bytes
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_UpdateParamValue(const char* key, char* parameter, uint16_t paramSize, uint32_t timeout);

/**
 * @brief	Test charger board EEPROM presence
 * @param	present: Pointer to variable where charger EEPROM presence status will be stored
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_CHARGER_TestBD(uint8_t* present, uint32_t timeout);

/**
 * @brief	Update charger configuration parameters from charger board EEPROM
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_CHARGER_UpdateFromBD(uint32_t timeout);

/**
 * @brief	Store current charger configuration parameters to charger board EEPROM
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_CHARGER_StoreToBD(uint32_t timeout);

/**
 * @brief	Update charger configuration parameter value
 * @param	key: Parameter name
 * @param	parameter: Pointer to new parameter value
 * @param	paramSize: Parameter value size in bytes
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_CHARGER_UpdateParamValue(const char* key, char* parameter, uint16_t paramSize, uint32_t timeout);

/**
 * @brief	Get charger configuration parameter by key
 * @param	key: Parameter name
 * @param	parameter: Pointer to buffer where parameter value will be stored
 * @param	paramSize: Pointer to variable where parameter value size will be stored
 * @param	defaultFlag: Pointer to variable indicating whether default value is used
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_CHARGER_GetParameter(const char* key, char* parameter, uint16_t* paramSize, uint8_t* defaultFlag);

/**
 * @brief	Erase charger board EEPROM and write complete new content
 * @param	data: Pointer to data to be written to charger board EEPROM
 * @param	size: Number of bytes to write
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_CHARGER_WriteFullBD(uint8_t* data, uint32_t size, uint32_t timeout);

/**
 * @brief	Get integer charger configuration parameter
 * @param	key: Parameter name
 * @param	value: Pointer to variable where parameter value will be stored
 * @param	defaultFlag: Pointer to variable indicating whether default value is used
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_CHARGER_GetParameter_Int(const char* key, int32_t* value, uint8_t* defaultFlag);

/**
 * @brief	Get floating-point charger configuration parameter
 * @param	key: Parameter name
 * @param	value: Pointer to variable where parameter value will be stored
 * @param	defaultFlag: Pointer to variable indicating whether default value is used
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_CHARGER_GetParameter_Float(const char* key, float* value, uint8_t* defaultFlag);

/**
 * @brief	Get string charger configuration parameter
 * @param	key: Parameter name
 * @param	buffer: Pointer to destination buffer
 * @param	bufferSize: Size of destination buffer in bytes
 * @param	defaultFlag: Pointer to variable indicating whether default value is used
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_CHARGER_GetParameter_String(const char* key, char* buffer, uint16_t bufferSize, uint8_t* defaultFlag);

/**
 * @brief	Set integer charger configuration parameter
 * @param	key: Parameter name
 * @param	value: New parameter value
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_CHARGER_SetParameter_Int(const char* key, int32_t value, uint32_t timeout);

/**
 * @brief	Set floating-point charger configuration parameter
 * @param	key: Parameter name
 * @param	value: New parameter value
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_CHARGER_SetParameter_Float(const char* key, float value, uint32_t timeout);

/**
 * @brief	Set string charger configuration parameter
 * @param	key: Parameter name
 * @param	value: Pointer to new parameter value
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::configuration_status_t
 */
configuration_status_t CONFIGURATION_CHARGER_SetParameter_String(const char* key, const char* value, uint32_t timeout);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_MIDDLEWARES_SERVICES_CONFIGURATION_CONFIGURATION_H_ */
