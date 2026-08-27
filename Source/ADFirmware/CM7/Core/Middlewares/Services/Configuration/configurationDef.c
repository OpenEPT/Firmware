/**
 ********************************************************************************
 * @file	configurationDef.c
 *
 * @brief	Configuration parameter definitions implementation.
 * 			This file contains default device and charger configuration parameter
 * 			tables and provides functions for accessing the default parameter
 * 			values and parameter counts.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	April 2026
 ********************************************************************************
 */

#include "configuration.h"
#include "globalConfig.h"
#include "configurationDef.h"

/**
 * @brief Helper macro used for converting macro values to strings
 */
#define STR_HELPER(x) #x

/**
 * @brief Convert macro value to string
 */
#define STR(x) STR_HELPER(x)



/**
 * @brief Default device configuration parameter table
 *
 * Contains default values and properties of all configuration parameters
 * used by the main device.
 */
static configuration_param_t prvCONFIGURATION_DEFAULTS[] =
{
		//System parameters
		{
			.name = "DEV_NAME",
			.value = "Acq Device",
			.type = CONFIGURATION_PARAM_TYPE_STRING,
			.readOnly = 1,
			.defaultValue = 1,
			.systemParam = 0
		},
		{
			.name = "HW_SERIAL",
			.value = "0123456789",
			.type = CONFIGURATION_PARAM_TYPE_STRING,
			.readOnly = 1,
			.defaultValue = 1,
			.systemParam = 1
		},
		{
			.name = "FW_VERSION",
			.value = "2.0.0",
			.type = CONFIGURATION_PARAM_TYPE_STRING,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 1
		},
		{
			.name = "DEFAULT_TIMEOUT",
			.value = "1000",
			.type = CONFIGURATION_PARAM_TYPE_INT,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		},

		//Sensing parameters
		{
			.name = "SENS_SHUNT",
			.value = STR(CONF_DPCONTROL_SHUNT_VALUE),
			.type = CONFIGURATION_PARAM_TYPE_FLOAT,
			.readOnly = 1,
			.defaultValue = 1,
			.systemParam = 1
		},
		{
			.name = "SENS_GAIN",
			.value = STR(CONF_DPCONTROL_INA_GAIN),
			.type = CONFIGURATION_PARAM_TYPE_FLOAT,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 1   //
		},
		//Calibration parameters
		{
			.name = "CAL_V_REF",
			.value = STR(CONF_DPCONTROL_CAL_V_REF),
			.type = CONFIGURATION_PARAM_TYPE_FLOAT,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		},
		{
			.name = "CAL_V_OFF",
			.value = STR(CONF_DPCONTROL_CAL_V_OFF),
			.type = CONFIGURATION_PARAM_TYPE_FLOAT,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		},
		{
			.name = "CAL_V_COR",
			.value = STR(CONF_DPCONTROL_CAL_V_COR),
			.type = CONFIGURATION_PARAM_TYPE_FLOAT,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		},
		{
			.name = "CAL_C_OFF",
			.value = STR(CONF_DPCONTROL_CAL_C_OFF),
			.type = CONFIGURATION_PARAM_TYPE_FLOAT,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		},
		{
			.name = "CAL_C_COR",
			.value = STR(CONF_DPCONTROL_CAL_C_COR),
			.type = CONFIGURATION_PARAM_TYPE_FLOAT,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		},
		//Network parameters
		{
			.name = "MAC_ADDRESS",
			.value = CONF_NETWORK_DEVICE_MAC_ADDRESS,
			.type = CONFIGURATION_PARAM_TYPE_STRING,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 1
		},
		{
			.name = "IP_ADDRESS",
			.value = CONF_NETWORK_DEVICE_IP_ADDRESS,
			.type = CONFIGURATION_PARAM_TYPE_STRING,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		},
		{
			.name = "IP_MASK",
			.value = CONF_NETWORK_DEVICE_IP_MASK,
			.type = CONFIGURATION_PARAM_TYPE_STRING,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		},
		{
			.name = "IP_GATEWAY",
			.value = CONF_NETWORK_DEVICE_IP_GW,
			.type = CONFIGURATION_PARAM_TYPE_STRING,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		},
		{
			.name = "CONTROL_SER_PORT",
			.value = STR(CONF_CONTROL_SERVER_PORT),
			.type = CONFIGURATION_PARAM_TYPE_INT,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		},
		//Discharge control
		{
			.name = "PROTECTIONS_UVOLTAGE_VALUE",
			.value = STR(CONF_DPCONTROL_UV_VALUE),
			.type = CONFIGURATION_PARAM_TYPE_FLOAT,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		},
		{
			.name = "PROTECTIONS_OVOLTAGE_VALUE",
			.value = STR(CONF_DPCONTROL_OV_VALUE),
			.type = CONFIGURATION_PARAM_TYPE_FLOAT,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		},
		{
			.name = "PROTECTIONS_OCURRENT_VALUE",
			.value = STR(CONF_DPCONTROL_OC_VALUE),
			.type = CONFIGURATION_PARAM_TYPE_INT,
			.readOnly = 0,
			.defaultValue = 1,
			.systemParam = 0
		}
};

/**
 * @brief Default charger configuration parameter table
 *
 * Contains default values and properties of all configuration parameters
 * associated with the optional charger module.
 */
static configuration_param_t prvCONFIGURATION_CHARGER_DEFAULTS[] =
{
        {
            .name = "HW_SER",
            .value = "0123456789",
            .type = CONFIGURATION_PARAM_TYPE_STRING,
            .readOnly = 1,
            .defaultValue = 1,
            .systemParam = 1
        },
        {
            .name = "FW_VER",
            .value = "1.0.0",
            .type = CONFIGURATION_PARAM_TYPE_STRING,
            .readOnly = 0,
            .defaultValue = 1,
            .systemParam = 1
        },
        {
            .name = "CH_CUR",
            .value = "100",
            .type = CONFIGURATION_PARAM_TYPE_INT,
            .readOnly = 0,
            .defaultValue = 1,
            .systemParam = 1
        },
        {
            .name = "TERM_VOLT",
            .value = "4.2",
            .type = CONFIGURATION_PARAM_TYPE_FLOAT,
            .readOnly = 0,
            .defaultValue = 1,
            .systemParam = 1
        },
        {
            .name = "TERM_CUR",
            .value = "3",
            .type = CONFIGURATION_PARAM_TYPE_INT,
            .readOnly = 0,
            .defaultValue = 1,
            .systemParam = 1
        },
        {
            .name = "MAX_CUR",
            .value = "5",
            .type = CONFIGURATION_PARAM_TYPE_INT,
            .readOnly = 0,
            .defaultValue = 1,
            .systemParam = 1
        }
};

#define CONFIGURATION_CHARGER_DEFAULTS_COUNT \
    (sizeof(prvCONFIGURATION_CHARGER_DEFAULTS) / sizeof(configuration_param_t))

#define CONFIGURATION_DEFAULTS_COUNT \
    (sizeof(prvCONFIGURATION_DEFAULTS) / sizeof(configuration_param_t))

configuration_param_t* CONFIGURATIONDEF_GetDefaults(void)
{
    return prvCONFIGURATION_DEFAULTS;
}

uint32_t CONFIGURATIONDEF_GetDefaultsCount(void)
{
    return CONFIGURATION_DEFAULTS_COUNT;
}

configuration_param_t* CONFIGURATIONDEF_CHARGER_GetDefaults(void)
{
    return prvCONFIGURATION_CHARGER_DEFAULTS;
}

uint32_t CONFIGURATIONDEF_CHARGER_GetDefaultsCount(void)
{
    return CONFIGURATION_CHARGER_DEFAULTS_COUNT;
}
