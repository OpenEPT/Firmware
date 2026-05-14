/**
 ******************************************************************************
 * @file	bq25180.h
 *
 * @brief	BQ25180 battery charger IC driver provides hardware abstraction layer
 * 			for controlling and monitoring the TI BQ25180 linear battery charger.
 * 			This driver supports charging control, current limit settings, 
 * 			interrupt handling, ADC monitoring, watchdog timer configuration,
 * 			and register access for comprehensive charger management.
 * 			All BQ25180 driver interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Dimitrije Lilic, Haris Turkmanovic
 * @date	April 2026
 ******************************************************************************
 */

#ifndef CORE_HAL_BQ25180_BQ25180_H_
#define CORE_HAL_BQ25180_BQ25180_H_
/**
 * @defgroup HAL Hardware Abstraction Layer
 * @{
 */

/**
 * @defgroup BQ25180_DRIVER BQ25180 Battery Charger Driver
 * @{
 */

/**
 * @defgroup BQ25180_PUBLIC_DEFINES BQ25180 driver public defines
 * @{
 */
#define BQ25180_MASK_VIN_PGOOD             0x0001      /*!< VIN input power good status flag */
#define BQ25180_MASK_THERMREG_ACTIVE       0x0002      /*!< Thermal regulation active status flag */
#define BQ25180_MASK_CHARGE_STATUS_CHANGED 0x0004      /*!< Charging status changed interrupt flag */
#define BQ25180_MASK_VINDPM_ACTIVE         0x0008      /*!< VIN dynamic power management active flag */
//#define BQ25180_MASK_VDPPM_ACTIVE         0x0008      /*!< VDPPM active status flag */

#define BQ25180_MASK_TS_HOT                0x0100      /*!< TS pin hot temperature condition flag */
#define BQ25180_MASK_TS_WARM               0x0200      /*!< TS pin warm temperature condition flag */
#define BQ25180_MASK_TS_COOL               0x0400      /*!< TS pin cool temperature condition flag */
#define BQ25180_MASK_TS_COLD               0x0800      /*!< TS pin cold temperature condition flag */

#define BQ25180_MASK_BAT_UVLO_FAULT        0x1000      /*!< Battery under-voltage lockout fault flag */
#define BQ25180_MASK_BAT_OCP_FAULT         0x2000      /*!< Battery over-current protection fault flag */
#define BQ25180_MASK_VIN_OVP_FAULT         0x8000      /*!< VIN over-voltage protection fault flag */
/**
 * @}
 */

/**
 * @defgroup BQ25180_PUBLIC_TYPES BQ25180 driver public data types
 * @{
 */

/**
 * @brief BQ25180 driver return status
 */
typedef enum
{
	BQ25180_STATUS_OK,				/*!< BQ25180 operation successful */
	BQ25180_STATUS_ERROR			/*!< BQ25180 operation failed */
}bq25180_status_t;

/**
 * @brief BQ25180 watchdog timer enable/disable state
 */
typedef enum
{
	BQ25180_WDG_STATUS_DISABLE	=	0,	/*!< Watchdog timer disabled */
	BQ25180_WDG_STATUS_ENABLE			/*!< Watchdog timer enabled */
}bq25180_wdg_status;

/**
 * @brief BQ25180 charging enable/disable state
 */
typedef enum
{
	BQ25180_CHARGE_STATUS_DISABLE = 0,	/*!< Charging disabled */
	BQ25180_CHARGE_STATUS_ENABLE		/*!< Charging enabled */
}bq25180_charge_status;

/**
 * @brief BQ25180 input current limit values in mA
 */
typedef enum
{
	BQ25180_ILIM_VALUE_50 	= 0,		/*!< Input current limit 50mA */
	BQ25180_ILIM_VALUE_100	= 1,		/*!< Input current limit 100mA */
	BQ25180_ILIM_VALUE_200	= 2,		/*!< Input current limit 150mA */
	BQ25180_ILIM_VALUE_300	= 3,		/*!< Input current limit 200mA */
	BQ25180_ILIM_VALUE_400	= 4,		/*!< Input current limit 300mA */
	BQ25180_ILIM_VALUE_500	= 5,		/*!< Input current limit 400mA */
	BQ25180_ILIM_VALUE_665	= 6,		/*!< Input current limit 500mA */
	BQ25180_ILIM_VALUE_1050	= 7			/*!< Input current limit 600mA */
}bq25180_ilim_value_t;


/**
 * @brief BQ25180 termination current values
 */
typedef enum
{
    BQ25180_TCURRENT_VALUE_DISABLED     = 0,    /*!< Termination current disabled */
    BQ25180_TCURRENT_VALUE_5            = 1,    /*!< Termination current 5mA */
    BQ25180_TCURRENT_VALUE_10           = 2,    /*!< Termination current 10mA */
    BQ25180_TCURRENT_VALUE_20           = 3     /*!< Termination current 20mA */
}bq25180_tcurrent_value_t;

/**
 * @brief BQ25180 safety timer configuration values
 */
typedef enum
{
    BQ25180_STIMER_VALUE_3H             = 0,    /*!< Safety timer 3 hours */
    BQ25180_STIMER_VALUE_6H             = 1,    /*!< Safety timer 6 hours */
    BQ25180_STIMER_VALUE_12H            = 2,    /*!< Safety timer 12 hours */
    BQ25180_STIMER_VALUE_DISABLED       = 3     /*!< Safety timer disabled */
}bq25180_stimer_value_t;

/**
 * @brief BQ25180 charging state values
 */
typedef enum
{
    BQ25180_CHARGING_STATUS_NOT         = 0,    /*!< Charging not active */
    BQ25180_CHARGING_STATUS_CC          = 1,    /*!< Constant current charging phase */
    BQ25180_CHARGING_STATUS_CV          = 2,    /*!< Constant voltage charging phase */
    BQ25180_CHARGING_STATUS_DONE        = 3     /*!< Charging completed */
}bq25180_charging_status_t;

/**
 * @brief BQ25180 interrupt callback function pointer type
 */
typedef void (*bq25180_intcb)();

/**
 * @defgroup BQ25180_PUBLIC_FUNCTIONS BQ25180 driver interface functions
 * @{
 */

/**
 * @brief	Initialize the BQ25180 battery charger driver
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_Init();
/**
 * @brief	Get charger interrupt flags
 * @param	intFlags: Pointer to variable to store interrupt flags
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
/**
 * @brief	Ping the BQ25180 device to verify communication
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_Ping(uint32_t timeout);

/**
 * @brief	Get charger interrupt flags
 * @param	intFlags: Pointer to variable to store interrupt flags
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_GetChargerIntFlags(uint8_t* intFlags, uint32_t timeout);


/**
 * @brief Set charger safety timer state
 * @param value: Safety timer configuration value. See ::bq25180_stimer_value_t
 * @param timeout: Communication timeout in milliseconds
 * @retval ::bq25180_status_t
 */
bq25180_status_t BQ25180_SetChargerTimerState(bq25180_stimer_value_t value, uint32_t timeout);



/**
 * @brief	Set charger interrupt mask
 * @param	mask: Interrupt mask value to set
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_SetChargerIntMask(uint8_t mask, uint32_t timeout);

/**
 * @brief	Get current charger interrupt mask
 * @param	mask: Pointer to variable to store interrupt mask
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_GetChargerIntMask(uint8_t* mask, uint32_t timeout);

/**
 * @brief	Set watchdog timer enable/disable status
 * @param	status: Watchdog timer status. See ::bq25180_wdg_status
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_WDG_SetStatus(bq25180_wdg_status status, uint32_t timeout);

/**
 * @brief	Set input current limit
 * @param	value: Input current limit value. See ::bq25180_ilim_value_t
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_ILim_Set(bq25180_ilim_value_t value, uint32_t timeout);

/**
 * @brief	Set charging enable/disable status
 * @param	status: Charging status. See ::bq25180_charge_status
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_Charge_ChargeStatus_Set(bq25180_charge_status status, uint32_t timeout);



/**
 * @brief	Set charging current
 * @param	current: Charging current value in mA
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_Charge_Current_Set(uint32_t current, uint32_t timeout);

/**
 * @brief	Get current charging current setting
 * @param	current: Pointer to variable to store charging current in mA
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_Charge_Current_Get(uint32_t* current, uint32_t timeout);

/**
 * @brief	Set pre-charge current
 * @param	current: Pre-charge current value in mA
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_Charge_PreCurrent_Set(uint32_t current, uint32_t timeout);

/**
 * @brief	Get current pre-charge current setting
 * @param	current: Pointer to variable to store pre-charge current in mA
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_Charge_PreCurrent_Get(uint32_t* current, uint32_t timeout);

/**
 * @brief	Set charge termination current
 * @param	current: Termination current value in mA
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_Charge_TermCurrent_Set(bq25180_tcurrent_value_t value, uint32_t timeout);

/**
 * @brief	Get current charge termination current setting
 * @param	current: Pointer to variable to store termination current in mA
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_Charge_TermCurrent_Get(uint32_t* current, uint32_t timeout);

/**
 * @brief	Set charge regulation voltage
 * @param	voltage: Regulation voltage value in Volts
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_Charge_RegVoltage_Set(float voltage, uint32_t timeout);

/**
 * @brief	Get current charge regulation voltage setting
 * @param	voltage: Pointer to variable to store regulation voltage in Volts
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_Charge_RegVoltage_Get(float* voltage, uint32_t timeout);

/**
 * @brief	Register interrupt callback function
 * @param	cb: Callback function pointer. See ::bq25180_intcb
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_RegCallback(bq25180_intcb cb);

/**
 * @brief	Read BQ25180 register content
 * @param	regAddr: Register address to read
 * @param	data: Pointer to variable to store register data
 * @param	timeout: Communication timeout in milliseconds
 * @retval	::bq25180_status_t
 */
bq25180_status_t BQ25180_ReadReg(uint8_t regAddr, uint8_t* data, uint32_t timeout);



/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_HAL_BQ25180_BQ25180_H_ */
