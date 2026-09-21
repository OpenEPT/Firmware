/**
 ******************************************************************************
 * @file    dpcontrol.h
 *
 * @brief   DPControl service provides interface for controlling power path
 *          components such as battery switch and protection latch.
 *          It also provides configuration and monitoring of under-voltage,
 *          over-voltage, and over-current protection circuitry.
 *
 * @author  Haris Turkmanovic
 * @date    November 2023
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_DPCONTROL_DPCONTROL_H_
#define CORE_MIDDLEWARES_SERVICES_DPCONTROL_DPCONTROL_H_

#include "globalConfig.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup DPCONTROL_SERVICE DPControl Service
 * @{
 */

/**
 * @defgroup DPCONTROL_PUBLIC_DEFINES DPControl public defines
 * @{
 */
#define DPCONTROL_TASK_NAME             CONF_DPCONTROL_TASK_NAME         /*!< Task name */
#define DPCONTROL_TASK_PRIO             CONF_DPCONTROL_TASK_PRIO         /*!< Task priority */
#define DPCONTROL_TASK_STACK            CONF_DPCONTROL_TASK_STACK_SIZE   /*!< Task stack size */

#define DPCONTROL_BAT_DISABLE_PORT      CONF_DPCONTROL_BAT_DISABLE_PORT  /*!< Battery disable GPIO port */
#define DPCONTROL_BAT_DISABLE_PIN       CONF_DPCONTROL_BAT_DISABLE_PIN   /*!< Battery disable GPIO pin */

#define DPCONTROL_GPIO_DISABLE_PORT     CONF_DPCONTROL_GPIO_DISABLE_PORT /*!< GPIO disable port */
#define DPCONTROL_GPIO_DISABLE_PIN      CONF_DPCONTROL_GPIO_DISABLE_PIN  /*!< GPIO disable pin */

#define DPCONTROL_LATCH_PORT            CONF_DPCONTROL_LATCH_PORT        /*!< Latch trigger port */
#define DPCONTROL_LATCH_PIN             CONF_DPCONTROL_LATCH_PIN         /*!< Latch trigger pin */

#define DPCONTROL_SHUNT_VALUE           CONF_DPCONTROL_SHUNT_VALUE
#define DPCONTROL_INA_GAIN              CONF_DPCONTROL_INA_GAIN
/**
 * @}
 */

/**
 * @defgroup DPCONTROL_PUBLIC_TYPES DPControl public types
 * @{
 */

/**
 * @brief Return status of DPControl API functions
 */
typedef enum {
    DPCONTROL_STATUS_OK = 0,   /*!< Operation successful */
    DPCONTROL_STATUS_ERROR     /*!< Operation failed */
} dpcontrol_status_t;

/**
 * @brief Power path state enable/disable
 */
typedef enum {
    DPCONTROL_PPATH_STATE_DISABLE = 0, /*!< Power path disabled */
    DPCONTROL_PPATH_STATE_ENABLE       /*!< Power path enabled */
} dpcontrol_ppath_state_t;

/**
 * @brief Battery state enable/disable
 */
typedef enum {
    DPCONTROL_BAT_STATE_DISABLE = 0,   /*!< Battery disabled */
    DPCONTROL_BAT_STATE_ENABLE         /*!< Battery enabled */
} dpcontrol_bat_state_t;

/**
 * @brief Protection signal state
 */
typedef enum {
    DPCONTROL_PROTECTION_STATE_DISABLE = 0, /*!< Protection inactive */
    DPCONTROL_PROTECTION_STATE_ENABLE       /*!< Protection active */
} dpcontrol_protection_state_t;

/**
 * @brief Internal task states
 */
typedef enum {
    DPCONTROL_STATE_INIT,     /*!< Initialization state */
    DPCONTROL_STATE_SERVICE,  /*!< Service running */
    DPCONTROL_STATE_ERROR     /*!< Error state */
} dpcontrol_state_t;
/**
 * @}
 */

/**
 * @defgroup DPCONTROL_PUBLIC_FUNCTIONS DPControl public functions
 * @{
 */

/**
 * @brief Initialize DPControl service.
 *
 * This function creates all required synchronization objects and starts
 * the DPControl service task.
 *
 * @param initTimeout: Initialization timeout in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_Init(uint32_t initTimeout);

/**
 * @brief	Set the battery path state (enable or disable connection to battery).
 *
 * Used to control whether the battery path is active or isolated.
 *
 * @param	state: Desired battery state. See ::dpcontrol_bat_state_t
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::dpcontrol_status_t
 */
dpcontrol_status_t DPCONTROL_SetBatState(dpcontrol_bat_state_t state, uint32_t timeout);

/**
 * @brief	Get the current battery connection state.
 *
 * This function checks if the battery path is currently enabled or disabled.
 *
 * @param	state: Pointer to variable to store battery state
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::dpcontrol_status_t
 */
dpcontrol_status_t DPCONTROL_GetBatState(dpcontrol_bat_state_t* state, uint32_t timeout);

/**
 * @brief	Set the power path state (enable or disable path from primary power source).
 *
 * This function is used to enable or disable the power path from the primary source (e.g., USB or adapter).
 *
 * @param	state: Desired power path state. See ::dpcontrol_ppath_state_t
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::dpcontrol_status_t
 */
dpcontrol_status_t DPCONTROL_SetPPathState(dpcontrol_ppath_state_t state, uint32_t timeout);

/**
 * @brief	Get the current state of the primary power path.
 *
 * @param	state: Pointer to variable to store power path state
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::dpcontrol_status_t
 */
dpcontrol_status_t DPCONTROL_GetPPathState(dpcontrol_ppath_state_t* state, uint32_t timeout);

/**
 * @brief	Get the under-voltage protection state.
 *
 * Indicates whether the system is currently experiencing an under-voltage fault.
 *
 * @param	state: Pointer to store protection state
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::dpcontrol_status_t
 */
dpcontrol_status_t DPCONTROL_GetUVoltageState(dpcontrol_protection_state_t* state, uint32_t timeout);

/**
 * @brief	Get the over-voltage protection state.
 *
 * Indicates whether the system is currently experiencing an over-voltage fault.
 *
 * @param	state: Pointer to store protection state
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::dpcontrol_status_t
 */
dpcontrol_status_t DPCONTROL_GetOVoltageState(dpcontrol_protection_state_t* state, uint32_t timeout);

/**
 * @brief	Get the over-current protection state.
 *
 * Indicates whether the system is currently experiencing an over-current fault.
 *
 * @param	state: Pointer to store protection state
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::dpcontrol_status_t
 */
dpcontrol_status_t DPCONTROL_GetOCurrentState(dpcontrol_protection_state_t* state, uint32_t timeout);

/**
 * @brief	Trigger the latch signal for external control or protection.
 *
 * This function activates a latch mechanism, typically used to initiate or reset external
 * circuitry (e.g., power latching, safety resets).
 *
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::dpcontrol_status_t
 */
dpcontrol_status_t DPCONTROL_LatchTriger(uint32_t timeout);

/**
 * @brief Set over-voltage protection threshold.
 *
 * @param value: Over-voltage threshold in volts
 * @param timeout: Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_SetOVValue(float value, uint32_t timeout);

/**
 * @brief Set under-voltage protection threshold.
 *
 * @param value: Under-voltage threshold in volts
 * @param timeout: Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_SetUVValue(float value, uint32_t timeout);

/**
 * @brief Set over-current protection threshold.
 *
 * @param value: Over-current threshold in milliamperes
 * @param timeout: Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_SetOCValue(int32_t value, uint32_t timeout);

/**
 * @brief Get over-voltage protection threshold.
 *
 * @param value: Pointer to store over-voltage threshold
 * @param timeout: Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_GetOVValue(float* value, uint32_t timeout);

/**
 * @brief Get under-voltage protection threshold.
 *
 * @param value: Pointer to store under-voltage threshold
 * @param timeout: Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_GetUVValue(float* value, uint32_t timeout);

/**
 * @brief Get over-current protection threshold.
 *
 * @param value: Pointer to store over-current threshold
 * @param timeout: Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_GetOCValue(int32_t* value, uint32_t timeout);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif
