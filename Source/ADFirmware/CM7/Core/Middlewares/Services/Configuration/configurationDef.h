/*
 * configurationDef.h
 *
 *  Created on: Apr 28, 2026
 *      Author: elektronika
 */

#ifndef CORE_MIDDLEWARES_SERVICES_CONFIGURATION_CONFIGURATIONDEF_H_
#define CORE_MIDDLEWARES_SERVICES_CONFIGURATION_CONFIGURATIONDEF_H_

typedef enum
{
    CONFIGURATION_PARAM_TYPE_STRING = 0,
    CONFIGURATION_PARAM_TYPE_INT,
    CONFIGURATION_PARAM_TYPE_FLOAT
} configuration_param_type_t;

typedef struct
{
    const char* name;        /*!< Parameter name */
    char        value[64];   /*!< Value stored as string */
    configuration_param_type_t type; /*!< Parameter type */

    uint8_t     readOnly;    /*!< 1 = read-only, 0 = writable */
    uint8_t     defaultValue;
} configuration_param_t;

configuration_param_t* CONFIGURATIONDEF_GetDefaults(void);
uint32_t CONFIGURATIONDEF_GetDefaultsCount(void);

#endif /* CORE_MIDDLEWARES_SERVICES_CONFIGURATION_CONFIGURATIONDEF_H_ */
