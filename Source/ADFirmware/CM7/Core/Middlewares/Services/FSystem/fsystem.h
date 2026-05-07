/**
 ******************************************************************************
 * @file    fsystem.h
 *
 * @brief   File system service interface.
 *          Provides basic infrastructure for file system handling.
 *          Core logic is implemented as a FreeRTOS task.
 *
 * @author  Haris Turkmanovic
 * @date    April 2026
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_FSYSTEM_FSYSTEM_H_
#define CORE_MIDDLEWARES_SERVICES_FSYSTEM_FSYSTEM_H_

#include "globalConfig.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup FSYSTEM_SERVICE File System Service
 * @{
 */

/**
 * @defgroup FSYSTEM_PUBLIC_DEFINES FSYSTEM public defines
 * @{
 */
#define FSYSTEM_TASK_NAME        		CONF_FSYSTEM_TASK_NAME
#define FSYSTEM_TASK_PRIO        		CONF_FSYSTEM_PRIO
#define FSYSTEM_TASK_STACK       		CONF_FSYSTEM_STACK_SIZE
#define FSYSTEM_OFFSET					CONF_FSYSTEM_OFFSET
#define FSYSTEM_BLOCK_SIZE			    CONF_FSYSTEM_BLOCK_SIZE
#define FSYSTEM_BLOCK_COUNT			    CONF_FSYSTEM_BLOCK_COUNT
#define FSYSTEM_BD_CHUNK_SIZE	 		CONF_FSYSTEM_BD_CHUNK_SIZE
#define FSYSTEM_BD_CHUNK_MIN_SIZE 		CONF_FSYSTEM_BD_CHUNK_MIN_SIZE
#define FSYSTEM_BD_SIZE					CONF_FSYSTEM_BD_SIZE
/* Queue config */
#define FSYSTEM_CMD_QUEUE_LENGTH      	8
#define FSYSTEM_CMD_RESPONSE_LENGTH   	8
#define FSYSTEM_CMD_TIMEOUT_MS        	1000
/**
 * @}
 */

/**
 * @defgroup FSYSTEM_PUBLIC_TYPES FSYSTEM public types
 * @{
 */

/**
 * @brief FSYSTEM status
 */
typedef enum {
    FSYSTEM_STATUS_OK,
    FSYSTEM_STATUS_ERROR
} fsystem_status_t;

/**
 * @brief FSYSTEM state machine
 */
typedef enum {
    FSYSTEM_STATE_INIT,
    FSYSTEM_STATE_SERVICE,
    FSYSTEM_STATE_ERROR
} fsystem_state_t;

/**
 * @}
 */

/**
 * @defgroup FSYSTEM_PUBLIC_FUNCTIONS FSYSTEM public functions
 * @{
 */

/**
 * @brief Initialize file system service
 *
 * Initializes internal FSYSTEM structures, queues, and starts the
 * underlying FreeRTOS task responsible for handling file system operations.
 *
 * @param initTimeout Timeout for initialization (in milliseconds)
 * @retval FSYSTEM_STATUS_OK     Initialization successful
 * @retval FSYSTEM_STATUS_ERROR  Initialization failed
 */
fsystem_status_t FSYSTEM_Init(uint32_t initTimeout);

/**
 * @brief Retrieve file content from given path
 *
 * Reads a file identified by its path from the underlying storage and
 * copies its content into the provided buffer.
 *
 * @param path Pointer to file path string
 * @param pathSize Length of the path string
 * @param dataBuffer Output buffer for file content
 * @param dataBufferMaxSize Maximum size of the output buffer
 * @param fileSize Pointer to variable where file size will be stored
 *
 * @retval FSYSTEM_STATUS_OK     File successfully read
 * @retval FSYSTEM_STATUS_ERROR  File not found or read failed
 */
fsystem_status_t FSYSTEM_GetFileFromPath(char* path, uint32_t pathSize, char* dataBuffer, uint32_t dataBufferMaxSize, uint32_t* fileSize);
fsystem_status_t FSYSTEM_WriteToFile(char* path, uint32_t pathSize, char* dataBuffer, uint32_t fileSize);

/**
 * @brief Read block device (BD) chunk
 *
 * Reads a chunk of data from the block device starting at the specified
 * offset and copies it into the provided buffer.
 *
 * @param offset Start address in block device
 * @param dataBuffer Output buffer
 * @param dataBufferMaxSize Maximum number of bytes to read
 * @param dataSize Pointer to variable where actual read size will be stored
 * @param timeout Timeout for operation (in milliseconds)
 *
 * @retval FSYSTEM_STATUS_OK     Read successful
 * @retval FSYSTEM_STATUS_ERROR  Read failed or invalid parameters
 */
fsystem_status_t FSYSTEM_ReadBDChunk(uint32_t offset, char* dataBuffer, uint32_t dataBufferMaxSize, uint32_t* dataSize, uint32_t timeout);

/**
 * @brief Write block device (BD) chunk
 *
 * Writes a chunk of data to the block device at the specified offset.
 * If the provided data size is not aligned to the minimal programmable
 * size (FSYSTEM_BD_CHUNK_MIN_SIZE), the remaining bytes are automatically
 * padded with 0x00.
 *
 * @param offset Start address in block device
 * @param dataBuffer Input data buffer
 * @param dataSize Pointer to size of data to write (may be internally aligned)
 * @param timeout Timeout for operation (in milliseconds)
 *
 * @note
 * The actual number of written bytes may be greater than the requested
 * size due to alignment requirements.
 *
 * @retval FSYSTEM_STATUS_OK     Write successful
 * @retval FSYSTEM_STATUS_ERROR  Write failed or invalid parameters
 */
fsystem_status_t FSYSTEM_WriteBDChunk(uint32_t offset, char* dataBuffer, uint32_t* dataSize, uint8_t verify, uint32_t timeout);

/**
 * @brief Format block device (BD)
 *
 * Erases the entire block device by writing 0x00 to all locations.
 * The operation is executed through the FSYSTEM service task.
 *
 * @param timeout Timeout for operation (in milliseconds)
 *
 * @note
 * This is a blocking operation and may take significant time depending
 * on the size and performance of the underlying storage.
 *
 * @warning
 * This function permanently erases all data on the block device.
 *
 * @retval FSYSTEM_STATUS_OK     Format successful
 * @retval FSYSTEM_STATUS_ERROR  Format failed
 */
fsystem_status_t FSYSTEM_FormatBD(uint32_t timeout);

/**
 * @brief Perform comprehensive block device (BD) test sequence
 *
 * This function executes a full validation procedure over the underlying
 * block device (EEPROM). The test includes multiple stages:
 *
 *  - Initial read check
 *  - Full device format (erase to 0x00)
 *  - Format verification
 *  - Sequential write of deterministic pattern
 *  - Read-back verification
 *  - Pattern update and overwrite test
 *  - Variable-size block write test
 *  - Final full memory verification
 *
 * The function is intended for debugging, validation, and bring-up of the
 * FSYSTEM service and underlying storage driver.
 *
 * @note
 * This function is blocking and may take a significant amount of time
 * depending on the size and characteristics of the underlying memory
 * device (e.g., EEPROM write cycle time).
 *
 * @warning
 * This function will overwrite the entire block device content.
 * It must not be used in production without proper safeguards.
 *
 * @retval FSYSTEM_STATUS_OK     Test completed successfully
 * @retval FSYSTEM_STATUS_ERROR  One or more test stages failed
 */
fsystem_status_t FSYSTEM_TestBD(void);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_MIDDLEWARES_SERVICES_FSYSTEM_FSYSTEM_H_ */
