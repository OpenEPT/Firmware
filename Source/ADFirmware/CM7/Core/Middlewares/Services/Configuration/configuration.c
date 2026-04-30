/**
 ******************************************************************************
 * @file    configuration.c
 *
 * @brief   Configuration service implementation.
 *          Contains central FreeRTOS task with basic state machine.
 *
 * @author  Haris Turkmanovic
 * @date    April 2026
 ******************************************************************************
 */

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "configuration.h"
#include "configurationDef.h"
#include "logging.h"
#include "system.h"
#include "fsystem.h"

/**
 * @defgroup CONFIGURATION_PRIVATE_STRUCTURES Configuration private structures
 * @{
 */

/**
 * @brief Configuration internal data structure
 */
typedef struct
{
    configuration_state_t state;
    SemaphoreHandle_t     initSig;
    SemaphoreHandle_t     guard;
    TaskHandle_t          taskHandle;

} configuration_data_t;

/**
 * @}
 */

/**
 * @defgroup CONFIGURATION_PRIVATE_DATA Configuration private data
 * @{
 */

static configuration_data_t prvCONFIGURATION_DATA;

/**
 * @}
 */

/**
 * @defgroup CONFIGURATION_PRIVATE_FUNCTIONS Configuration private functions
 * @{
 */

/**
 * @brief Get parameter by key
 *
 * @param key Parameter name
 * @return Pointer to parameter or NULL if not found
 */
static configuration_param_t* prvCONFIGURATION_GetParam(const char* key)
{
    configuration_param_t* params;
    uint32_t count;
    uint32_t i;

    if(key == NULL)
        return NULL;

    params = CONFIGURATIONDEF_GetDefaults();
    count  = CONFIGURATIONDEF_GetDefaultsCount();

    for(i = 0; i < count; i++)
    {
        if(strcmp(params[i].name, key) == 0)
        {
            return &params[i];
        }
    }

    return NULL;
}


/**
 * @brief Update configuration parameters from FS
 */
static void prvCONFIGURATION_UpdateFromFS(void)
{
    char fileBuffer[CONFIGURATION_FILE_MAX_SIZE];
    uint32_t fileSize = 0;

    char* line;
    char* key;
    char* value;
    char* saveptr1;
    char* saveptr2;

    /* Get file from FS */
    if(FSYSTEM_GetFileFromPath(
            CONFIGURATION_FILE_PATH,
            strlen(CONFIGURATION_FILE_PATH),
            fileBuffer,
            CONFIGURATION_FILE_MAX_SIZE,
            &fileSize) != FSYSTEM_STATUS_OK)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_WARNING,
                      "Config file not found, using defaults\r\n");
        return;
    }

    /* Ensure null termination */
    if(fileSize >= CONFIGURATION_FILE_MAX_SIZE)
        fileSize = CONFIGURATION_FILE_MAX_SIZE - 1;

    fileBuffer[fileSize] = '\0';

    /* Parse line by line */
    line = strtok_r(fileBuffer, "\r\n", &saveptr1);

    while(line != NULL)
    {
        /* Split KEY:VALUE */
        key = strtok_r(line, ":", &saveptr2);
        value = strtok_r(NULL, ":", &saveptr2);

        if((key != NULL) && (value != NULL))
        {
            configuration_param_t* param = prvCONFIGURATION_GetParam(key);

            if(param != NULL)
            {
                /* Skip read-only parameters */
                if(param->readOnly == 0)
                {
                    /* Copy value safely */
                    strncpy(param->value, value, sizeof(param->value) - 1);
                    param->value[sizeof(param->value) - 1] = '\0';

                    /* Mark as non-default */
                    param->defaultValue = 0;
                }
            }
        }

        line = strtok_r(NULL, "\r\n", &saveptr1);
    }

    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO,"Configuration loaded from FS\r\n");
}



/**
 * @brief Main task function
 */
static void prvCONFIGURATION_Task(void *pvParameters)
{
    for(;;)
    {
        switch(prvCONFIGURATION_DATA.state)
        {
        case CONFIGURATION_STATE_INIT:

            /***************************************************************
             * INIT STATE
             ***************************************************************/

            LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Configuration init\r\n");

            prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_SERVICE;

            xSemaphoreGive(prvCONFIGURATION_DATA.initSig);

            break;

        case CONFIGURATION_STATE_SERVICE:

            /***************************************************************
             * SERVICE STATE
             ***************************************************************/

            vTaskDelay(portMAX_DELAY);

            break;

        case CONFIGURATION_STATE_ERROR:

            /***************************************************************
             * ERROR STATE
             ***************************************************************/

            SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);

            vTaskDelay(portMAX_DELAY);

            break;

        default:
            break;
        }
    }
}

/**
 * @}
 */

/**
 * @defgroup CONFIGURATION_PUBLIC_FUNCTIONS Configuration public functions
 * @{
 */

configuration_status_t CONFIGURATION_Init(uint32_t initTimeout)
{
    memset(&prvCONFIGURATION_DATA, 0, sizeof(configuration_data_t));

    prvCONFIGURATION_DATA.initSig = xSemaphoreCreateBinary();
    if(prvCONFIGURATION_DATA.initSig == NULL)
        return CONFIGURATION_STATUS_ERROR;

    prvCONFIGURATION_DATA.guard = xSemaphoreCreateMutex();
    if(prvCONFIGURATION_DATA.guard == NULL)
        return CONFIGURATION_STATUS_ERROR;

    prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_INIT;

    if(xTaskCreate(
            prvCONFIGURATION_Task,
            CONFIGURATION_TASK_NAME,
            CONFIGURATION_TASK_STACK,
            NULL,
            CONFIGURATION_TASK_PRIO,
            &prvCONFIGURATION_DATA.taskHandle) != pdPASS)
    {
        return CONFIGURATION_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig,
            pdMS_TO_TICKS(initTimeout)) != pdPASS)
    {
        return CONFIGURATION_STATUS_ERROR;
    }

    return CONFIGURATION_STATUS_OK;
}

/**
 * @}
 */
