/**
 ********************************************************************************
 * @file	configurationDef.h
 *
 * @brief	Configuration parameter definitions.
 * 			This file defines configuration parameter types, parameter structures,
 * 			and interfaces used to obtain default device and charger configuration
 * 			parameters.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	April 2026
 ********************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_CONFIGURATION_CONFIGURATIONDEF_H_
#define CORE_MIDDLEWARES_SERVICES_CONFIGURATION_CONFIGURATIONDEF_H_

#include "configuration.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup CONFIGURATION_DEF Configuration Definitions
 * @{
 */

/**
 * @defgroup CONFIGURATION_DEF_PUBLIC_TYPES Configuration definition public data types
 * @{
 */

/**
 * @brief Configuration parameter data type
 */
typedef enum
{
    CONFIGURATION_PARAM_TYPE_STRING = 0,    /*!< String parameter */
    CONFIGURATION_PARAM_TYPE_INT,           /*!< Integer parameter */
    CONFIGURATION_PARAM_TYPE_FLOAT          /*!< Floating-point parameter */
} configuration_param_type_t;

/**
 * @brief Configuration parameter structure
 */
typedef struct
{
    char name[CONFIGURATION_MAX_PARAM_VALUESIZE];      /*!< Parameter name */
    uint8_t value[CONFIGURATION_MAX_PARAM_VALUESIZE];  /*!< Parameter value stored as string */
    configuration_param_type_t type;                   /*!< Parameter data type */
    uint8_t readOnly;                                  /*!< 1 = read-only, 0 = writable */
    uint8_t defaultValue;                              /*!< 1 = default value is currently used, 0 = configured value is used */
    uint8_t systemParam;                               /*!< 1 = system parameter, 0 = user configuration parameter */
} configuration_param_t;

/**
 * @}
 */

/**
 * @defgroup CONFIGURATION_DEF_PUBLIC_FUNCTIONS Configuration definition interface functions
 * @{
 */

/**
 * @brief	Get default device configuration parameters
 * @retval	Pointer to the array of default configuration parameters
 */
configuration_param_t* 	CONFIGURATIONDEF_GetDefaults(void);

/**
 * @brief	Get number of default device configuration parameters
 * @retval	Number of default configuration parameters
 */
uint32_t 				CONFIGURATIONDEF_GetDefaultsCount(void);

/**
 * @brief	Get default charger configuration parameters
 * @retval	Pointer to the array of default charger configuration parameters
 */
configuration_param_t* 	CONFIGURATIONDEF_CHARGER_GetDefaults(void);

/**
 * @brief	Get number of default charger configuration parameters
 * @retval	Number of default charger configuration parameters
 */
uint32_t 				CONFIGURATIONDEF_CHARGER_GetDefaultsCount(void);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_MIDDLEWARES_SERVICES_CONFIGURATION_CONFIGURATIONDEF_H_ */

