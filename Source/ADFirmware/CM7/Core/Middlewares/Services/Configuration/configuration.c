/**
 ********************************************************************************
 * @file	configuration.c
 *
 * @brief	Configuration service implementation.
 *
 * 			Implements device and charger configuration management, including
 * 			loading and storing device parameters from the file system, accessing
 * 			system parameters stored in board memory, and managing charger
 * 			configuration stored in the charger board EEPROM.
 *
 * 			The service is implemented as a FreeRTOS task and uses task
 * 			notifications to process configuration operations.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	April 2026
 ********************************************************************************
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
#include "m24c32.h"
#include "at24cs01.h"

/**
 * @defgroup CONFIGURATION_PRIVATE_DEFINES Configuration private defines
 * @{
 */

#define CONFIGURATION_MASK_UPDATE_FROM_FS			0x00000001	/*!< Request configuration update from file system */
#define CONFIGURATION_MASK_SET_PARAM				0x00000002	/*!< Request device parameter update */
#define CONFIGURATION_MASK_SAVE_TO_FS				0x00000004	/*!< Request configuration storage to file system */
#define CONFIGURATION_MASK_CHARGER_TEST_BD			0x00000008	/*!< Request charger board EEPROM presence test */
#define CONFIGURATION_MASK_CHARGER_UPDATE_FROM_BD	0x00000010	/*!< Request charger configuration update from EEPROM */
#define CONFIGURATION_MASK_CHARGER_SAVE_TO_BD		0x00000020	/*!< Request charger configuration storage to EEPROM */
#define CONFIGURATION_MASK_CHARGER_SET_PARAM		0x00000040	/*!< Request charger parameter update */
#define CONFIGURATION_MASK_CHARGER_WRITE_FULL_BD	0x00000080	/*!< Request complete charger EEPROM write */
#define CONFIGURATION_MASK_CHARGER_READ_FULL_BD		0x00000100	/*!< Request complete charger EEPROM read */

/**
 * @}
 */
/**
 * @brief Configuration service internal data structure
 */
typedef struct
{
    configuration_state_t	state;										/*!< Current configuration service state */
    SemaphoreHandle_t		initSig;									/*!< Synchronization semaphore for configuration operations */
    SemaphoreHandle_t		guard;										/*!< Mutex protecting configuration data */
    TaskHandle_t			taskHandle;									/*!< Configuration service task handle */

    configuration_param_t	params[CONFIGURATION_MAX_PARAMS];			/*!< Runtime device configuration parameters */
    uint32_t				paramsCount;								/*!< Number of valid device configuration parameters */

    configuration_param_t	chargerParams[CONFIGURATION_MAX_PARAMS];	/*!< Runtime charger configuration parameters */
    uint32_t				chargerParamsCount;							/*!< Number of valid charger configuration parameters */
    uint8_t					chargerPresent;								/*!< Charger EEPROM presence flag */

    configuration_param_t	lastParam;									/*!< Pending device parameter update */
    configuration_param_t	lastChargerParam;							/*!< Pending charger parameter update */

    uint8_t					chargerBDData[AT24CS01_MEMORY_SIZE_BYTES];	/*!< Buffer used for complete charger EEPROM access */
    uint32_t				chargerBDDataSize;							/*!< Number of valid bytes in charger EEPROM buffer */

} configuration_data_t;

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
 * @brief	Get device configuration parameter from runtime table
 * @param	key: Parameter name
 * @retval	Pointer to ::configuration_param_t or NULL if parameter is not found
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
 * @brief	Serialize device configuration parameters to string
 * @param	buffer: Destination buffer
 * @param	maxSize: Maximum destination buffer size in bytes
 * @param	outSize: Pointer to variable where serialized data size will be stored
 * @retval	void
 */
static void prvCONFIGURATION_SerializeToString(char* buffer, uint32_t maxSize, uint32_t* outSize)
{
    uint32_t offset = 0;

    if(buffer == NULL || outSize == NULL || maxSize == 0)
        return;

    buffer[0] = '\0';
    *outSize = 0;

    for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
    {
        configuration_param_t* param = &prvCONFIGURATION_DATA.params[i];

        if(param == NULL || param->name == NULL || param->value == NULL)
            continue;

        if(param->systemParam == 1U)
            continue;

        size_t valueLen = strnlen((char*)param->value, CONFIGURATION_MAX_PARAM_VALUESIZE);

        int written = snprintf(&buffer[offset], maxSize - offset, "%s:%.*s\r\n", param->name, (int)valueLen, param->value);

        if(written <= 0 || (offset + (uint32_t)written) >= maxSize)
        {
            buffer[maxSize - 1] = '\0';
            break;
        }

        offset += (uint32_t)written;
    }

    *outSize = offset;
}
/**
 * @brief	Serialize and store device configuration to file system
 * @retval	void
 */
static void prvCONFIGURATION_SaveToFS(void)
{
    char buffer[CONFIGURATION_FILE_MAX_SIZE];
    uint32_t size = 0;

    prvCONFIGURATION_SerializeToString(buffer, sizeof(buffer), &size);

    FSYSTEM_WriteToFile(CONFIGURATION_FILE_PATH, strlen(CONFIGURATION_FILE_PATH), buffer, size);
}
/**
 * @brief	Initialize runtime device configuration table using default parameters
 * @retval	void
 */
static void prvCONFIGURATION_InitParams(void)
{
    configuration_param_t* defaults = CONFIGURATIONDEF_GetDefaults();
    uint32_t count = CONFIGURATIONDEF_GetDefaultsCount();

    prvCONFIGURATION_DATA.paramsCount = count;

    for(uint32_t i = 0; i < count; i++)
    {
        memcpy(&prvCONFIGURATION_DATA.params[i], &defaults[i], sizeof(configuration_param_t));
    }
}
/**
 * @brief	Report configuration parameters that are still using default values
 * @retval	void
 */
static void prvCONFIGURATION_ReportDefaultParams(void)
{
    for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
    {
        configuration_param_t* param = &prvCONFIGURATION_DATA.params[i];

        if(param == NULL || param->name == NULL || param->value == NULL)
            continue;

        if(param->defaultValue == 1U)
        {
            LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_WARNING, "Default param used: %s = %s\r\n", param->name, param->value);
        }
    }
}
/**
 * @brief	Update runtime device configuration parameters from file system
 * @retval	void
 */
static void prvCONFIGURATION_UpdateFromFS(void)
{
    char fileBuffer[CONFIGURATION_FILE_MAX_SIZE];
    uint32_t fileSize = 0;

    char* line;
    char* sep;
    char* key;
    char* value;
    char* saveptr;

    if(FSYSTEM_GetFileFromPath( CONFIGURATION_FILE_PATH, strlen(CONFIGURATION_FILE_PATH), fileBuffer, CONFIGURATION_FILE_MAX_SIZE, &fileSize) != FSYSTEM_STATUS_OK)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_WARNING, "Config file not found, using defaults\r\n");
        return;
    }

    if(fileSize >= CONFIGURATION_FILE_MAX_SIZE)
        fileSize = CONFIGURATION_FILE_MAX_SIZE - 1;

    fileBuffer[fileSize] = '\0';

    line = strtok_r(fileBuffer, "\r\n", &saveptr);

    while(line != NULL)
    {
        sep = strchr(line, ':');

        if(sep != NULL)
        {
            *sep = '\0';
            key = line;
            value = sep + 1;

            while(*key == ' ' || *key == '\t') key++;

            while(*value == ' ' || *value == '\t') value++;

            char* end = value + strlen(value) - 1;
            while(end > value && (*end == ' ' || *end == '\t'))
            {
                *end = '\0';
                end--;
            }

            for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
            {
                configuration_param_t* param = &prvCONFIGURATION_DATA.params[i];

                if(param->name == NULL || param->value == NULL)
                    continue;

                if(strcmp(param->name, key) == 0)
                {
                    size_t len = strnlen(value, CONFIGURATION_MAX_PARAM_VALUESIZE);

                    memcpy(param->value, value, len);
                    param->value[len] = '\0';

                    param->defaultValue = 0;

                    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Loaded param: %s = %s\r\n", param->name, param->value);

                    break;
                }
            }
        }

        line = strtok_r(NULL, "\r\n", &saveptr);
    }

    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Configuration loaded from FS\r\n");

}
/**
 * @brief	Update system configuration parameters from system board memory
 * @retval	void
 */
static void prvCONFIGURATION_UpdateSystemParamFromBD(void)
{
    uint8_t header[CONF_CONFIGURATION_HEADER_SIZE];
    uint32_t payloadSize = 0;

    char* line;
    char* sep;
    char* key;
    char* value;
    char* saveptr;

    static char payloadBuffer[CONFIGURATION_FILE_MAX_SIZE];

    if(M24C32_Read(0x0000, header, CONF_CONFIGURATION_HEADER_SIZE, 1000) != M24C32_STATUS_OK)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "System BD read failed (header)\r\n");
        return;
    }

    uint32_t magic = 0;
    memcpy(&magic, &header[0], sizeof(uint32_t));

    if(magic != 0xA5A6A7A8)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Invalid system MAGIC\r\n");
        return;
    }

    memcpy(&payloadSize, &header[4], sizeof(uint32_t));

    if(payloadSize == 0 || payloadSize >= CONFIGURATION_FILE_MAX_SIZE)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Invalid system payload size\r\n");
        return;
    }

    if(M24C32_Read(CONF_CONFIGURATION_HEADER_SIZE, (uint8_t*)payloadBuffer, payloadSize, 1000) != M24C32_STATUS_OK)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "System BD read failed (payload)\r\n");
        return;
    }

    payloadBuffer[payloadSize] = '\0';

    line = strtok_r(payloadBuffer, "\r\n", &saveptr);

    while(line != NULL)
    {
        sep = strchr(line, ':');

        if(sep != NULL)
        {
            *sep = '\0';
            key = line;
            value = sep + 1;

            while(*key == ' ' || *key == '\t') key++;

            while(*value == ' ' || *value == '\t') value++;

            char* end = value + strlen(value) - 1;
            while(end > value && (*end == ' ' || *end == '\t'))
            {
                *end = '\0';
                end--;
            }

            for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
            {
                configuration_param_t* param = &prvCONFIGURATION_DATA.params[i];

                if(param->name == NULL || param->value == NULL)
                    continue;

                if(param->systemParam == 0U)
                    continue;

                if(strcmp(param->name, key) == 0)
                {
                    size_t len = strnlen(value, CONFIGURATION_MAX_PARAM_VALUESIZE);

                    memcpy(param->value, value, len);
                    param->value[len] = '\0';

                    param->defaultValue = 0;

                    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "System param: %s = %s\r\n", param->name, param->value);

                    break;
                }
            }
        }

        line = strtok_r(NULL, "\r\n", &saveptr);
    }

    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "System configuration loaded from BD\r\n");
}




/**
 * @brief	Initialize runtime charger configuration table using default parameters
 * @retval	void
 */
static void prvCONFIGURATION_CHARGER_InitParams(void)
{
    configuration_param_t* defaults = CONFIGURATIONDEF_CHARGER_GetDefaults();
    uint32_t count = CONFIGURATIONDEF_CHARGER_GetDefaultsCount();

    if(count > CONFIGURATION_MAX_PARAMS)
    {
        count = CONFIGURATION_MAX_PARAMS;
    }

    prvCONFIGURATION_DATA.chargerParamsCount = count;

    for(uint32_t i = 0; i < count; i++)
    {
        memcpy(&prvCONFIGURATION_DATA.chargerParams[i], &defaults[i], sizeof(configuration_param_t));
    }
}

/**
 * @brief	Get charger configuration parameter from runtime charger table
 * @param	key: Parameter name
 * @retval	Pointer to ::configuration_param_t or NULL if parameter is not found
 */
static configuration_param_t* prvCONFIGURATION_CHARGER_GetParam(const char* key)
{
    if(key == NULL)
        return NULL;

    for(uint32_t i = 0; i < prvCONFIGURATION_DATA.chargerParamsCount; i++)
    {
        configuration_param_t* param = &prvCONFIGURATION_DATA.chargerParams[i];

        if(strcmp(param->name, key) == 0)
        {
            return param;
        }
    }

    return NULL;
}

/**
 * @brief	Calculate CRC32 checksum
 * @param	data: Pointer to input data
 * @param	size: Input data size in bytes
 * @retval	Calculated CRC32 value
 *
 * @note	The implementation uses the same polynomial as zlib.crc32.
 */
static uint32_t prvCONFIGURATION_CRC32(const uint8_t* data, uint32_t size)
{
    uint32_t crc = 0xFFFFFFFFU;

    for(uint32_t i = 0; i < size; i++)
    {
        crc ^= data[i];

        for(uint32_t bit = 0; bit < 8U; bit++)
        {
            if((crc & 1U) != 0U)
                crc = (crc >> 1U) ^ 0xEDB88320U;
            else
                crc >>= 1U;
        }
    }

    return crc ^ 0xFFFFFFFFU;
}

/**
 * @brief	Test charger board EEPROM presence
 * @retval	void
 */
static void prvCONFIGURATION_CHARGER_TestBD(void)
{
    if(AT24CS01_Ping(1000U) == AT24CS01_STATUS_OK)
    {
        prvCONFIGURATION_DATA.chargerPresent = 1U;
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Charger EEPROM detected\r\n");
    }
    else
    {
        prvCONFIGURATION_DATA.chargerPresent = 0U;
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_WARNING, "Charger EEPROM not detected\r\n");
    }
}

/**
 * @brief	Update runtime charger configuration parameters from charger EEPROM
 * @retval	void
 *
 * @note	The EEPROM image is validated using the configuration magic value
 * 			and CRC32 before parameters are applied.
 */
static void prvCONFIGURATION_CHARGER_UpdateFromBD(void)
{
    uint8_t header[CONF_CONFIGURATION_HEADER_SIZE];
    uint32_t magic = 0U;
    uint32_t payloadSize = 0U;
    uint32_t storedCrc = 0U;
    uint32_t calculatedCrc;
    char payloadBuffer[AT24CS01_MEMORY_SIZE_BYTES];
    char* line;
    char* sep;
    char* key;
    char* value;
    char* saveptr;

    if(prvCONFIGURATION_DATA.chargerPresent == 0U)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_WARNING, "Charger EEPROM is not present\r\n");
        return;
    }

    if(AT24CS01_Read(0U, header, CONF_CONFIGURATION_HEADER_SIZE, 1000U) != AT24CS01_STATUS_OK)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Charger EEPROM read failed (header)\r\n");
        return;
    }

    memcpy(&magic, &header[0], sizeof(uint32_t));
    memcpy(&payloadSize, &header[4], sizeof(uint32_t));

    if(magic != 0xA5A6A7A8U)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Invalid charger configuration MAGIC\r\n");
        return;
    }

    if((payloadSize == 0U) || ((CONF_CONFIGURATION_HEADER_SIZE + payloadSize + sizeof(uint32_t)) > AT24CS01_MEMORY_SIZE_BYTES))
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Invalid charger payload size\r\n");
        return;
    }

    if(AT24CS01_Read(CONF_CONFIGURATION_HEADER_SIZE, (uint8_t*)payloadBuffer, (uint16_t)payloadSize, 1000U) != AT24CS01_STATUS_OK)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Charger EEPROM read failed (payload)\r\n");
        return;
    }

    if(AT24CS01_Read((uint8_t)(CONF_CONFIGURATION_HEADER_SIZE + payloadSize), (uint8_t*)&storedCrc, sizeof(storedCrc), 1000U) != AT24CS01_STATUS_OK)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Charger EEPROM read failed (CRC)\r\n");
        return;
    }

    calculatedCrc = prvCONFIGURATION_CRC32((uint8_t*)payloadBuffer, payloadSize);

    if(calculatedCrc != storedCrc)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Invalid charger configuration CRC\r\n");
        return;
    }

    payloadBuffer[payloadSize] = '\0';
    line = strtok_r(payloadBuffer, "\r\n", &saveptr);

    while(line != NULL)
    {
        sep = strchr(line, ':');

        if(sep != NULL)
        {
            *sep = '\0';
            key = line;
            value = sep + 1;

            while(*key == ' ' || *key == '\t') key++;
            while(*value == ' ' || *value == '\t') value++;

            char* end = value + strlen(value);
            while((end > value) && ((end[-1] == ' ') || (end[-1] == '\t')))
            {
                end--;
                *end = '\0';
            }

            configuration_param_t* param = prvCONFIGURATION_CHARGER_GetParam(key);

            if(param != NULL)
            {
                size_t len = strnlen(value, CONFIGURATION_MAX_PARAM_VALUESIZE - 1U);

                memcpy(param->value, value, len);
                param->value[len] = '\0';
                param->defaultValue = 0U;

                LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Charger param: %s = %s\r\n", param->name, param->value);
            }
        }

        line = strtok_r(NULL, "\r\n", &saveptr);
    }
}

/**
 * @brief	Serialize and store charger configuration parameters to charger EEPROM
 * @retval	void
 *
 * @note	The stored image contains configuration header, serialized parameter
 * 			payload and CRC32 checksum.
 */
static void prvCONFIGURATION_CHARGER_SaveToBD(void)
{
    uint8_t eraseBuffer[AT24CS01_MEMORY_SIZE_BYTES];
    uint8_t image[AT24CS01_MEMORY_SIZE_BYTES];
    char payload[AT24CS01_MEMORY_SIZE_BYTES];
    uint32_t offset = 0U;
    uint32_t payloadSize;
    uint32_t crc;
    uint32_t magic = 0xA5A6A7A8U;

    if(prvCONFIGURATION_DATA.chargerPresent == 0U)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_WARNING, "Charger EEPROM is not present\r\n");
        return;
    }

    memset(payload, 0, sizeof(payload));

    for(uint32_t i = 0; i < prvCONFIGURATION_DATA.chargerParamsCount; i++)
    {
        configuration_param_t* param = &prvCONFIGURATION_DATA.chargerParams[i];

        int written = snprintf(&payload[offset], sizeof(payload) - offset, "%s:%s\r\n", param->name, param->value);

        if((written <= 0) || ((offset + (uint32_t)written) >= sizeof(payload)))
        {
            LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Charger configuration serialization overflow\r\n");
            return;
        }

        offset += (uint32_t)written;
    }

    payloadSize = offset;

    if((CONF_CONFIGURATION_HEADER_SIZE + payloadSize + sizeof(uint32_t)) > AT24CS01_MEMORY_SIZE_BYTES)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Charger configuration does not fit EEPROM\r\n");
        return;
    }

    crc = prvCONFIGURATION_CRC32((uint8_t*)payload, payloadSize);

    memset(eraseBuffer, 0, sizeof(eraseBuffer));

    if(AT24CS01_Write(0U, eraseBuffer, AT24CS01_MEMORY_SIZE_BYTES, 1000U) != AT24CS01_STATUS_OK)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Charger EEPROM erase failed\r\n");
        return;
    }

    memset(image, 0, sizeof(image));
    memcpy(&image[0], &magic, sizeof(magic));
    memcpy(&image[4], &payloadSize, sizeof(payloadSize));
    memcpy(&image[CONF_CONFIGURATION_HEADER_SIZE], payload, payloadSize);
    memcpy(&image[CONF_CONFIGURATION_HEADER_SIZE + payloadSize], &crc, sizeof(crc));

    if(AT24CS01_Write(0U, image, AT24CS01_MEMORY_SIZE_BYTES, 1000U) != AT24CS01_STATUS_OK)
    {
        LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Charger EEPROM write failed\r\n");
        return;
    }

    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Charger configuration stored to EEPROM\r\n");
}

/**
 * @brief	Configuration service main task
 * @param	pvParameters: FreeRTOS task parameters
 * @retval	void
 *
 * @details	The task implements the Configuration service state machine.
 * 			During initialization it loads default, file-system and system-board
 * 			configuration. In service state it processes device and charger
 * 			configuration requests received through FreeRTOS task notifications.
 */
static void prvCONFIGURATION_Task(void *pvParameters)
{
    for(;;)
    {
        switch(prvCONFIGURATION_DATA.state)
        {
        case CONFIGURATION_STATE_INIT:

            /* Initialize device configuration parameters. */
            prvCONFIGURATION_InitParams();

            /* Initialize charger configuration parameters. */
            prvCONFIGURATION_CHARGER_InitParams();

            LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Configuration initialized\r\n");

            /* Load device configuration from the file system. */
            prvCONFIGURATION_UpdateFromFS();

            LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Configuration updated from FS\r\n");

            /* Load system parameters from block device. */
            prvCONFIGURATION_UpdateSystemParamFromBD();

            LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Configuration updated from System memory\r\n");

            /* Report parameters that are using default values. */
            prvCONFIGURATION_ReportDefaultParams();

            /* Switch to the service state. */
            prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_SERVICE;

            /* Signal that configuration initialization is complete. */
            xSemaphoreGive(prvCONFIGURATION_DATA.initSig);

            break;

        case CONFIGURATION_STATE_SERVICE:

            int32_t value;

            /* Wait for a configuration service request. */
            xTaskNotifyWait(0x0, 0xFFFFFFFF, &value, portMAX_DELAY);

            /* Process a request to reload configuration from the file system. */
            if(value & CONFIGURATION_MASK_UPDATE_FROM_FS)
            {
                prvCONFIGURATION_UpdateFromFS();

                xSemaphoreGive(prvCONFIGURATION_DATA.initSig);
            }

            /* Process a request to update a device parameter. */
            if(value & CONFIGURATION_MASK_SET_PARAM)
            {
                if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE)
                {
                    prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_ERROR;
                    break;
                }

                configuration_param_t* param = NULL;

                /* Find the requested parameter in the runtime table. */
                for(uint32_t i = 0; i < prvCONFIGURATION_DATA.paramsCount; i++)
                {
                    if(strcmp(prvCONFIGURATION_DATA.params[i].name, prvCONFIGURATION_DATA.lastParam.name) == 0)
                    {
                        param = &prvCONFIGURATION_DATA.params[i];
                        break;
                    }
                }

                /* Update the parameter if it exists and is writable. */
                if(param != NULL && param->readOnly == 0)
                {
                    strncpy(param->value, prvCONFIGURATION_DATA.lastParam.value, sizeof(param->value) - 1);
                    param->value[sizeof(param->value) - 1] = '\0';
                    param->defaultValue = 0;
                }

                xSemaphoreGive(prvCONFIGURATION_DATA.guard);
                xSemaphoreGive(prvCONFIGURATION_DATA.initSig);
            }

            /* Process a request to store device configuration to the file system. */
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

                LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Configuration stored to FS\r\n");
            }

            /* Process a request to test charger EEPROM presence. */
            if(value & CONFIGURATION_MASK_CHARGER_TEST_BD)
            {
                prvCONFIGURATION_CHARGER_TestBD();

                xSemaphoreGive(prvCONFIGURATION_DATA.initSig);
            }

            /* Process a request to load charger configuration from EEPROM. */
            if(value & CONFIGURATION_MASK_CHARGER_UPDATE_FROM_BD)
            {
                prvCONFIGURATION_CHARGER_UpdateFromBD();

                xSemaphoreGive(prvCONFIGURATION_DATA.initSig);
            }

            /* Process a request to store charger configuration to EEPROM. */
            if(value & CONFIGURATION_MASK_CHARGER_SAVE_TO_BD)
            {
                if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE)
                {
                    prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_ERROR;
                    break;
                }

                prvCONFIGURATION_CHARGER_SaveToBD();

                xSemaphoreGive(prvCONFIGURATION_DATA.guard);
                xSemaphoreGive(prvCONFIGURATION_DATA.initSig);
            }

            /* Process a request to update a charger parameter. */
            if(value & CONFIGURATION_MASK_CHARGER_SET_PARAM)
            {
                if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE)
                {
                    prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_ERROR;
                    break;
                }

                /* Find the requested charger parameter in the runtime table. */
                configuration_param_t* param = prvCONFIGURATION_CHARGER_GetParam(prvCONFIGURATION_DATA.lastChargerParam.name);

                /* Update the charger parameter if it exists and is writable. */
                if((param != NULL) && (param->readOnly == 0U))
                {
                    strncpy((char*)param->value, (char*)prvCONFIGURATION_DATA.lastChargerParam.value, sizeof(param->value) - 1U);
                    param->value[sizeof(param->value) - 1U] = '\0';
                    param->defaultValue = 0U;
                }

                xSemaphoreGive(prvCONFIGURATION_DATA.guard);
                xSemaphoreGive(prvCONFIGURATION_DATA.initSig);
            }

            /* Process a request to write the complete charger EEPROM content. */
            if(value & CONFIGURATION_MASK_CHARGER_WRITE_FULL_BD)
            {
                if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE)
                {
                    prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_ERROR;
                    break;
                }

                uint8_t eraseBuffer[AT24CS01_MEMORY_SIZE_BYTES];

                /* Prepare the EEPROM erase buffer. */
                memset(eraseBuffer, 0xFF, sizeof(eraseBuffer));

                /* Erase the complete charger EEPROM. */
                if(AT24CS01_Write(0U, eraseBuffer, AT24CS01_MEMORY_SIZE_BYTES, 1000U) != AT24CS01_STATUS_OK)
                {
                    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Charger EEPROM erase failed\r\n");
                }

                /* Write the new content to the charger EEPROM. */
                else if(AT24CS01_Write(0U, prvCONFIGURATION_DATA.chargerBDData, prvCONFIGURATION_DATA.chargerBDDataSize, 1000U) != AT24CS01_STATUS_OK)
                {
                    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Charger EEPROM write failed\r\n");
                }
                else
                {
                    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Charger EEPROM updated\r\n");
                }

                xSemaphoreGive(prvCONFIGURATION_DATA.guard);
                xSemaphoreGive(prvCONFIGURATION_DATA.initSig);
            }

            /* Process a request to read the complete charger EEPROM content. */
            if(value & CONFIGURATION_MASK_CHARGER_READ_FULL_BD)
            {
                if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE)
                {
                    prvCONFIGURATION_DATA.state = CONFIGURATION_STATE_ERROR;
                    break;
                }

                /* Clear the charger EEPROM data buffer. */
                memset(prvCONFIGURATION_DATA.chargerBDData, 0, AT24CS01_MEMORY_SIZE_BYTES);

                /* Read the complete charger EEPROM content. */
                if(AT24CS01_Read(0U, prvCONFIGURATION_DATA.chargerBDData, AT24CS01_MEMORY_SIZE_BYTES, 1000U) != AT24CS01_STATUS_OK)
                {
                    prvCONFIGURATION_DATA.chargerBDDataSize = 0U;

                    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_ERROR, "Charger EEPROM read failed\r\n");
                }
                else
                {
                    prvCONFIGURATION_DATA.chargerBDDataSize = AT24CS01_MEMORY_SIZE_BYTES;

                    LOGGING_Write("CONFIG", LOGGING_MSG_TYPE_INFO, "Charger EEPROM read successfully\r\n");
                }

                xSemaphoreGive(prvCONFIGURATION_DATA.guard);
                xSemaphoreGive(prvCONFIGURATION_DATA.initSig);
            }

            break;

        case CONFIGURATION_STATE_ERROR:

            /* Report the configuration service error. */
            SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);

            /* Suspend the configuration task. */
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

    if(xTaskCreate( prvCONFIGURATION_Task, CONFIGURATION_TASK_NAME, CONFIGURATION_TASK_STACK, NULL, CONFIGURATION_TASK_PRIO, &prvCONFIGURATION_DATA.taskHandle) != pdPASS)
    {
        return CONFIGURATION_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS)
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
configuration_status_t CONFIGURATION_GetParameter_Int(const char* key, int32_t* value, uint8_t* defaultFlag)
{
    if(key == NULL || value == NULL || defaultFlag == NULL) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_GetParam(key);

    if(param == NULL || param->type != CONFIGURATION_PARAM_TYPE_INT)
    {
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_ERROR;
    }

    *value = (int32_t)strtol((char*)param->value, NULL, 10);
    *defaultFlag = param->defaultValue;

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);
    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_GetParameter_Float(const char* key, float* value, uint8_t* defaultFlag)
{
    if(key == NULL || value == NULL || defaultFlag == NULL) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_GetParam(key);

    if(param == NULL || param->type != CONFIGURATION_PARAM_TYPE_FLOAT)
    {
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_ERROR;
    }

    float tmp;
	int ret = sscanf((char*)param->value, "%f", &tmp);

	if(ret != 1)
	{
		xSemaphoreGive(prvCONFIGURATION_DATA.guard);
		return CONFIGURATION_STATUS_ERROR;
	}

	*value = tmp;
	*defaultFlag = param->defaultValue;

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);
    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_GetParameter_String(const char* key, char* buffer, uint16_t bufferSize, uint8_t* defaultFlag)
{
    uint16_t size;

    if(key == NULL || buffer == NULL || defaultFlag == NULL || bufferSize == 0) return CONFIGURATION_STATUS_ERROR;

    if(CONFIGURATION_GetParameter(key, buffer, &size, defaultFlag) != CONFIGURATION_STATUS_OK) return CONFIGURATION_STATUS_ERROR;

    if(size >= bufferSize) return CONFIGURATION_STATUS_ERROR;

    buffer[size] = '\0';

    return CONFIGURATION_STATUS_OK;
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
configuration_status_t CONFIGURATION_SetParameter_String(const char* key, const char* value, uint32_t timeout)
{
    if(key == NULL || value == NULL) return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_UpdateParamValue(key, (char*)value, strlen(value), timeout);
}
configuration_status_t CONFIGURATION_SetParameter_Int(const char* key, int32_t value, uint32_t timeout)
{
    char buffer[32];

    if(key == NULL) return CONFIGURATION_STATUS_ERROR;

    int len = snprintf(buffer, sizeof(buffer), "%ld", value);
    if(len <= 0 || len >= sizeof(buffer)) return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_UpdateParamValue(key, buffer, len, timeout);
}
configuration_status_t CONFIGURATION_SetParameter_Float(const char* key, float value, uint32_t timeout)
{
    char buffer[32];

    if(key == NULL) return CONFIGURATION_STATUS_ERROR;

    int len = snprintf(buffer, sizeof(buffer), "%.4f", value);
    if(len <= 0 || len >= sizeof(buffer)) return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_UpdateParamValue(key, buffer, len, timeout);
}
configuration_status_t CONFIGURATION_StoreToFS(uint32_t timeout)
{
    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_SAVE_TO_FS, eSetBits) != pdTRUE) return CONFIGURATION_STATUS_ERROR;
    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return CONFIGURATION_STATUS_ERROR;
    return CONFIGURATION_STATUS_OK;
}


configuration_status_t CONFIGURATION_CHARGER_TestBD(uint8_t* present, uint32_t timeout)
{
    if(present == NULL)
        return CONFIGURATION_STATUS_ERROR;

    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_CHARGER_TEST_BD, eSetBits) != pdTRUE)
        return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS)
        return CONFIGURATION_STATUS_ERROR;

    *present = prvCONFIGURATION_DATA.chargerPresent;

    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_CHARGER_UpdateFromBD(uint32_t timeout)
{
    if(prvCONFIGURATION_DATA.chargerPresent == 0U)
        return CONFIGURATION_STATUS_ERROR;

    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_CHARGER_UPDATE_FROM_BD, eSetBits) != pdTRUE)
        return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS)
        return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_CHARGER_StoreToBD(uint32_t timeout)
{
    if(prvCONFIGURATION_DATA.chargerPresent == 0U)
        return CONFIGURATION_STATUS_ERROR;

    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_CHARGER_SAVE_TO_BD, eSetBits) != pdTRUE)
        return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS)
        return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_CHARGER_UpdateParamValue(const char* key, char* parameter, uint16_t paramSize, uint32_t timeout)
{
    if((key == NULL) || (parameter == NULL) || (paramSize == 0U))
        return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE)
        return CONFIGURATION_STATUS_ERROR;

    strncpy(prvCONFIGURATION_DATA.lastChargerParam.name, key, sizeof(prvCONFIGURATION_DATA.lastChargerParam.name) - 1U);

    strncpy((char*)prvCONFIGURATION_DATA.lastChargerParam.value, parameter, sizeof(prvCONFIGURATION_DATA.lastChargerParam.value) - 1U);

    prvCONFIGURATION_DATA.lastChargerParam.name[
        sizeof(prvCONFIGURATION_DATA.lastChargerParam.name) - 1U] = '\0';

    prvCONFIGURATION_DATA.lastChargerParam.value[
        sizeof(prvCONFIGURATION_DATA.lastChargerParam.value) - 1U] = '\0';

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);

    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_CHARGER_SET_PARAM, eSetBits) != pdTRUE)
        return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS)
        return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_CHARGER_GetParameter(const char* key, char* parameter, uint16_t* paramSize, uint8_t* defaultFlag)
{
    if((key == NULL) || (parameter == NULL) || (paramSize == NULL) || (defaultFlag == NULL))
        return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE)
        return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_CHARGER_GetParam(key);

    if(param == NULL)
    {
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_ERROR;
    }

    uint16_t len = strlen((char*)param->value);
    memcpy(parameter, param->value, len);
    parameter[len] = '\0';
    *paramSize = len;
    *defaultFlag = param->defaultValue;

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);

    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_CHARGER_WriteFullBD(uint8_t* data, uint32_t size, uint32_t timeout)
{
    if(data == NULL || size == 0U || size > AT24CS01_MEMORY_SIZE_BYTES) return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    memcpy(prvCONFIGURATION_DATA.chargerBDData, data, size);
    prvCONFIGURATION_DATA.chargerBDDataSize = size;

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);

    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_CHARGER_WRITE_FULL_BD, eSetBits) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_STATUS_OK;
}
configuration_status_t CONFIGURATION_CHARGER_ReadFullBD(uint8_t* data, uint32_t size, uint32_t timeout)
{
    if(data == NULL || size < AT24CS01_MEMORY_SIZE_BYTES) return CONFIGURATION_STATUS_ERROR;

    if(prvCONFIGURATION_DATA.chargerPresent == 0U) return CONFIGURATION_STATUS_ERROR;

    if(xTaskNotify(prvCONFIGURATION_DATA.taskHandle, CONFIGURATION_MASK_CHARGER_READ_FULL_BD, eSetBits) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    if(prvCONFIGURATION_DATA.chargerBDDataSize != AT24CS01_MEMORY_SIZE_BYTES)
    {
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_ERROR;
    }

    memcpy(data, prvCONFIGURATION_DATA.chargerBDData, AT24CS01_MEMORY_SIZE_BYTES);

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);

    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_CHARGER_GetParameter_Int(const char* key, int32_t* value, uint8_t* defaultFlag)
{
    if(key == NULL || value == NULL || defaultFlag == NULL) return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_CHARGER_GetParam(key);

    if(param == NULL || param->type != CONFIGURATION_PARAM_TYPE_INT)
    {
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_ERROR;
    }

    *value = (int32_t)strtol((char*)param->value, NULL, 10);
    *defaultFlag = param->defaultValue;

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);

    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_CHARGER_GetParameter_Float(const char* key, float* value, uint8_t* defaultFlag)
{
    if(key == NULL || value == NULL || defaultFlag == NULL) return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_CHARGER_GetParam(key);

    if(param == NULL || param->type != CONFIGURATION_PARAM_TYPE_FLOAT)
    {
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_ERROR;
    }

    float tmp;
    int ret = sscanf((char*)param->value, "%f", &tmp);

    if(ret != 1)
    {
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_ERROR;
    }

    *value = tmp;
    *defaultFlag = param->defaultValue;

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);

    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_CHARGER_GetParameter_String(const char* key, char* buffer, uint16_t bufferSize, uint8_t* defaultFlag)
{
    if(key == NULL || buffer == NULL || defaultFlag == NULL || bufferSize == 0U) return CONFIGURATION_STATUS_ERROR;

    if(xSemaphoreTake(prvCONFIGURATION_DATA.guard, portMAX_DELAY) != pdTRUE) return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_CHARGER_GetParam(key);

    if(param == NULL || param->type != CONFIGURATION_PARAM_TYPE_STRING)
    {
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_ERROR;
    }

    uint16_t len = strlen((char*)param->value);

    if(len >= bufferSize)
    {
        xSemaphoreGive(prvCONFIGURATION_DATA.guard);
        return CONFIGURATION_STATUS_ERROR;
    }

    memcpy(buffer, param->value, len);
    buffer[len] = '\0';
    *defaultFlag = param->defaultValue;

    xSemaphoreGive(prvCONFIGURATION_DATA.guard);

    return CONFIGURATION_STATUS_OK;
}

configuration_status_t CONFIGURATION_CHARGER_SetParameter_Int(const char* key, int32_t value, uint32_t timeout)
{
    if(key == NULL) return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_CHARGER_GetParam(key);

    if(param == NULL || param->type != CONFIGURATION_PARAM_TYPE_INT) return CONFIGURATION_STATUS_ERROR;

    char buffer[32];

    int len = snprintf(buffer, sizeof(buffer), "%ld", value);

    if(len <= 0 || len >= sizeof(buffer)) return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_CHARGER_UpdateParamValue(key, buffer, len, timeout);
}

configuration_status_t CONFIGURATION_CHARGER_SetParameter_Float(const char* key, float value, uint32_t timeout)
{
    if(key == NULL) return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_CHARGER_GetParam(key);

    if(param == NULL || param->type != CONFIGURATION_PARAM_TYPE_FLOAT) return CONFIGURATION_STATUS_ERROR;

    char buffer[32];

    int len = snprintf(buffer, sizeof(buffer), "%.4f", value);

    if(len <= 0 || len >= sizeof(buffer)) return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_CHARGER_UpdateParamValue(key, buffer, len, timeout);
}

configuration_status_t CONFIGURATION_CHARGER_SetParameter_String(const char* key, const char* value, uint32_t timeout)
{
    if(key == NULL || value == NULL) return CONFIGURATION_STATUS_ERROR;

    configuration_param_t* param = prvCONFIGURATION_CHARGER_GetParam(key);

    if(param == NULL || param->type != CONFIGURATION_PARAM_TYPE_STRING) return CONFIGURATION_STATUS_ERROR;

    return CONFIGURATION_CHARGER_UpdateParamValue(key, (char*)value, strlen(value), timeout);
}

/**
 * @}
 */
