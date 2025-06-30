/**
 ******************************************************************************
 * @file	bq25150.h
 *
 * @brief	BQ25150 battery charger IC driver provides hardware abstraction layer
 * 			for controlling and monitoring the TI BQ25150 linear battery charger.
 * 			This driver supports charging control, current limit settings, 
 * 			interrupt handling, ADC monitoring, watchdog timer configuration,
 * 			and register access for comprehensive charger management.
 * 			All BQ25150 driver interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	elektronika
 * @date	April 2025
 ******************************************************************************
 */

#ifndef CORE_HAL_BQ25150_BQ25150_H_
#define CORE_HAL_BQ25150_BQ25150_H_
/**
 * @defgroup HAL Hardware Abstraction Layer
 * @{
 */

/**
 * @defgroup BQ25150_DRIVER BQ25150 Battery Charger Driver
 * @{
 */

/**
 * @defgroup BQ25150_PUBLIC_DEFINES BQ25150 driver public defines
 * @{
 */
#define BQ25150_MASK_VIN_PGOOD             0x0001		/*!< VIN power good status mask */
#define BQ25150_MASK_THERMREG_ACTIVE       0x0002		/*!< Thermal regulation active mask */
#define BQ25150_MASK_VINDPM_ACTIVE         0x0004		/*!< VIN DPM (Dynamic Power Management) active mask */
#define BQ25150_MASK_VDPPM_ACTIVE          0x0008		/*!< VDPM active mask */
#define BQ25150_MASK_IINLIM_ACTIVE         0x0010		/*!< Input current limit active mask */
#define BQ25150_MASK_CHARGE_DONE           0x0020		/*!< Charge termination done mask */
#define BQ25150_MASK_CHRG_CV               0x0040		/*!< Constant voltage charge phase mask */
#define BQ25150_MASK_TS_HOT                0x0100		/*!< Temperature sensor hot condition mask */
#define BQ25150_MASK_TS_WARM               0x0200		/*!< Temperature sensor warm condition mask */
#define BQ25150_MASK_TS_COOL               0x0400		/*!< Temperature sensor cool condition mask */
#define BQ25150_MASK_TS_COLD               0x0800		/*!< Temperature sensor cold condition mask */
#define BQ25150_MASK_BAT_UVLO_FAULT        0x1000		/*!< Battery under-voltage lockout fault mask */
#define BQ25150_MASK_BAT_OCP_FAULT         0x2000		/*!< Battery over-current protection fault mask */
#define BQ25150_MASK_VIN_OVP_FAULT         0x8000		/*!< VIN over-voltage protection fault mask */
/**
 * @}
 */

/**
 * @defgroup BQ25150_PUBLIC_TYPES BQ25150 driver public data types
 * @{
 */

/**
 * @brief BQ25150 driver return status
 */
typedef enum
{
	BQ25150_STATUS_OK,				/*!< BQ25150 operation successful */
	BQ25150_STATUS_ERROR			/*!< BQ25150 operation failed */
}bq25150_status_t;

/**
 * @brief BQ25150 watchdog timer enable/disable state
 */
typedef enum
{
	BQ25150_WDG_STATUS_DISABLE	=	0,	/*!< Watchdog timer disabled */
	BQ25150_WDG_STATUS_ENABLE			/*!< Watchdog timer enabled */
}bq25150_wdg_status;

/**
 * @brief BQ25150 charging enable/disable state
 */
typedef enum
{
	BQ25150_CHARGE_STATUS_DISABLE = 0,	/*!< Charging disabled */
	BQ25150_CHARGE_STATUS_ENABLE		/*!< Charging enabled */
}bq25150_charge_status;

/**
 * @brief BQ25150 input current limit values in mA
 */
typedef enum
{
	BQ25150_ILIM_VALUE_50 	= 0,		/*!< Input current limit 50mA */
	BQ25150_ILIM_VALUE_100	= 1,		/*!< Input current limit 100mA */
	BQ25150_ILIM_VALUE_150	= 2,		/*!< Input current limit 150mA */
	BQ25150_ILIM_VALUE_200	= 3,		/*!< Input current limit 200mA */
	BQ25150_ILIM_VALUE_300	= 4,		/*!< Input current limit 300mA */
	BQ25150_ILIM_VALUE_400	= 5,		/*!< Input current limit 400mA */
	BQ25150_ILIM_VALUE_500	= 6,		/*!< Input current limit 500mA */
	BQ25150_ILIM_VALUE_600	= 7			/*!< Input current limit 600mA */
}bq25250_ilim_value_t;

/**
 * @brief BQ25150 interrupt callback function pointer type
 */
typedef void (*bq25150_intcb)();

/**
 * @defgroup BQ25150_PUBLIC_FUNCTIONS BQ25150 driver interface functions
 * @{
 */

/**
 * @brief	Initialize the BQ25150 battery charger driver
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_Init();
/**
 * @brief	Get charger interrupt flags
 * @param	intFlags: Pointer to variable to store interrupt flags
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
/**
 * @brief	Ping the BQ25150 device to verify communication
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_Ping(uint32_t timeout);

/**
 * @brief	Get charger interrupt flags
 * @param	intFlags: Pointer to variable to store interrupt flags
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_GetChargerIntFlags(uint16_t* intFlags, uint32_t timeout);

/**
 * @brief	Get ADC interrupt flags
 * @param	intFlags: Pointer to variable to store ADC interrupt flags
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_GetADCIntFlags(uint8_t* intFlags, uint32_t timeout);

/**
 * @brief	Get timer interrupt flags
 * @param	intFlags: Pointer to variable to store timer interrupt flags
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_GetTimerIntFlags(uint8_t* intFlags, uint32_t timeout);

/**
 * @brief	Set charger interrupt mask
 * @param	mask: Interrupt mask value to set
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_SetChargerIntMask(uint16_t mask, uint32_t timeout);

/**
 * @brief	Get current charger interrupt mask
 * @param	mask: Pointer to variable to store interrupt mask
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_GetChargerIntMask(uint16_t* mask, uint32_t timeout);

/**
 * @brief	Set ADC interrupt mask
 * @param	mask: ADC interrupt mask value to set
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_SetADCIntMask(uint8_t mask, uint32_t timeout);

/**
 * @brief	Get current ADC interrupt mask
 * @param	mask: Pointer to variable to store ADC interrupt mask
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_GetADCIntMask(uint8_t* mask, uint32_t timeout);

/**
 * @brief	Set timer interrupt mask
 * @param	mask: Timer interrupt mask value to set
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_SetTimerIntMask(uint8_t mask, uint32_t timeout);

/**
 * @brief	Get current timer interrupt mask
 * @param	mask: Pointer to variable to store timer interrupt mask
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_GetTimerIntMask(uint8_t* mask, uint32_t timeout);

/**
 * @brief	Set watchdog timer enable/disable status
 * @param	status: Watchdog timer status. See ::bq25150_wdg_status
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_WDG_SetStatus(bq25150_wdg_status status, uint32_t timeout);

/**
 * @brief	Set input current limit
 * @param	value: Input current limit value. See ::bq25250_ilim_value_t
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_ILim_Set(bq25250_ilim_value_t value, uint32_t timeout);

/**
 * @brief	Set charging enable/disable status
 * @param	status: Charging status. See ::bq25150_charge_status
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_Charge_SetStatus(bq25150_charge_status status, uint32_t timeout);

/**
 * @brief	Set charging current
 * @param	current: Charging current value in mA
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_Charge_Current_Set(uint32_t current, uint32_t timeout);

/**
 * @brief	Get current charging current setting
 * @param	current: Pointer to variable to store charging current in mA
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_Charge_Current_Get(uint32_t* current, uint32_t timeout);

/**
 * @brief	Set pre-charge current
 * @param	current: Pre-charge current value in mA
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_Charge_PreCurrent_Set(uint32_t current, uint32_t timeout);

/**
 * @brief	Get current pre-charge current setting
 * @param	current: Pointer to variable to store pre-charge current in mA
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_Charge_PreCurrent_Get(uint32_t* current, uint32_t timeout);

/**
 * @brief	Set charge termination current
 * @param	current: Termination current value in mA
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_Charge_TermCurrent_Set(uint32_t current, uint32_t timeout);

/**
 * @brief	Get current charge termination current setting
 * @param	current: Pointer to variable to store termination current in mA
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_Charge_TermCurrent_Get(uint32_t* current, uint32_t timeout);

/**
 * @brief	Set charge regulation voltage
 * @param	voltage: Regulation voltage value in Volts
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_Charge_RegVoltage_Set(float voltage, uint32_t timeout);

/**
 * @brief	Get current charge regulation voltage setting
 * @param	voltage: Pointer to variable to store regulation voltage in Volts
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_Charge_RegVoltage_Get(float* voltage, uint32_t timeout);

/**
 * @brief	Register interrupt callback function
 * @param	cb: Callback function pointer. See ::bq25150_intcb
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_RegCallback(bq25150_intcb cb);

/**
 * @brief	Read BQ25150 register content
 * @param	regAddr: Register address to read
 * @param	data: Pointer to variable to store register data
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25150_status_t
 */
bq25150_status_t BQ25150_ReadReg(uint8_t regAddr, uint8_t* data, uint32_t timeout);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_HAL_BQ25150_BQ25150_H_ */