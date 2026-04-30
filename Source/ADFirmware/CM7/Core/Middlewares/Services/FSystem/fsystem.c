/**
 ******************************************************************************
 * @file    fsystem.c
 *
 * @brief   File system service implementation.
 *          Contains central FreeRTOS task with basic state machine.
 *
 * @author  Haris Turkmanovic
 * @date    April 2026
 ******************************************************************************
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include <string.h>

#include "fsystem.h"
#include "system.h"
#include "logging.h"


#include "m24c32.h"

#include "littlefs/lfs.h"
#include "littlefs/bd/bd_driver_flash.h"

/**
 * @defgroup FSYSTEM_PRIVATE_STRUCTURES FSYSTEM private structures
 * @{
 */

typedef struct
{
	uint32_t offset;
	uint32_t size;
	uint8_t  buffer[FSYSTEM_BD_CHUNK_SIZE];
}fsystem_bd_chunk_info_t;

/**
 * @brief FSYSTEM internal data structure
 */
typedef struct
{
    fsystem_state_t state;
    SemaphoreHandle_t initSig;
    TaskHandle_t taskHandle;
    QueueHandle_t commandRequestQueue;
    QueueHandle_t commandResponseQueue;
    uint32_t 		commandIdCounter;


    /* LittleFS context */
    lfs_t lfs;
    struct lfs_config cfg;

    uint8_t readBuffer[32];
    uint8_t progBuffer[32];
    uint8_t lookaheadBuffer[32];

    uint8_t fsPresent;

} fsystem_data_t;

typedef enum
{
    FSYSTEM_COMMAND_TYPE_REQ = 0,
    FSYSTEM_COMMAND_TYPE_RES
} fsystem_command_type_t;

typedef enum
{
    FSYSTEM_COMMAND_READ_BD_CHUNK = 0,
    FSYSTEM_COMMAND_WRITE_BD_CHUNK,
    FSYSTEM_COMMAND_FORMAT_BD
} fsystem_command_t;

/**
 * @brief FSYSTEM command request
 */
typedef struct
{
    fsystem_command_type_t type;
    fsystem_command_t      command;
    uint32_t               commandId;
    fsystem_status_t       status;
    void*                  data;

} fsystem_command_request_t;

/**
 * @brief FSYSTEM command response
 */
typedef struct
{
    fsystem_command_type_t type;
    fsystem_command_t      command;
    uint32_t               commandId;
    fsystem_status_t       status;
    void*                  data;

} fsystem_command_response_t;

/**
 * @}
 */

/**
 * @defgroup FSYSTEM_PRIVATE_DATA FSYSTEM private data
 * @{
 */

static fsystem_data_t 			prvFSYSTEM_DATA;
static fsystem_bd_chunk_info_t 	prvFSYSTEM_LAST_BD_CHUNK;
/**
 * @}
 */

/**
 * @defgroup FSYSTEM_PRIVATE_FUNCTIONS FSYSTEM private functions
 * @{
 */

static uint32_t prvFSYSTEM_GetNextCommandId(void)
{
    return ++prvFSYSTEM_DATA.commandIdCounter;
}


/**
 * @brief FSYSTEM main task
 */
static void prvFSYSTEM_Task(void *pvParameters)
{
    (void) pvParameters;

    for(;;)
    {
        switch(prvFSYSTEM_DATA.state)
        {
        case FSYSTEM_STATE_INIT:
        {
            uint8_t tx[8] = {1,2,3,4,5,6,7,8};
            uint8_t rx[8] = {0};

            if(M24C32_Init() != M24C32_STATUS_OK)
            {
    			LOGGING_Write("File system", LOGGING_MSG_TYPE_ERROR, "Unable to initialize EEPROM\r\n");
    			prvFSYSTEM_DATA.state = FSYSTEM_STATE_ERROR;
    			break;
            }
			LOGGING_Write("File system", LOGGING_MSG_TYPE_INFO, "EEPROM Succesfully initialized\r\n");

            if(M24C32_Ping(1000) != M24C32_STATUS_OK)
            {
    			LOGGING_Write("File system", LOGGING_MSG_TYPE_ERROR, "Unable to establish communication with EEPROM\r\n");
    			prvFSYSTEM_DATA.state = FSYSTEM_STATE_ERROR;
    			break;
            }
			LOGGING_Write("File system", LOGGING_MSG_TYPE_INFO, "Communication with EEPROM Successfully established\r\n");

			if(prvFSYSTEM_DATA.fsPresent == 0)
			{

				if(M24C32_Write(0x0000, tx, sizeof(tx), 1000) != M24C32_STATUS_OK)
					Error_Handler();

				if(M24C32_Read(0x0000, rx, sizeof(rx), 1000) != M24C32_STATUS_OK)
					Error_Handler();

//				/* LFS CONFIG */
//				prvFSYSTEM_DATA.cfg.read  = bd_driver_flash_read;
//				prvFSYSTEM_DATA.cfg.prog  = bd_driver_flash_prog;
//				prvFSYSTEM_DATA.cfg.erase = bd_driver_flash_erase;
//				prvFSYSTEM_DATA.cfg.sync  = bd_driver_flash_sync;
//
//				prvFSYSTEM_DATA.cfg.context = NULL;
//
//				prvFSYSTEM_DATA.cfg.read_size = 1;
//				prvFSYSTEM_DATA.cfg.prog_size = 1;
//
//				prvFSYSTEM_DATA.cfg.block_size = 32;
//				prvFSYSTEM_DATA.cfg.block_count = 128;
//
//				prvFSYSTEM_DATA.cfg.cache_size = 32;
//				prvFSYSTEM_DATA.cfg.lookahead_size = 32;
//				prvFSYSTEM_DATA.cfg.block_cycles = 100;
//
//				prvFSYSTEM_DATA.cfg.read_buffer = prvFSYSTEM_DATA.readBuffer;
//				prvFSYSTEM_DATA.cfg.prog_buffer = prvFSYSTEM_DATA.progBuffer;
//				prvFSYSTEM_DATA.cfg.lookahead_buffer = prvFSYSTEM_DATA.lookaheadBuffer;
//
//				int err = lfs_mount(&prvFSYSTEM_DATA.lfs, &prvFSYSTEM_DATA.cfg);
//
//				if (err)
//				{
//					if (lfs_format(&prvFSYSTEM_DATA.lfs, &prvFSYSTEM_DATA.cfg) != 0)
//						Error_Handler();
//
//					if (lfs_mount(&prvFSYSTEM_DATA.lfs, &prvFSYSTEM_DATA.cfg) != 0)
//						Error_Handler();
//				}
			}

            LOGGING_Write("FSYSTEM", LOGGING_MSG_TYPE_INFO, "FSYSTEM initialized\r\n");

            prvFSYSTEM_DATA.state = FSYSTEM_STATE_SERVICE;
            xSemaphoreGive(prvFSYSTEM_DATA.initSig);
        }
        break;

        case FSYSTEM_STATE_SERVICE:

            /* TODO: Add main service logic */
        	fsystem_command_request_t req;
        	fsystem_command_response_t res;

        	if(xQueueReceive(prvFSYSTEM_DATA.commandRequestQueue, &req, portMAX_DELAY) == pdPASS)
        	{
        	    fsystem_command_response_t res;
        	    fsystem_bd_chunk_info_t* chunk = (fsystem_bd_chunk_info_t*)req.data;

        	    memset(&res, 0, sizeof(res));

        	    res.type = FSYSTEM_COMMAND_TYPE_RES;
        	    res.command = req.command;
        	    res.commandId = req.commandId;
        	    res.data = req.data;
        	    res.status = FSYSTEM_STATUS_ERROR;

        	    switch(req.command)
        	    {
        	        case FSYSTEM_COMMAND_READ_BD_CHUNK:
        	        {
        	            if(chunk != NULL && M24C32_Read(chunk->offset, chunk->buffer, chunk->size, 1000) == M24C32_STATUS_OK)
        	            {
        	                res.status = FSYSTEM_STATUS_OK;
        	            }
        	        }
        	        break;
        	        case FSYSTEM_COMMAND_WRITE_BD_CHUNK:
        	        {
        	            if(chunk != NULL && M24C32_Write(chunk->offset, chunk->buffer, chunk->size, 1000) == M24C32_STATUS_OK)
        	            {
        	                res.status = FSYSTEM_STATUS_OK;
        	            }
        	        }
        	        break;
        	        case FSYSTEM_COMMAND_FORMAT_BD:
        	        {
        	            uint8_t zeroBuffer[FSYSTEM_BD_CHUNK_SIZE];
        	            memset(zeroBuffer, 0x00, FSYSTEM_BD_CHUNK_SIZE);

        	            uint32_t totalSize = 4096;
        	            uint32_t offset = 0;

        	            while(offset < totalSize)
        	            {
        	                uint32_t writeSize = FSYSTEM_BD_CHUNK_SIZE;

        	                if((offset + writeSize) > totalSize)
        	                {
        	                    writeSize = totalSize - offset;
        	                }

        	                if(M24C32_Write(offset, zeroBuffer, writeSize, 1000) != M24C32_STATUS_OK)
        	                {
        	                    res.status = FSYSTEM_STATUS_ERROR;
        	                    break;
        	                }

        	                offset += writeSize;
        	            }

        	            if(offset >= totalSize)
        	            {
        	                res.status = FSYSTEM_STATUS_OK;
        	            }
        	        }
        	        break;

        	        default:
        	            break;
        	    }

        	    if(xQueueSend(prvFSYSTEM_DATA.commandResponseQueue, &res, 0) != pdPASS)
        	    {
        	        LOGGING_Write("FSYSTEM", LOGGING_MSG_TYPE_ERROR, "FSYSTEM: Response queue FULL\r\n");
        	    }
        	    else
        	    {
        	        if(res.status == FSYSTEM_STATUS_OK)
        	        {
        	            if(req.command == FSYSTEM_COMMAND_READ_BD_CHUNK)
        	            {
        	                LOGGING_Write("FSYSTEM", LOGGING_MSG_TYPE_INFO, "FSYSTEM: BD chunk read OK (offset=%lu, size=%lu)\r\n", chunk->offset, chunk->size);
        	            }
        	            else if(req.command == FSYSTEM_COMMAND_WRITE_BD_CHUNK)
        	            {
        	                LOGGING_Write("FSYSTEM", LOGGING_MSG_TYPE_INFO, "FSYSTEM: BD chunk write OK (offset=%lu, size=%lu)\r\n", chunk->offset, chunk->size);
        	            }
        	            else if(req.command == FSYSTEM_COMMAND_FORMAT_BD)
        	            {
        	                LOGGING_Write("FSYSTEM", LOGGING_MSG_TYPE_INFO, "FSYSTEM: BD FORMAT OK\r\n");
        	            }
        	        }
        	        else
        	        {
        	            if(req.command == FSYSTEM_COMMAND_READ_BD_CHUNK)
        	            {
        	                LOGGING_Write("FSYSTEM", LOGGING_MSG_TYPE_ERROR, "FSYSTEM: BD chunk read FAILED\r\n");
        	            }
        	            else if(req.command == FSYSTEM_COMMAND_WRITE_BD_CHUNK)
        	            {
        	                LOGGING_Write("FSYSTEM", LOGGING_MSG_TYPE_ERROR, "FSYSTEM: BD chunk write FAILED\r\n");
        	            }
        	            else if(req.command == FSYSTEM_COMMAND_FORMAT_BD)
        	            {
        	                LOGGING_Write("FSYSTEM", LOGGING_MSG_TYPE_ERROR, "FSYSTEM: BD FORMAT FAILED\r\n");
        	            }
        	        }
        	    }
        	}

        	break;

        case FSYSTEM_STATE_ERROR:

            /* TODO: Add error handling */

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
 * @defgroup FSYSTEM_PUBLIC_FUNCTIONS FSYSTEM public functions
 * @{
 */

fsystem_status_t FSYSTEM_Init(uint32_t initTimeout)
{

	memset(&prvFSYSTEM_DATA, 0, sizeof(fsystem_data_t));
	memset(&prvFSYSTEM_LAST_BD_CHUNK, 0, sizeof(fsystem_bd_chunk_info_t));

	prvFSYSTEM_DATA.commandRequestQueue = xQueueCreate(
	        FSYSTEM_CMD_QUEUE_LENGTH,
	        sizeof(fsystem_command_request_t));

	prvFSYSTEM_DATA.commandResponseQueue = xQueueCreate(
	        FSYSTEM_CMD_RESPONSE_LENGTH,
	        sizeof(fsystem_command_response_t));

	if((prvFSYSTEM_DATA.commandRequestQueue == NULL) || (prvFSYSTEM_DATA.commandResponseQueue == NULL))
	{
	    return FSYSTEM_STATUS_ERROR;
	}

    if(xTaskCreate(
            prvFSYSTEM_Task,
            FSYSTEM_TASK_NAME,
            FSYSTEM_TASK_STACK,
            NULL,
            FSYSTEM_TASK_PRIO,
            &prvFSYSTEM_DATA.taskHandle) != pdPASS)
    {
        return FSYSTEM_STATUS_ERROR;
    }

    prvFSYSTEM_DATA.initSig = xSemaphoreCreateBinary();

    if(prvFSYSTEM_DATA.initSig == NULL)
    {
        return FSYSTEM_STATUS_ERROR;
    }

    prvFSYSTEM_DATA.state = FSYSTEM_STATE_INIT;

    if(xSemaphoreTake(prvFSYSTEM_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS)
    {
        return FSYSTEM_STATUS_ERROR;
    }

    return FSYSTEM_STATUS_OK;
}
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

fsystem_status_t FSYSTEM_GetFileFromPath(char* path,
                                         uint32_t pathSize,
                                         char* dataBuffer,
                                         uint32_t dataBufferMaxSize,
                                         uint32_t* fileSize)
{
    (void)path;
    (void)pathSize;

    const char* configString =
    "MAC_ADDRESS:00:11:22:33:44:55\r\n"
    "IP_ADDRESS:" CONF_NETWORK_DEVICE_IP_ADDRESS "\r\n"
    "IP_MASK:" CONF_NETWORK_DEVICE_IP_MASK "\r\n"
    "IP_GATEWAY:" CONF_NETWORK_DEVICE_IP_GW "\r\n"
    "DEFAULT_TIMEOUT:1000\r\n"
    "PROTECTIONS_UVOLTAGE_VALUE:" STR(CONF_DPCONTROL_UV_VALUE) "\r\n"
    "PROTECTIONS_OVOLTAGE_VALUE:" STR(CONF_DPCONTROL_OV_VALUE) "\r\n"
    "PROTECTIONS_OCURRENT_VALUE:" STR(CONF_DPCONTROL_OC_VALUE) "\r\n";

    if(dataBuffer == NULL || fileSize == NULL)
        return FSYSTEM_STATUS_ERROR;

    uint32_t len = strlen(configString);

    /* +1 ako želiš i null terminator */
    if(dataBufferMaxSize < (len + 1))
        return FSYSTEM_STATUS_ERROR;

    memcpy(dataBuffer, configString, len);

    dataBuffer[len] = '\0';   /* sigurnost */

    *fileSize = len;

    return FSYSTEM_STATUS_OK;
}


fsystem_status_t FSYSTEM_WriteToFile(char* path, uint32_t pathSize, char* dataBuffer, uint32_t fileSize)
{

    return FSYSTEM_STATUS_OK;
}

fsystem_status_t FSYSTEM_ReadBDChunk(uint32_t offset, char* dataBuffer, uint32_t dataBufferMaxSize, uint32_t* dataSize, uint32_t timeout)
{
    if((dataBuffer == NULL) || (dataSize == NULL)) return FSYSTEM_STATUS_ERROR;
    if(dataBufferMaxSize > FSYSTEM_BD_CHUNK_SIZE) return FSYSTEM_STATUS_ERROR;

    fsystem_bd_chunk_info_t* chunk = &prvFSYSTEM_LAST_BD_CHUNK;
    chunk->offset = offset;
    chunk->size   = dataBufferMaxSize;

    fsystem_command_request_t req;
    memset(&req, 0, sizeof(req));

    req.type = FSYSTEM_COMMAND_TYPE_REQ;
    req.command = FSYSTEM_COMMAND_READ_BD_CHUNK;
    req.commandId = prvFSYSTEM_GetNextCommandId();
    req.data = chunk;

    if(xQueueSend(prvFSYSTEM_DATA.commandRequestQueue, &req, pdMS_TO_TICKS(timeout)) != pdPASS) return FSYSTEM_STATUS_ERROR;

    fsystem_command_response_t res;
    if(xQueueReceive(prvFSYSTEM_DATA.commandResponseQueue, &res, pdMS_TO_TICKS(timeout)) != pdPASS) return FSYSTEM_STATUS_ERROR;

    if((res.commandId != req.commandId) || (res.status != FSYSTEM_STATUS_OK)) return FSYSTEM_STATUS_ERROR;

    chunk = (fsystem_bd_chunk_info_t*)res.data;
    if(chunk == NULL) return FSYSTEM_STATUS_ERROR;

    memcpy(dataBuffer, chunk->buffer, chunk->size);
    *dataSize = chunk->size;

    return FSYSTEM_STATUS_OK;
}

fsystem_status_t FSYSTEM_WriteBDChunk(uint32_t offset, char* dataBuffer, uint32_t* dataSize, uint32_t timeout)
{
    if((dataBuffer == NULL) || (dataSize == NULL)) return FSYSTEM_STATUS_ERROR;
    if(*dataSize > FSYSTEM_BD_CHUNK_SIZE) return FSYSTEM_STATUS_ERROR;

    uint32_t alignedSize = *dataSize;

    if(FSYSTEM_BD_CHUNK_MIN_SIZE > 0)
    {
        uint32_t remainder = alignedSize % FSYSTEM_BD_CHUNK_MIN_SIZE;
        if(remainder != 0)
        {
            alignedSize += (FSYSTEM_BD_CHUNK_MIN_SIZE - remainder);
        }
    }

    if(alignedSize > FSYSTEM_BD_CHUNK_SIZE) return FSYSTEM_STATUS_ERROR;

    fsystem_bd_chunk_info_t* chunk = &prvFSYSTEM_LAST_BD_CHUNK;
    chunk->offset = offset;
    chunk->size   = alignedSize;

    memset(chunk->buffer, 0x00, alignedSize);
    memcpy(chunk->buffer, dataBuffer, *dataSize);

    fsystem_command_request_t req;
    memset(&req, 0, sizeof(req));

    req.type = FSYSTEM_COMMAND_TYPE_REQ;
    req.command = FSYSTEM_COMMAND_WRITE_BD_CHUNK;
    req.commandId = prvFSYSTEM_GetNextCommandId();
    req.data = chunk;

    if(xQueueSend(prvFSYSTEM_DATA.commandRequestQueue, &req, pdMS_TO_TICKS(timeout)) != pdPASS) return FSYSTEM_STATUS_ERROR;

    fsystem_command_response_t res;
    if(xQueueReceive(prvFSYSTEM_DATA.commandResponseQueue, &res, pdMS_TO_TICKS(timeout)) != pdPASS) return FSYSTEM_STATUS_ERROR;

    if((res.commandId != req.commandId) || (res.status != FSYSTEM_STATUS_OK)) return FSYSTEM_STATUS_ERROR;

    return FSYSTEM_STATUS_OK;
}

fsystem_status_t FSYSTEM_FormatBD(uint32_t timeout)
{
    fsystem_command_request_t req;
    memset(&req, 0, sizeof(req));

    req.type = FSYSTEM_COMMAND_TYPE_REQ;
    req.command = FSYSTEM_COMMAND_FORMAT_BD;
    req.commandId = prvFSYSTEM_GetNextCommandId();
    req.data = NULL;

    if(xQueueSend(prvFSYSTEM_DATA.commandRequestQueue, &req, pdMS_TO_TICKS(timeout)) != pdPASS) return FSYSTEM_STATUS_ERROR;

    fsystem_command_response_t res;
    if(xQueueReceive(prvFSYSTEM_DATA.commandResponseQueue, &res, pdMS_TO_TICKS(timeout)) != pdPASS) return FSYSTEM_STATUS_ERROR;

    if((res.commandId != req.commandId) || (res.status != FSYSTEM_STATUS_OK)) return FSYSTEM_STATUS_ERROR;

    return FSYSTEM_STATUS_OK;
}

static uint8_t fsTestTx[CONF_FSYSTEM_BD_CHUNK_SIZE];
static uint8_t fsTestRx[CONF_FSYSTEM_BD_CHUNK_SIZE];

fsystem_status_t FSYSTEM_TestBD()
{
    uint32_t readSize = 0;
    uint8_t error = 0;

    /* ================= READ (INITIAL) ================= */
    memset(fsTestRx, 0, FSYSTEM_BD_CHUNK_SIZE);

    if(FSYSTEM_ReadBDChunk(0x0000, (char*)fsTestRx, FSYSTEM_BD_CHUNK_SIZE, &readSize, 2000) != FSYSTEM_STATUS_OK)
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Initial Read FAILED\r\n");
    }
    else
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "FS TEST: Initial Read OK\r\n");
    }

    /* ================= GENERATE BASE PATTERN ================= */
    for(uint32_t i = 0; i < FSYSTEM_BD_CHUNK_SIZE; i++)
    {
        fsTestTx[i] = (uint8_t)(i % 256);
    }

    /* ================= FORMAT ================= */
    if(FSYSTEM_FormatBD(20000) != FSYSTEM_STATUS_OK)
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Format FAILED\r\n");
        return;
    }
    else
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "FS TEST: Format OK\r\n");
    }

    /* ================= VERIFY FORMAT (EXPECT 0x00) ================= */
    memset(fsTestRx, 0, FSYSTEM_BD_CHUNK_SIZE);
    readSize = 0;
    error = 0;

    if(FSYSTEM_ReadBDChunk(0x0000, (char*)fsTestRx, FSYSTEM_BD_CHUNK_SIZE, &readSize, 2000) != FSYSTEM_STATUS_OK)
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Read After Format FAILED\r\n");
        return;
    }

    for(uint32_t i = 0; i < FSYSTEM_BD_CHUNK_SIZE; i++)
    {
        if(fsTestRx[i] != 0x00)
        {
            error = 1;
            LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Format verify FAILED at %lu\r\n", i);
            break;
        }
    }

    if(error == 0)
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "FS TEST: Format VERIFY OK\r\n");
    }

    /* ================= WRITE BASE PATTERN ================= */
    uint32_t size = FSYSTEM_BD_CHUNK_SIZE;

    if(FSYSTEM_WriteBDChunk(0x0000, (char*)fsTestTx, &size, 2000) != FSYSTEM_STATUS_OK)
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Write FAILED\r\n");
        return;
    }
    else
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "FS TEST: Write OK\r\n");
    }

    /* ================= VERIFY WRITE ================= */
    memset(fsTestRx, 0, FSYSTEM_BD_CHUNK_SIZE);
    readSize = 0;
    error = 0;

    if(FSYSTEM_ReadBDChunk(0x0000, (char*)fsTestRx, FSYSTEM_BD_CHUNK_SIZE, &readSize, 2000) != FSYSTEM_STATUS_OK)
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Read After Write FAILED\r\n");
        return;
    }

    for(uint32_t i = 0; i < FSYSTEM_BD_CHUNK_SIZE; i++)
    {
        if(fsTestTx[i] != fsTestRx[i])
        {
            error = 1;
            LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Write verify FAILED at %lu\r\n", i);
            break;
        }
    }

    if(error == 0)
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "FS TEST: Write VERIFY OK\r\n");
    }

    /* ================= UPDATE PATTERN (+10 mod 256) ================= */
    for(uint32_t i = 0; i < FSYSTEM_BD_CHUNK_SIZE; i++)
    {
        fsTestTx[i] = (uint8_t)((fsTestTx[i] + 10) % 256);
    }

    /* ================= WRITE UPDATED PATTERN ================= */
    size = FSYSTEM_BD_CHUNK_SIZE;

    if(FSYSTEM_WriteBDChunk(0x0000, (char*)fsTestTx, &size, 2000) != FSYSTEM_STATUS_OK)
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Write Updated FAILED\r\n");
        return;
    }
    else
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "FS TEST: Write Updated OK\r\n");
    }

    /* ================= VERIFY UPDATED ================= */
    memset(fsTestRx, 0, FSYSTEM_BD_CHUNK_SIZE);
    readSize = 0;
    error = 0;

    if(FSYSTEM_ReadBDChunk(0x0000, (char*)fsTestRx, FSYSTEM_BD_CHUNK_SIZE, &readSize, 2000) != FSYSTEM_STATUS_OK)
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Read Updated FAILED\r\n");
        return;
    }

    for(uint32_t i = 0; i < FSYSTEM_BD_CHUNK_SIZE; i++)
    {
        if(fsTestTx[i] != fsTestRx[i])
        {
            error = 1;
            LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Updated verify FAILED at %lu\r\n", i);
            break;
        }
    }

    if(error == 0)
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "FS TEST: Updated VERIFY OK\r\n");
    }

    /* ================= VARIABLE BLOCK WRITE TEST ================= */
    LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "FS TEST: Variable block WRITE START\r\n");

    uint32_t offset = 0;
    uint32_t totalSize = FSYSTEM_BD_CHUNK_SIZE;
    uint32_t chunkSizes[] = {7, 13, 29, 64, 3, 128, 11, 256};
    uint32_t chunkCount = sizeof(chunkSizes) / sizeof(chunkSizes[0]);
    uint32_t idx = 0;

    /* regenerate base pattern */
    for(uint32_t i = 0; i < totalSize; i++)
    {
        fsTestTx[i] = (uint8_t)(i % 256);
    }

    while(offset < totalSize)
    {
    	uint32_t reqSize = chunkSizes[idx % chunkCount];
    	uint32_t writeSize = reqSize;

    	if((offset + reqSize) > totalSize)
    	{
    	    writeSize = totalSize - offset;
    	}

    	uint32_t tmpSize = writeSize;

    	if(FSYSTEM_WriteBDChunk(offset, (char*)&fsTestTx[offset], &tmpSize, 2000) != FSYSTEM_STATUS_OK)
    	{
    	    LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Variable Write FAILED at offset %lu\r\n", offset);
    	    return;
    	}

    	/* offset ide po originalnom request size */
    	offset += writeSize;
    	idx++;
    }

    LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "FS TEST: Variable block WRITE OK\r\n");

    /* ================= VARIABLE BLOCK VERIFY ================= */
    memset(fsTestRx, 0, FSYSTEM_BD_CHUNK_SIZE);
    readSize = 0;
    error = 0;

    if(FSYSTEM_ReadBDChunk(0x0000, (char*)fsTestRx, FSYSTEM_BD_CHUNK_SIZE, &readSize, 2000) != FSYSTEM_STATUS_OK)
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Variable Read FAILED\r\n");
        return;
    }

    for(uint32_t i = 0; i < FSYSTEM_BD_CHUNK_SIZE; i++)
    {
        if(fsTestTx[i] != fsTestRx[i])
        {
            error = 1;
            LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "FS TEST: Variable verify FAILED at %lu (tx=%u rx=%u)\r\n", i, fsTestTx[i], fsTestRx[i]);
            break;
        }
    }

    if(error == 0)
    {
        LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "FS TEST: Variable VERIFY OK\r\n");
    }
}


/**
 * @}
 */
