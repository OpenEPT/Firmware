/**
 ******************************************************************************
 * @file   	drv_aout.h
 *
 * @brief  	Analog Output (AOUT) driver provides hardware abstraction layer
 * 			for output enable/disable functionality, and vaule manipulation.
 * 			All AOUT driver interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2023
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_ANALOGOUT_DRV_AOUTH_
#define CORE_DRIVERS_PLATFORM_ANALOGOUT_DRV_AOUTH_

#include <stdint.h>

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup AOUT_DRIVER Analog Output Driver
 * @{
 */

/**
 * @defgroup AOUT_PUBLIC_TYPES AOUT driver public data types
 * @{
 */

/**
 * @brief AOUT driver return status
 */
typedef enum
{
	DRV_AOUT_STATUS_OK,				/*!< AOUT operation successful */
	DRV_AOUT_STATUS_ERROR			/*!< AOUT operation failed */
}drv_aout_status_t;

/**
 * @brief AOUT output enable/disable status
 */
typedef enum
{
	DRV_AOUT_ACTIVE_STATUS_DISABLED = 0,	/*!< Analog output disabled */
	DRV_AOUT_ACTIVE_STATUS_ENABLED			/*!< Analog output enabled */
}drv_aout_active_status_t;

/**
 * @brief AOUT output enable/disable status
 */
typedef enum
{
	DRV_AOUT_CHANNEL_A = 0,					/*!< DAC Channel A */
	DRV_AOUT_CHANNEL_B = 1,					/*!< DAC Channel B */
	DRV_AOUT_CHANNEL_C = 2,					/*!< DAC Channel C */
	DRV_AOUT_CHANNEL_D = 3,					/*!< DAC Channel D */
	DRV_AOUT_CHANNEL_E = 4,					/*!< DAC Channel E */
	DRV_AOUT_CHANNEL_F = 5,					/*!< DAC Channel F */
	DRV_AOUT_CHANNEL_G = 6,					/*!< DAC Channel G */
	DRV_AOUT_CHANNEL_H = 7					/*!< DAC Channel H */
}drv_aout_channel_t;

/**
 * @}
 */

/**
 * @defgroup AOUT_PUBLIC_FUNCTIONS AOUT driver interface functions  
 * @{
 */

/**
 * @brief	Initialize analog output peripheral
 * @retval	::drv_aout_status_t
 */
drv_aout_status_t DRV_AOUT_Init(void);

/**
 * @brief	Enable or disable analog output
 * @param	aStatus: Output enable status. See ::drv_aout_active_status_t
 * @retval	::drv_aout_status_t
 */
drv_aout_status_t DRV_AOUT_SetEnable(drv_aout_active_status_t aStatus);

/**
 * @brief	Set analog output value
 * @param	value: Value
 * @retval	::drv_aout_status_t
 */
drv_aout_status_t DRV_AOUT_SetValue(uint32_t value, drv_aout_channel_t channel);
/**
 * @brief   Set analog output voltage for selected channel
 *
 * This function converts a floating-point voltage value [V] into the
 * corresponding DAC digital code and writes it to the specified channel.
 *
 * Internally:
 * - Performs voltage-to-digital conversion using DAC characteristics
 * - Calls ::DRV_AOUT_SetValue() to update DAC output
 *
 * @param   voltage: Desired output voltage [V]
 *                  Valid range depends on DAC full-scale voltage
 *                  (typically 0V to DAC6578_FS_VOLTAGE)
 *
 * @param   channel: Target analog output channel.
 *                  See ::drv_aout_channel_t
 *
 * @retval  DRV_AOUT_STATUS_OK     Voltage successfully applied
 * @retval  DRV_AOUT_STATUS_ERROR  Conversion or write operation failed
 *
 * @note
 * - Input voltage is automatically saturated to valid DAC range
 * - Resolution is limited by DAC (10-bit → 1024 steps)
 * - Accuracy depends on reference voltage and analog front-end
 */
drv_aout_status_t DRV_AOUT_SetVoltage(float voltage, drv_aout_channel_t channel);
/**
 * @brief   Convert analog voltage value [V] to DAC digital code
 *
 * This function converts a floating-point voltage value into a
 * corresponding DAC digital value using underlying DAC driver logic.
 *
 * Internally, this uses DAC-specific scaling (e.g. DAC6578).
 *
 * @param   value: Input voltage [V]
 *
 * @retval  uint16_t Converted DAC digital value (0–1023)
 */
uint16_t DRV_AOUT_ConvertFloatToDigital(float value);
/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_DRIVERS_PLATFORM_ANALOGOUT_DRV_AOUTH_ */
