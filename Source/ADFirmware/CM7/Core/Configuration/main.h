/**
 ******************************************************************************
 * @file    main.h
 *
 * @brief   Main system configuration header providing basic system definitions,
 *          error handling, and essential STM32 HAL integration. This file
 *          serves as the primary hardware configuration interface and
 *          includes core system function prototypes, pin definitions, and
 *          critical error handling mechanisms required by the system.
 *
 * @date    November 2022
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CONFIGURATION System Configuration
 * @{
 */

/**
 * @defgroup MAIN_CONFIG Main System Configuration
 * @{
 */

#include "stm32h7xx_hal.h"


/**
 * @defgroup MAIN_EXPORTED_FUNCTIONS Exported Functions
 * @{
 */

/**
 * @brief   System error handler function
 * @details This function is called when a critical system error occurs.
 *          It handles error conditions by stopping system execution and
 *          entering an error state with visual indication if available.
 */
void Error_Handler(void);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */