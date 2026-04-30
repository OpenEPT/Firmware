/**
 ******************************************************************************
 * @file    dac6578.h
 *
 * @brief   DAC6578 DAC driver - Hardware Abstraction Layer (HAL) interface.
 *
 *          This driver provides a high-level API for controlling the
 *          Texas Instruments DAC6578 device:
 *          - 10-bit resolution
 *          - 8 independent output channels
 *          - I2C communication interface
 *
 *          The driver allows:
 *          - Initialization of the DAC device and underlying I2C peripheral
 *          - Setting output voltage via digital code (0–1023)
 *          - Conversion from physical voltage (float) to DAC digital value
 *
 *          The module is designed to be lightweight and deterministic,
 *          suitable for real-time embedded applications.
 *
 * @author  Haris Turkmanovic
 * @date    April 2026
 ******************************************************************************
 */

#ifndef CORE_HAL_DAC6578_DAC6578_H_
#define CORE_HAL_DAC6578_DAC6578_H_

#include <stdint.h>

/**
 * @defgroup HAL Hardware Abstraction Layer
 * @{
 */

/**
 * @defgroup DAC6578_DRIVER DAC6578 DAC Driver
 * @{
 */

/**
 * @defgroup DAC6578_PUBLIC_DEFINES DAC6578 driver public defines
 * @{
 */

/**
 * @brief DAC resolution in bits
 *
 * DAC6578 uses 10-bit resolution → 2^10 = 1024 discrete levels.
 */
#define DAC6578_RESOLUTION_BITS          10U

/**
 * @brief Full-scale output voltage of DAC [V]
 *
 * This value represents the maximum output voltage corresponding
 * to the maximum digital code (DAC6578_MAX_VALUE).
 *
 * NOTE:
 * This depends on the reference voltage used in hardware.
 */
#define DAC6578_FS_VOLTAGE               5.0f

/**
 * @brief Maximum digital value for DAC input
 *
 * For 10-bit DAC:
 * max = (2^10 - 1) = 1023
 */
#define DAC6578_MAX_VALUE                1023U

/**
 * @brief 7-bit I2C device address
 *
 * NOTE:
 * Actual address may depend on hardware configuration (A0–A3 pins).
 */
#define DAC6578_DEV_ADDR                 0x48U

/**
 * @brief Default I2C communication timeout [ms]
 */
#define DAC6578_DEFAULT_TIMEOUT_MS       100U

/**
 * @brief Convert float voltage value to DAC digital code
 *
 * This macro performs:
 * - Linear scaling: V → DAC code
 * - Saturation:
 *      v <= 0      → 0
 *      v >= FS     → MAX
 * - Rounding to nearest integer
 *
 * Formula:
 *      code = (v / Vfs) * (2^N - 1)
 *
 * @param v Input voltage [V]
 *
 * @retval uint16_t DAC digital value (0–1023)
 *
 * @note
 * - Safe for runtime use (includes saturation)
 * - Uses floating point operations
 */
#define DAC6578_FLOAT_TO_DVALUE(v) \
    ( (uint16_t)( \
        ((v) <= 0.0f) ? 0U : \
        ((v) >= DAC6578_FS_VOLTAGE) ? DAC6578_MAX_VALUE : \
        ( ( (v) / DAC6578_FS_VOLTAGE ) * (float)DAC6578_MAX_VALUE + 0.5f ) \
    ) )

/**
 * @}
 */

/**
 * @defgroup DAC6578_PUBLIC_TYPES DAC6578 driver public data types
 * @{
 */

/**
 * @brief DAC6578 driver return status
 */
typedef enum
{
    DAC6578_STATUS_OK = 0,   /*!< Operation successful */
    DAC6578_STATUS_ERROR     /*!< Operation failed */
} dac6578_status_t;

/**
 * @brief DAC6578 channel selection
 *
 * Logical mapping of DAC outputs:
 * - CHANNEL_1 → DAC A
 * - CHANNEL_2 → DAC B
 * ...
 * - CHANNEL_8 → DAC H
 */
typedef enum
{
    DAC6578_CHANNEL_1 = 0,   /*!< DAC channel A */
    DAC6578_CHANNEL_2,       /*!< DAC channel B */
    DAC6578_CHANNEL_3,       /*!< DAC channel C */
    DAC6578_CHANNEL_4,       /*!< DAC channel D */
    DAC6578_CHANNEL_5,       /*!< DAC channel E */
    DAC6578_CHANNEL_6,       /*!< DAC channel F */
    DAC6578_CHANNEL_7,       /*!< DAC channel G */
    DAC6578_CHANNEL_8        /*!< DAC channel H */
} dac6578_channel_t;

/**
 * @}
 */

/**
 * @defgroup DAC6578_PUBLIC_FUNCTIONS DAC6578 driver interface functions
 * @{
 */

/**
 * @brief Initialize DAC6578 driver
 *
 * This function initializes:
 * - I2C peripheral (if required by implementation)
 * - Internal driver state
 * - DAC device configuration (optional)
 *
 * @retval DAC6578_STATUS_OK     Initialization successful
 * @retval DAC6578_STATUS_ERROR  Initialization failed
 */
dac6578_status_t DAC6578_Init(void);

/**
 * @brief Set DAC output value for selected channel
 *
 * Writes a 10-bit digital value to the selected DAC channel.
 *
 * @param channel  DAC channel (see ::dac6578_channel_t)
 * @param value    Digital value (0–1023)
 * @param timeout  I2C communication timeout [ms]
 *
 * @retval DAC6578_STATUS_OK     Value successfully written
 * @retval DAC6578_STATUS_ERROR  Communication or validation error
 *
 * @note
 * - No internal scaling is performed here
 * - Use DAC6578_FLOAT_TO_DVALUE() for voltage-based input
 */
dac6578_status_t DAC6578_SetChannelValue(dac6578_channel_t channel,
                                         uint16_t value,
                                         uint32_t timeout);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_HAL_DAC6578_DAC6578_H_ */
