/**
 ******************************************************************************
 * @file    network.h
 *
 * @brief   Network service provides TCP/IP communication capabilities for the
 *          system. It manages network interfaces, connection establishment,
 *          data transmission, and network status monitoring. The service
 *          supports both client and server modes with configurable connection
 *          parameters and timeout handling.
 *          All network service interface functions, defines, and types are
 *          declared in this header file.
 *
 * @author  Haris Turkmanovic
 * @email   haris.turkmanovic@gmail.com
 * @date    November 2022
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_NETWORK_NETWORK_H_
#define CORE_MIDDLEWARES_SERVICES_NETWORK_NETWORK_H_

#include <stdint.h>

#include "globalConfig.h"
/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup NETWORK_SERVICE Network Service
 * @{
 */

/**
 * @defgroup NETWORK_PUBLIC_DEFINES Network service public defines
 * @{
 */
#define NETWORK_TASK_NAME					CONF_NETWORK_TASK_NAME
#define NETWORK_TASK_PRIO					CONF_NETWORK_TASK_PRIO
#define NETWORK_TASK_STACK_SIZE				CONF_NETWORK_TASK_STACK_SIZE

#define NETWORK_DEVICE_IP_ADDRESS			CONF_NETWORK_DEVICE_IP_ADDRESS
#define NETWORK_DEVICE_IP_MASK				CONF_NETWORK_DEVICE_IP_MASK
#define NETWORK_DEVICE_IP_GW				CONF_NETWORK_DEVICE_IP_GW
/**
 * @}
 */

/**
 * @defgroup NETWORK_PUBLIC_TYPES Network service public data types
 * @{
 */

/**
 * @brief Network service state
 */
typedef enum {
    NETWORK_STATE_INIT,             /*!< Initialization state */
    NETWORK_STATE_SERVICE,          /*!< Network service active */
    NETWORK_STATE_ERROR             /*!< Network service in error state */
} network_state_t;

/**
 * @brief Network service return status
 */
typedef enum
{
    NETWORK_STATUS_OK,              /*!< Network operation successful */
    NETWORK_STATUS_ERROR           /*!< Network operation failed */
}network_status_t;
/**
 * @}
 */

/**
 * @defgroup NETWORK_PUBLIC_FUNCTIONS Network service interface functions
 * @{
 */
/**
 * @brief   Initialize the network service
 * @param   timeout: Timeout to complete initialization
 * @retval  ::network_status_t
 */
network_status_t NETWORK_Init(uint32_t timeout);
/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_MIDDLEWARES_SERVICES_NETWORK_NETWORK_H_ */
