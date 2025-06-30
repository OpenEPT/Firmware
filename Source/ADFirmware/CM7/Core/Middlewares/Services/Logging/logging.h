/**
 ******************************************************************************
 * @file    logging.h
 *
 * @brief   Logging service provides centralized logging functionality for system
 *          messages with different severity levels (info, warning, error). It
 *          supports formatted message output with service identification and
 *          timestamp capabilities. The service uses queue-based message handling
 *          for non-blocking operation and multiple output channels.
 *          All logging service interface functions, defines, and types are
 *          declared in this header file.
 *
 * @author  Haris Turkmanovic
 * @email   haris.turkmanovic@gmail.com
 * @date    November 2022
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_LOGGING_LOGGING_H_
#define CORE_MIDDLEWARES_SERVICES_LOGGING_LOGGING_H_

#include "globalConfig.h"
/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup LOGGING_SERVICE Logging Service
 * @{
 */

/**
 * @defgroup LOGGING_PUBLIC_DEFINES Logging service public defines
 * @{
 */
#define LOGGING_INTERNAL_BUFFER_SIZE    300                 /*!< Internal message buffer size in bytes */
#define LOGGING_QUEUE_LENGTH            50                  /*!< Maximum number of messages in queue */
#define LOGGING_MAX_NUMBER_OF_CHANNELS  1                   /*!< Maximum number of output channels */
#define LOGGING_TRANSMIT_TIMEOUT        1000                /*!< Transmission timeout in milliseconds */

#define LOGGING_TASK_NAME               CONFIG_LOGGING_TASK_NAME        /*!< Logging service task name */
#define LOGGING_TASK_PRIO               CONFIG_LOGGING_PRIO             /*!< Logging service task priority */
#define LOGGING_TASK_STACK              CONFIG_LOGGING_STACK_SIZE       /*!< Logging service task stack size */


/**
 * @}
 */

/**
 * @defgroup LOGGING_PUBLIC_TYPES Logging service public data types
 * @{
 */

/**
 * @brief Logging service return status
 */
typedef enum{
    LOGGING_STATUS_OK,              /*!< Logging operation successful */
    LOGGING_STATUS_ERROR            /*!< Logging operation failed */
}logging_status_t;

/**
 * @brief Logging message type/severity level
 */
typedef enum{
    LOGGING_MSG_TYPE_INFO,          /*!< Informational message */
    LOGGING_MSG_TYPE_WARNING,       /*!< Warning message */
    LOGGING_MSG_TYPE_ERROR          /*!< Error message */
}logging_msg_type_t;


/**
 * @brief Logging service state
 */
typedef enum
{
    LOGGING_STATE_INIT,             /*!< Initialization state */
    LOGGING_STATE_SERVICE,          /*!< Logging service active */
    LOGGING_STATE_ERROR             /*!< Logging service in error state */
}logging_state_t;

/**
 * @brief Logging service initialization status
 */
typedef enum
{
    LOGGING_INITIALIZATION_STATUS_NOINIT    =   0,      /*!< Service not initialized */
    LOGGING_INITIALIZATION_STATUS_INIT      =   1       /*!< Service initialized */
}logging_initialization_status_t;

/**
 * @}
 */

/**
 * @defgroup LOGGING_PUBLIC_FUNCTIONS Logging service interface functions
 * @{
 */

/**
 * @brief   Initialize the logging service
 * @param   initTimeout: Timeout to complete initialization
 * @retval  ::logging_status_t
 */
logging_status_t 	LOGGING_Init(uint32_t initTimeout);
/**
 * @brief   Write a formatted log message with specified severity level
 * @param   serviceName: Name of the service/module generating the message
 * @param   msgType: Message type/severity level. See ::logging_msg_type_t
 * @param   message: Printf-style format string for the message
 * @param   ...: Variable arguments for format string (printf-style)
 * @retval  ::logging_status_t
 */
logging_status_t 	LOGGING_Write(char* serviceName, logging_msg_type_t msgType, char* message, ...);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */
#endif /* CORE_MIDDLEWARES_SERVICES_LOGGING_LOGGING_H_ */
