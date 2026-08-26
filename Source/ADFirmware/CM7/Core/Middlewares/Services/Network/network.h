/**
 ******************************************************************************
 * @file    network.h
 *
 * @brief   Network service interface.
 *
 * @details This module provides initialization and runtime management of the
 *          TCP/IP stack (LwIP) and Ethernet interface.
 *
 *          Responsibilities:
 *          - Initialize LwIP stack and network interface (netif)
 *          - Configure MAC and static IPv4 parameters (IP, MASK, Gateway)
 *          - Monitor Ethernet PHY link status (LAN8742)
 *          - Provide runtime API for updating network parameters
 *
 *          Architecture notes:
 *          - The service runs as a FreeRTOS task
 *          - All configuration parameters are obtained from CONFIGURATION service
 *          - Runtime updates are applied inside the network task context
 *          - External modules MUST NOT directly access LwIP or ETH drivers
 *
 *          This module DOES NOT:
 *          - Implement TCP/UDP protocols
 *          - Provide socket-level communication
 *          - Handle application-layer networking
 *
 * @author  Haris Turkmanovic
 * @email   haris.turkmanovic@gmail.com
 * @date    April 2026
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

/**
 * @brief FreeRTOS task configuration
 */
#define NETWORK_TASK_NAME            CONF_NETWORK_TASK_NAME
#define NETWORK_TASK_PRIO            CONF_NETWORK_TASK_PRIO
#define NETWORK_TASK_STACK_SIZE      CONF_NETWORK_TASK_STACK_SIZE

/**
 * @}
 */

/**
 * @defgroup NETWORK_PUBLIC_TYPES Network service public data types
 * @{
 */

/**
 * @brief Network service state machine
 */
typedef enum
{
    NETWORK_STATE_INIT,     /*!< Initialization phase (LwIP + netif setup) */
    NETWORK_STATE_SERVICE,  /*!< Normal operation (link monitoring + updates) */
    NETWORK_STATE_ERROR     /*!< Error state (system-level failure) */
} network_state_t;

/**
 * @brief Network service return status
 */
typedef enum
{
    NETWORK_STATUS_OK,      /*!< Operation successful */
    NETWORK_STATUS_ERROR    /*!< Operation failed */
} network_status_t;

/**
 * @}
 */

/**
 * @defgroup NETWORK_PUBLIC_FUNCTIONS Network service interface functions
 * @{
 */

/**
 * @brief Initialize network service
 *
 * @details Creates and starts the network task, initializes LwIP stack,
 *          configures network interface, and waits for initialization completion.
 *
 * @param timeout Timeout in milliseconds for initialization
 * @return NETWORK_STATUS_OK on success, NETWORK_STATUS_ERROR otherwise
 */
network_status_t NETWORK_Init(uint32_t timeout);

/**
 * @brief Set IP address (runtime update)
 *
 * @details Updates IP address asynchronously inside network task context.
 *          New value is applied using LwIP API and stored in CONFIGURATION.
 *
 * @param ip IPv4 address string (e.g. "192.168.1.100")
 * @return NETWORK_STATUS_OK or NETWORK_STATUS_ERROR
 */
network_status_t NETWORK_SetIPAddress(const char* ip);

/**
 * @brief Set subnet mask (runtime update)
 *
 * @param mask Subnet mask string (e.g. "255.255.255.0")
 * @return NETWORK_STATUS_OK or NETWORK_STATUS_ERROR
 */
network_status_t NETWORK_SetIPMask(const char* mask);

/**
 * @brief Set gateway address (runtime update)
 *
 * @param gw Gateway IPv4 address string
 * @return NETWORK_STATUS_OK or NETWORK_STATUS_ERROR
 */
network_status_t NETWORK_SetGateway(const char* gw);

/**
 * @brief Set complete IP configuration atomically
 *
 * @details Updates IP address, subnet mask, and gateway in a single operation.
 *          This prevents inconsistent intermediate states.
 *
 * @param ip IPv4 address
 * @param mask Subnet mask
 * @param gw Gateway address
 * @return NETWORK_STATUS_OK or NETWORK_STATUS_ERROR
 */
network_status_t NETWORK_SetIPAll(const char* ip, const char* mask, const char* gw);

/**
 * @brief Get MAC address
 *
 * @param mac Pointer to buffer (must be 6 bytes)
 * @return NETWORK_STATUS_OK or NETWORK_STATUS_ERROR
 */
network_status_t NETWORK_GetMACAddr(uint8_t* mac);

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
