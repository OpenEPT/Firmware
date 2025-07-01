/**
 ******************************************************************************
 * @file    energy_debugger.h
 *
 * @brief   Energy Debugger service provides interface for energy consumption
 *          monitoring and debugging capabilities. It enables TCP communication
 *          with remote servers for data collection and analysis of energy usage
 *          patterns in embedded systems.
 *          All energy debugger service interface functions, defines, and types
 *          are declared in this header file.
 *
 * @author  Pavle Lakic & Dimitrije Lilic
 * @date    June 2024
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_ED_ENERGY_DEBUGGER_H_
#define CORE_MIDDLEWARES_SERVICES_ED_ENERGY_DEBUGGER_H_

#include <stdint.h>

#include "globalConfig.h"
/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup ENERGY_DEBUGGER_SERVICE Energy Debugger Service
 * @{
 */

/**
 * @defgroup ENERGY_DEBUGGER_PUBLIC_DEFINES Energy Debugger service public defines
 * @{
 */
#define ENERGY_DEBUGGER_TASK_NAME               CONF_ENERGY_DEBUGGER_TASK_NAME         /*!< Energy debugger service task name */
#define ENERGY_DEBUGGER_TASK_PRIO               CONF_ENERGY_DEBUGGER_TASK_PRIO         /*!< Energy debugger service task priority */
#define ENERGY_DEBUGGER_STACK_SIZE              CONF_ENERGY_DEBUGGER_STACK_SIZE        /*!< Energy debugger service task stack size */
#define ENERGY_DEBUGGER_BUTTON_PORT             CONF_ENERGY_DEBUGGER_BUTTON_PORT       /*!< GPIO port for energy debugger button */
#define ENERGY_DEBUGGER_BUTTON_PIN              CONF_ENERGY_DEBUGGER_BUTTON_PIN        /*!< GPIO pin for energy debugger button */
#define ENERGY_DEBUGGER_BUTTON_ISR_PRIO         CONF_ENERGY_DEBUGGER_BUTTON_ISR_PRIO   /*!< Button interrupt service routine priority */
#define ENERGY_DEBUGGER_ID_QUEUE_LENGTH         CONF_ENERGY_DEBUGGER_ID_QUEUE_LENGTH   /*!< ID queue length */
#define ENERGY_DEBUGGER_EBP_NAMES_QUEUE_LENGTH  CONF_ENERGY_DEBUGGER_ID_QUEUE_LENGTH   /*!< EBP names queue length */
#define ENERGY_DEBUGGER_EBP_QUEUE_LENGTH        CONF_ENERGY_DEBUGGER_EBP_QUEUE_LENGTH  /*!< EBP queue length */
#define ENERGY_DEBUGGER_MESSAGE_BUFFER_LENGTH   CONF_ENERGY_DEBUGGER_MESSAGE_BUFFER_LENGTH  /*!< Message buffer length */
#define ENERGY_DEBUGGER_TCP_MESSAGE_BUFFER_LENGTH CONF_ENERGY_DEBUGGER_TCP_MESSAGE_BUFFER_LENGTH  /*!< TCP message buffer length */

#define ENERGY_DEBUGGER_MAX_CONNECTIONS         CONF_ENERGY_DEBUGGER_MAX_CONNECTIONS   /*!< Maximum number of concurrent connections */
/**
 * @}
 */

/**
 * @defgroup ENERGY_DEBUGGER_PUBLIC_TYPES Energy Debugger service public data types
 * @{
 */

/**
 * @brief Energy debugger service state
 */
typedef enum
{
    ENERGY_DEBUGGER_STATE_INIT = 0,     /*!< Initialization state */
    ENERGY_DEBUGGER_STATE_SERVICE,      /*!< Energy debugger service active */
    ENERGY_DEBUGGER_STATE_ERROR         /*!< Energy debugger service in error state */
} energy_debugger_state_t;


/**
 * @brief Energy debugger service return status
 */
typedef enum
{
    ENERGY_DEBUGGER_STATUS_OK,          /*!< Energy debugger operation successful */
    ENERGY_DEBUGGER_STATUS_ERROR        /*!< Energy debugger operation failed */
} energy_debugger_status_t;

/**
 * @brief Energy debugger connection information structure
 */
typedef struct
{
    uint8_t     serverIp[4];            /*!< Server IP address (4 bytes for IPv4) */
    uint16_t    serverport;             /*!< Server port number */
    uint32_t    id;                     /*!< Connection identifier */
} energy_debugger_connection_info;

/**
 * @}
 */

/**
 * @defgroup ENERGY_DEBUGGER_PUBLIC_FUNCTIONS Energy Debugger service interface functions
 * @{
 */

/**
 * @brief   Initialize the energy debugger service
 * @param   timeout: Timeout to complete initialization
 * @retval  ::energy_debugger_status_t
 */
energy_debugger_status_t ENERGY_DEBUGGER_Init(uint32_t timeout);
/**
 * @brief   Create a TCP link connection to remote server
 * @param   serverInfo: Pointer to connection information structure. See ::energy_debugger_connection_info
 * @param   timeout: Timeout for connection establishment
 * @retval  ::energy_debugger_status_t
 */
energy_debugger_status_t ENERGY_DEBUGGER_CreateLink(energy_debugger_connection_info* serverInfo, uint32_t timeout);
/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */
#endif /* CORE_MIDDLEWARES_SERVICES_ED_ENERGY_DEBUGGER_H_ */
