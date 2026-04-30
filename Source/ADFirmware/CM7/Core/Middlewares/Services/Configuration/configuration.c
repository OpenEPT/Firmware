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
#define CONFIGURATION_MASK_UPDATE_FROM_FS   0x00000001
#define CONFIGURATION_MASK_SET_PARAM        0x00000002
#define CONFIGURATION_MASK_SAVE_TO_FS   0x00000004
/**
 * @brief Configuration internal data structure
 */
typedef struct
{
    configuration_state_t state;
    SemaphoreHandle_t     initSig;
    SemaphoreHandle_t     guard;
    TaskHandle_t          taskHandle;
    configuration_param_t params[CONFIGURATION_MAX_PARAMS];
    uint32_t              paramsCount;

    configuration_param_t lastParam;

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
 * @brief Get parameter by key from runtime table
 *
 * @param key Parameter name
 * @return Pointer to parameter or NULL if not found
 */
static configuration_param_t* prvCONFIGURATION_GetParam(const char* key)
{
    if(key == NULL)
        return NULL;

    for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
    {
        configuration_param_t* param = &prvCONFIGURATION_DATA.params[i];

        if(strcmp(param->name, key) == 0)
        {
            return param;
        }
    }

    return NULL;
}

/**
 * @brief Serialize all configuration parameters into string buffer
 *
 * @param buffer Output buffer
 * @param maxSize Maximum buffer size
 * @param outSize Actual written size
 */
static void prvCONFIGURATION_SerializeToString(char* buffer,
                                               uint32_t maxSize,
                                               uint32_t* outSize)
{
    uint32_t offset = 0;

    if(buffer == NULL || outSize == NULL)
        return;

    buffer[0] = '\0';
    *outSize = 0;

    for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
    {
        configuration_param_t* param = &prvCONFIGURATION_DATA.params[i];

        int written = snprintf(&buffer[offset],
                               maxSize - offset,
                               "%s:%s\r\n",
                               param->name,
                               param->value);

        /* Check for error or overflow */
        if(written <= 0 || (offset + written) >= maxSize)
        {
            break;
        }

        offset += written;
    }

    *outSize = offset;
}

static void prvCONFIGURATION_SaveToFS(void)
{
    char buffer[CONFIGURATION_FILE_MAX_SIZE];
    uint32_t size = 0;

    prvCONFIGURATION_SerializeToString(buffer,
                                       sizeof(buffer),
                                       &size);

    FSYSTEM_WriteToFile(CONFIGURATION_FILE_PATH,
                            strlen(CONFIGURATION_FILE_PATH),
                            buffer,
                            size);
}

static void prvCONFIGURATION_InitParams(void)
{
    configuration_param_t* defaults = CONFIGURATIONDEF_GetDefaults();
    uint32_t count = CONFIGURATIONDEF_GetDefaultsCount();

    prvCONFIGURATION_DATA.paramsCount = count;

    for(uint32_t i = 0; i < count; i++)
    {
        memcpy(&prvCONFIGURATION_DATA.params[i],
               &defaults[i],
               sizeof(configuration_param_t));
    }
}


static void prvCONFIGURATION_UpdateFromFS(void)
{
    char fileBuffer[CONFIGURATION_FILE_MAX_SIZE];
    uint32_t fileSize = 0;

    char* line;
    char* key;
    char* value;
    char* saveptr1;
    char* saveptr2;


    /* ===== 2. LOAD FROM FS ===== */
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

    if(fileSize >= CONFIGURATION_FILE_MAX_SIZE)
        fileSize = CONFIGURATION_FILE_MAX_SIZE - 1;

    fileBuffer[fileSize] = '\0';

    /* ===== 3. PARSE ===== */
    line = strtok_r(fileBuffer, "\r\n", &saveptr1);

    while(line != NULL)
    {
        key = strtok_r(line, ":", &saveptr2);
        value = strtok_r(NULL, ":", &saveptr2);

        if((key != NULL) && (value != NULL))
        {
            /* traži u runtime tabeli */
            for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
            {
                configuration_param_t* param = &prvCONFIGURATION_DATA.params[i];

                if(strcmp(param->name, key) == 0)
                {
					strncpy(param->value, value, sizeof(param->value) - 1);
					param->value[sizeof(param->value) - 1] = '\0';
					param->defaultValue = 0;
                    break;
                }
            }
        }

        line = strtok_r(NULL, "\r\n", &saveptr1);
    }

    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO,
                  "Configuration loaded from FS\r\n");

    /* ===== 4. PRINT PARAMETERS NOT UPDATED FROM FS ===== */
    for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
    {
        configuration_param_t* param = &prvCONFIGURATION_DATA.params[i];

        if(param->defaultValue == 1)
        {
            LOGGING_Write("CONFIG",
                          LOGGING_MSG_TYPE_WARNING,
                          "Default param used: %s = %s\r\n",
                          param->name,
                          param->value);
        }
    }
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
            prvCONFIGURATION_InitParams();
            LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Configuration initialized\r\n");

        	prvCONFIGURATION_UpdateFromFS();
            LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Configuration updated from FS\r\n");

            prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_SERVICE;

            xSemaphoreGive(prvCONFIGURATION_DATA.initSig);

            break;

        case CONFIGURATION_STATE_SERVICE:

        	int32_t value;

        	xTaskNotifyWait(0x0, 0xFFFFFFFF, &value, portMAX_DELAY);

        	/* ===== UPDATE FROM FS ===== */
        	if(value & CONFIGURATION_MASK_UPDATE_FROM_FS)
        	{
        	    prvCONFIGURATION_UpdateFromFS();
        	    xSemaphoreGive(prvCONFIGURATION_DATA.initSig);
        	}

        	/* ===== SET PARAM ===== */
        	if(value & CONFIGURATION_MASK_SET_PARAM)
        	{
        	    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE)
        	    {
        	        prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_ERROR;
        	        break;
        	    }

        	    configuration_param_t* param = NULL;

        	    for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
        	    {
        	        if(strcmp(prvCONFIGURATION_DATA.params[i].name,
        	                  prvCONFIGURATION_DATA.lastParam.name) == 0)
        	        {
        	            param = &prvCONFIGURATION_DATA.params[i];
        	            break;
        	        }
        	    }

        	    if(param != NULL && param->readOnly == 0)
        	    {
        	        strncpy(param->value,
        	                prvCONFIGURATION_DATA.lastParam.value,
        	                sizeof(param->value) - 1);

        	        param->value[sizeof(param->value) - 1] = '\0';
        	        param->defaultValue = 0;
        	    }

        	    xSemaphoreGive(prvCONFIGURATION_DATA.guard);

        	    xSemaphoreGive(prvCONFIGURATION_DATA.initSig);
        	}
        	if(value & CONFIGURATION_MASK_SAVE_TO_FS)
        	{
        	    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE)
        	    {
        	        prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_ERROR;
        	        break;
        	    }

        	    prvCONFIGURATION_SaveToFS();

        	    xSemaphoreGive(prvCONFIGURATION_DATA.guard);

        	    xSemaphoreGive(prvCONFIGURATION_DATA.initSig);

        	    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO,
        	                  "Configuration stored to FS\r\n");
        	}

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
configuration_status_t CONFIGURATION_UpdateFromFS(uint32_t timeout)
{
    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_UPDATE_FROM_FS, eSetBits) != pdTRUE) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return CONFIGURATION_STATUS_ERROR;
    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_GetParameter(const char* key, char* parameter, uint16_t* paramSize, uint8_t* defaultFlag)
{
    if(key == NULL || parameter == NULL || paramSize == NULL || defaultFlag == NULL) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_GetParam(key);

    if(param != NULL)
    {
        uint16_t len = strlen(param->value);
        memcpy(parameter, param->value, len);
        parameter[len] = '\0';
        *paramSize = len;
        *defaultFlag = param->defaultValue;
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_OK;
    }

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);
    return CONFIGURATION_STATUS_ERROR;
}

configuration_status_t CONFIGURATION_UpdateParamValue(const char* key, char* parameter, uint16_t paramSize, uint32_t timeout)
{
    if(key == NULL || parameter == NULL) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    strncpy(prvCONFIGURATION_DATA.lastParam.name, key, sizeof(prvCONFIGURATION_DATA.lastParam.name) - 1);
    strncpy(prvCONFIGURATION_DATA.lastParam.value, parameter, sizeof(prvCONFIGURATION_DATA.lastParam.value) - 1);

    prvCONFIGURATION_DATA.lastParam.name[sizeof(prvCONFIGURATION_DATA.lastParam.name) - 1] = '\0';
    prvCONFIGURATION_DATA.lastParam.value[sizeof(prvCONFIGURATION_DATA.lastParam.value) - 1] = '\0';

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);

    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_SET_PARAM, eSetBits) != pdTRUE) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_StoreToFS(uint32_t timeout)
{
    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_SAVE_TO_FS, eSetBits) != pdTRUE) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return CONFIGURATION_STATUS_ERROR;
    return CONFIGURATION_STATUS_OK;
}

/**
 * @}
 */
