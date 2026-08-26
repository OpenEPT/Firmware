/**
 ******************************************************************************
 * @file    bringup.h
 *
 * @brief   Hardware bring-up service interface.
 *
 *          Bring-up service is responsible for initial hardware
 *          validation and peripheral communication checks during
 *          early system startup.
 *
 *          Service is conditionally enabled using:
 *
 *          @code
 *          CONF_BRINGUP_ENABLE
 *          @endcode
 *
 *          configuration macro.
 *
 * @author  Haris Turkmanovic
 * @email   haris.turkmanovic@gmail.com
 * @date    May 2026
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_SYSTEM_BRINGUP_BRINGUP_H_
#define CORE_MIDDLEWARES_SERVICES_SYSTEM_BRINGUP_BRINGUP_H_

#include "globalConfig.h"

#if(CONF_BRINGUP_ENABLE == 1)

#include <stdint.h>

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup BRINGUP_SERVICE Bring-up service
 *
 * @brief Hardware bring-up and validation service
 *
 * Bring-up service is responsible for:
 *  - Initial hardware initialization
 *  - Peripheral communication validation
 *  - Hardware self-test execution
 *  - Startup diagnostics
 *  - Basic board validation
 *
 * Service executes only once during startup.
 *
 * @{
 */

/**
 * @defgroup BRINGUP_PUBLIC_DEFINES Bring-up service public defines
 * @{
 */

#define BRINGUP_TASK_NAME           CONF_BRINGUP_TASK_NAME         /*!< Bring-up task name */
#define BRINGUP_TASK_PRIO           CONF_BRINGUP_TASK_PRIO         /*!< Bring-up task priority */
#define BRINGUP_TASK_STACK_SIZE     CONF_BRINGUP_TASK_STACK_SIZE   /*!< Bring-up task stack size */

/**
 * @}
 */

/**
 * @defgroup BRINGUP_PUBLIC_TYPES Bring-up service public data types
 * @{
 */

/**
 * @brief Bring-up service return status
 */
typedef enum
{
    BRINGUP_STATUS_OK,         /*!< Bring-up operation successful */
    BRINGUP_STATUS_ERROR       /*!< Bring-up operation failed */

}bringup_status_t;

/**
 * @}
 */

/**
 * @defgroup BRINGUP_PUBLIC_FUNCTIONS Bring-up service interface functions
 * @{
 */

/**
 * @brief Initialize bring-up service
 *
 * This function creates bring-up task responsible
 * for hardware validation during startup.
 *
 * @retval BRINGUP_STATUS_OK     Bring-up service successfully initialized
 * @retval BRINGUP_STATUS_ERROR  Bring-up service initialization failed
 */
bringup_status_t BRINGUP_Init(void);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CONF_BRINGUP_ENABLE */

#endif /* CORE_MIDDLEWARES_SERVICES_SYSTEM_BRINGUP_BRINGUP_H_ */
