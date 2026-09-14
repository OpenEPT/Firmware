/**
 ******************************************************************************
 * @file    load.h
 *
 * @brief   Load service provides interface for controlling the programmable
 *          electronic load and generating load current waveforms.
 *
 * @author  Haris Turkmanovic
 * @date    September 2026
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_LOAD_LOAD_H_
#define CORE_MIDDLEWARES_SERVICES_LOAD_LOAD_H_

#include "globalConfig.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup LOAD_SERVICE Load Service
 * @{
 */

/**
 * @defgroup LOAD_PUBLIC_DEFINES Load public defines
 * @{
 */

#define LOAD_TASK_NAME                         CONF_LOAD_TASK_NAME
#define LOAD_TASK_PRIO                         CONF_LOAD_TASK_PRIO
#define LOAD_TASK_STACK                        CONF_LOAD_TASK_STACK_SIZE

#define LOAD_DISABLE_PORT                      CONF_LOAD_DISABLE_PORT
#define LOAD_DISABLE_PIN                       CONF_LOAD_DISABLE_PIN

#define LOAD_WAVE_CHUNK_MSG_SIZE               50
#define LOAD_WAVE_CHUNK_MSG_FIELDS             6
#define LOAD_WAVE_CHUNK_MSG_QUEUE_LENGTH       10
#define LOAD_WAVE_CHUNK_MAX_NO                 200
#define LOAD_WAVE_CHUNK_PBS                    200

/**
 * @}
 */

/**
 * @defgroup LOAD_PUBLIC_TYPES Load public types
 * @{
 */

/**
 * @brief Return status of Load API functions.
 */
typedef enum
{
    LOAD_STATUS_OK = 0,
    LOAD_STATUS_ERROR
} load_status_t;

/**
 * @brief Load enable/disable state.
 */
typedef enum
{
    LOAD_STATE_DISABLE = 0,
    LOAD_STATE_ENABLE
} load_state_t;

/**
 * @brief Wave generator state.
 */
typedef enum
{
    LOAD_WAVE_STATE_UNDEF = 0,
    LOAD_WAVE_STATE_ACTIVE,
    LOAD_WAVE_STATE_INACTIVE
} load_wave_state_t;

/**
 * @brief DAC active status
 */
/**
 * @brief Wave complete callback type.
 * @note  Called from ISR context when wave finishes all repetitions.
 */
typedef void (*load_wave_complete_callback_t)(void);

typedef enum {
    LOAD_DAC_STATUS_INACTIVE = 0, /*!< DAC inactive */
    LOAD_DAC_STATUS_ACTIVE        /*!< DAC active */
} load_dac_status_t;

/**
 * @}
 */

/**
 * @defgroup LOAD_PUBLIC_FUNCTIONS Load public functions
 * @{
 */

/**
 * @brief Initialize Load service.
 * @param initTimeout Initialization timeout in milliseconds.
 * @retval ::LOAD_STATUS_OK or ::LOAD_STATUS_ERROR
 */
load_status_t LOAD_Init(uint32_t initTimeout);

/**
 * @brief Set load current.
 * @param current Load current in mA.
 * @param timeout Timeout in milliseconds.
 * @retval ::LOAD_STATUS_OK or ::LOAD_STATUS_ERROR
 */
load_status_t LOAD_SetCurrent(uint32_t current, uint32_t timeout);

/**
 * @brief Get configured load current.
 * @param current Pointer where load current in mA will be stored.
 * @param timeout Timeout in milliseconds.
 * @retval ::LOAD_STATUS_OK or ::LOAD_STATUS_ERROR
 */
load_status_t LOAD_GetCurrent(uint32_t* current, uint32_t timeout);

/**
 * @brief Enable or disable programmable load.
 * @param state Desired load state.
 * @param timeout Timeout in milliseconds.
 * @retval ::LOAD_STATUS_OK or ::LOAD_STATUS_ERROR
 */
load_status_t LOAD_SetState(load_state_t state, uint32_t timeout);

/**
 * @brief Get programmable load state.
 * @param state Pointer where current load state will be stored.
 * @param timeout Timeout in milliseconds.
 * @retval ::LOAD_STATUS_OK or ::LOAD_STATUS_ERROR
 */
load_status_t LOAD_GetState(load_state_t* state, uint32_t timeout);

/**
 * @brief Add waveform chunk.
 * @param waveDesc Wave chunk textual description.
 * @param waveDescSize Wave chunk description size.
 * @param timeout Timeout in milliseconds.
 * @retval ::LOAD_STATUS_OK or ::LOAD_STATUS_ERROR
 */
load_status_t LOAD_AddWaveChunk(char* waveDesc, uint16_t waveDescSize, uint32_t timeout);

/**
 * @brief Start or stop waveform generation.
 * @param state Desired waveform state.
 * @param timeout Timeout in milliseconds.
 * @retval ::LOAD_STATUS_OK or ::LOAD_STATUS_ERROR
 */
load_status_t LOAD_SetWaveState(load_wave_state_t state, uint32_t timeout);

/**
 * @brief Set waveform repetition counter.
 * @param counter Number of waveform repetitions. -1 represents infinite repetition.
 * @param timeout Timeout in milliseconds.
 * @retval ::LOAD_STATUS_OK or ::LOAD_STATUS_ERROR
 */
load_status_t LOAD_SetWaveCounter(int counter, uint32_t timeout);

/**
 * @brief Clear currently configured waveform.
 * @param timeout Timeout in milliseconds.
 * @retval ::LOAD_STATUS_OK or ::LOAD_STATUS_ERROR
 */
load_status_t LOAD_ClearWave(uint32_t timeout);

/**
 * @brief Register callback invoked when wave completes (ISR context).
 * @param callback Function to call, NULL to unregister.
 * @retval ::LOAD_STATUS_OK or ::LOAD_STATUS_ERROR
 */
load_status_t LOAD_RegisterWaveCompleteCallback(load_wave_complete_callback_t callback);

/**
 * @brief	Set the status of the DAC (Digital-to-Analog Converter).
 *
 * This function enables or disables the DAC output used by the programmable load.
 *
 * @param	activeStatus: Desired DAC status. See ::load_dac_status_t
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::load_status_t
 */
load_status_t LOAD_SetDACStatus(load_dac_status_t activeStatus, uint32_t timeout);

/**
 * @brief	Get the current status of the DAC.
 *
 * This function retrieves whether the DAC output used by the programmable load
 * is currently enabled or disabled.
 *
 * @param	activeStatus: Pointer to variable to store the current DAC status
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::load_status_t
 */
load_status_t LOAD_GetDACStatus(load_dac_status_t* activeStatus, uint32_t timeout);

/**
 * @brief	Set the raw DAC value used by the programmable load.
 *
 * This function sets the raw value of the DAC channel used to control
 * the programmable electronic load.
 *
 * @param	value: Raw DAC value to be set
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::load_status_t
 */
load_status_t LOAD_SetDACValue(uint32_t value, uint32_t timeout);

/**
 * @brief	Get the raw DAC value used by the programmable load.
 *
 * This function retrieves the currently configured raw value of the DAC
 * channel used to control the programmable electronic load.
 *
 * @param	value: Pointer to variable where the raw DAC value will be stored
 * @param	timeout: Timeout for operation in milliseconds
 * @retval	::load_status_t
 */
load_status_t LOAD_GetDACValue(uint32_t* value, uint32_t timeout);


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
