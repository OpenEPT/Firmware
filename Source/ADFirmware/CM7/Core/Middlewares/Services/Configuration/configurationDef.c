#include "configuration.h"
#include "globalConfig.h"
#include "configurationDef.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)



/**
 * @brief Default configuration table
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
			.value = "1.0.0",
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
