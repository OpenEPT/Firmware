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
    {
        .name = "MAC_ADDRESS",
        .value = "00:11:22:33:44:55",
        .type = CONFIGURATION_PARAM_TYPE_STRING,
        .readOnly = 1,
        .defaultValue = 1
    },
    {
        .name = "IP_ADDRESS",
        .value = CONF_NETWORK_DEVICE_IP_ADDRESS,
        .type = CONFIGURATION_PARAM_TYPE_STRING,
        .readOnly = 0,
        .defaultValue = 1
    },
    {
        .name = "IP_MASK",
        .value = CONF_NETWORK_DEVICE_IP_MASK,
        .type = CONFIGURATION_PARAM_TYPE_STRING,
        .readOnly = 0,
        .defaultValue = 1
    },
    {
        .name = "IP_GATEWAY",
        .value = CONF_NETWORK_DEVICE_IP_GW,
        .type = CONFIGURATION_PARAM_TYPE_STRING,
        .readOnly = 0,
        .defaultValue = 1
    },
    {
        .name = "DEFAULT_TIMEOUT",
        .value = "1000",
        .type = CONFIGURATION_PARAM_TYPE_INT,
        .readOnly = 0,
        .defaultValue = 1
    },
    {
        .name = "PROTECTIONS_UVOLTAGE_VALUE",
        .value = STR(CONF_DPCONTROL_UV_VALUE),
        .type = CONFIGURATION_PARAM_TYPE_FLOAT,
        .readOnly = 0,
        .defaultValue = 1
    },
    {
        .name = "PROTECTIONS_OVOLTAGE_VALUE",
        .value = STR(CONF_DPCONTROL_OV_VALUE),
        .type = CONFIGURATION_PARAM_TYPE_FLOAT,
        .readOnly = 0,
        .defaultValue = 1
    },
    {
        .name = "PROTECTIONS_OCURRENT_VALUE",
        .value = STR(CONF_DPCONTROL_OC_VALUE),
        .type = CONFIGURATION_PARAM_TYPE_INT,
        .readOnly = 0,
        .defaultValue = 1
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
