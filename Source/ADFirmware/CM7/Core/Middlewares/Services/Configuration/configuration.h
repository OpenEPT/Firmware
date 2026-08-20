/**
 ******************************************************************************
 * @file    configuration.h
 *
 * @brief   Configuration service - template implementation.
 *          Provides infrastructure for system configuration handling.
 *
 * @author  Haris Turkmanovic
 * @date    April 2026
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_CONFIGURATION_CONFIGURATION_H_
#define CORE_MIDDLEWARES_SERVICES_CONFIGURATION_CONFIGURATION_H_

#include <stdint.h>
#include "globalConfig.h"

/**
 * @defgroup CONFIGURATION_SERVICE Configuration Service
 * @{
 */

/**
 * @defgroup CONFIGURATION_PUBLIC_DEFINES Configuration public defines
 * @{
 */
#define CONFIGURATION_TASK_NAME      		CONF_CONFIGURATION_TASK_NAME
#define CONFIGURATION_TASK_PRIO      		CONF_CONFIGURATION_TASK_PRIO
#define CONFIGURATION_TASK_STACK     		CONF_CONFIGURATION_TASK_STACK_SIZE
#define CONFIGURATION_FILE_PATH      		CONF_CONFIGURATION_FILE_PATH
#define CONFIGURATION_FILE_MAX_SIZE  		CONF_CONFIGURATION_FILE_MAX_SIZE
#define CONFIGURATION_MAX_PARAMS     		CONF_CONFIGURATION_MAX_PARAMS
#define CONFIGURATION_MAX_PARAM_VALUESIZE	CONF_CONFIGURATION_MAX_PARAM_VALUESIZE
/**
 * @}
 */

/**
 * @defgroup CONFIGURATION_PUBLIC_TYPES Configuration public types
 * @{
 */

/**
 * @brief Return status of CONFIGURATION API
 */
typedef enum
{
    CONFIGURATION_STATUS_OK = 0,
    CONFIGURATION_STATUS_ERROR
} configuration_status_t;

/**
 * @brief Configuration service internal states
 */
typedef enum
{
    CONFIGURATION_STATE_INIT = 0,
    CONFIGURATION_STATE_SERVICE,
    CONFIGURATION_STATE_ERROR
} configuration_state_t;

/**
 * @}
 */

/**
 * @defgroup CONFIGURATION_PUBLIC_FUNCTIONS Configuration public functions
 * @{
 */

/**
 * @brief Initialize Configuration service
 */
configuration_status_t CONFIGURATION_Init(uint32_t initTimeout);
configuration_status_t CONFIGURATION_UpdateFromFS(uint32_t initTimeout);
configuration_status_t CONFIGURATION_StoreToFS(uint32_t initTimeout);
configuration_status_t CONFIGURATION_GetParameter(const char* key, char* parameter, uint16_t* paramSize, uint8_t* defaultFlag);
configuration_status_t CONFIGURATION_GetParameter_Int(const char* key, int32_t* value, uint8_t* defaultFlag);
configuration_status_t CONFIGURATION_GetParameter_Float(const char* key, float* value, uint8_t* defaultFlag);
configuration_status_t CONFIGURATION_GetParameter_String(const char* key, char* buffer, uint16_t bufferSize, uint8_t* defaultFlag);
configuration_status_t CONFIGURATION_SetParameter_Int(const char* key, int32_t value, uint32_t timeout);
configuration_status_t CONFIGURATION_SetParameter_Float(const char* key, float value, uint32_t timeout);
configuration_status_t CONFIGURATION_SetParameter_String(const char* key, const char* value, uint32_t timeout);


configuration_status_t CONFIGURATION_UpdateParamValue(const char* key, char* parameter, uint16_t paramSize, uint32_t timeout);

configuration_status_t CONFIGURATION_CHARGER_TestBD(uint8_t* present, uint32_t timeout);
configuration_status_t CONFIGURATION_CHARGER_UpdateFromBD(uint32_t timeout);
configuration_status_t CONFIGURATION_CHARGER_StoreToBD(uint32_t timeout);
configuration_status_t CONFIGURATION_CHARGER_UpdateParamValue(const char* key, char* parameter, uint16_t paramSize, uint32_t timeout);
configuration_status_t CONFIGURATION_CHARGER_GetParameter(const char* key, char* parameter, uint16_t* paramSize, uint8_t* defaultFlag);

/**
 * @}
 */

#endif /* CORE_MIDDLEWARES_SERVICES_CONFIGURATION_CONFIGURATION_H_ */
