/**
 ********************************************************************************
 * @file	fsystem.h
 *
 * @brief	File system service provides interface for file and block device
 * 			operations.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	April 2026
 ********************************************************************************
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
 * @defgroup FSYSTEM_PUBLIC_DEFINES File system service public defines
 * @{
 */

#define FSYSTEM_TASK_NAME        		CONF_FSYSTEM_TASK_NAME			/*!< File system service task name */
#define FSYSTEM_TASK_PRIO        		CONF_FSYSTEM_PRIO				/*!< File system service task priority */
#define FSYSTEM_TASK_STACK       		CONF_FSYSTEM_STACK_SIZE			/*!< File system service task stack size */
#define FSYSTEM_OFFSET					CONF_FSYSTEM_OFFSET				/*!< File system block device offset */
#define FSYSTEM_BLOCK_SIZE			    CONF_FSYSTEM_BLOCK_SIZE			/*!< File system block size */
#define FSYSTEM_BLOCK_COUNT			    CONF_FSYSTEM_BLOCK_COUNT			/*!< Number of file system blocks */
#define FSYSTEM_BD_CHUNK_SIZE	 		CONF_FSYSTEM_BD_CHUNK_SIZE		/*!< Block device chunk size */
#define FSYSTEM_BD_CHUNK_MIN_SIZE 		CONF_FSYSTEM_BD_CHUNK_MIN_SIZE	/*!< Minimum block device chunk size */
#define FSYSTEM_BD_SIZE					CONF_FSYSTEM_BD_SIZE				/*!< Block device size */

/* Queue configuration. */
#define FSYSTEM_CMD_QUEUE_LENGTH      	8								/*!< Command queue length */
#define FSYSTEM_CMD_RESPONSE_LENGTH   	8								/*!< Command response queue length */
#define FSYSTEM_CMD_TIMEOUT_MS        	1000							/*!< Command timeout in milliseconds */

/**
 * @}
 */

/**
 * @defgroup FSYSTEM_PUBLIC_TYPES File system service public data types
 * @{
 */

/**
 * @brief File system service return status
 */
typedef enum {
    FSYSTEM_STATUS_OK,			/*!< File system operation successful */
    FSYSTEM_STATUS_ERROR		/*!< File system operation failed */
} fsystem_status_t;

/**
 * @brief File system service state
 */
typedef enum {
    FSYSTEM_STATE_INIT,			/*!< File system service initialization state */
    FSYSTEM_STATE_SERVICE,		/*!< File system service active state */
    FSYSTEM_STATE_ERROR			/*!< File system service error state */
} fsystem_state_t;

/**
 * @}
 */

/**
 * @defgroup FSYSTEM_PUBLIC_FUNCTIONS File system service interface functions
 * @{
 */

/**
 * @brief	Initialize the file system service
 * @param	initTimeout: Timeout to complete initialization in milliseconds
 * @retval	::fsystem_status_t
 */
fsystem_status_t FSYSTEM_Init(uint32_t initTimeout);

/**
 * @brief	Read file content from specified path
 * @param	path: Pointer to file path
 * @param	pathSize: File path length
 * @param	dataBuffer: Pointer to destination buffer
 * @param	dataBufferMaxSize: Maximum destination buffer size
 * @param	fileSize: Pointer to variable where file size will be stored
 * @retval	::fsystem_status_t
 */
fsystem_status_t FSYSTEM_GetFileFromPath(char* path, uint32_t pathSize, char* dataBuffer, uint32_t dataBufferMaxSize, uint32_t* fileSize);

/**
 * @brief	Write data to specified file
 * @param	path: Pointer to file path
 * @param	pathSize: File path length
 * @param	dataBuffer: Pointer to data buffer
 * @param	fileSize: Number of bytes to write
 * @retval	::fsystem_status_t
 */
fsystem_status_t FSYSTEM_WriteToFile(char* path, uint32_t pathSize, char* dataBuffer, uint32_t fileSize);

/**
 * @brief	Read data chunk from block device
 * @param	offset: Start offset in block device
 * @param	dataBuffer: Pointer to destination buffer
 * @param	dataBufferMaxSize: Maximum number of bytes to read
 * @param	dataSize: Pointer to variable where read size will be stored
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::fsystem_status_t
 */
fsystem_status_t FSYSTEM_ReadBDChunk(uint32_t offset, char* dataBuffer, uint32_t dataBufferMaxSize, uint32_t* dataSize, uint32_t timeout);

/**
 * @brief	Write data chunk to block device
 * @param	offset: Start offset in block device
 * @param	dataBuffer: Pointer to data buffer
 * @param	dataSize: Pointer to number of bytes to write
 * @param	verify: Enable write verification
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::fsystem_status_t
 */
fsystem_status_t FSYSTEM_WriteBDChunk(uint32_t offset, char* dataBuffer, uint32_t* dataSize, uint8_t verify, uint32_t timeout);

/**
 * @brief	Format the block device
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::fsystem_status_t
 */
fsystem_status_t FSYSTEM_FormatBD(uint32_t timeout);

/**
 * @brief	Test block device read and write operations
 * @retval	::fsystem_status_t
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
