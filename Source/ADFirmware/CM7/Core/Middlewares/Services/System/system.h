/**
 ******************************************************************************
 * @file   	system.h
 *
 * @brief  	System service provides core system functionality including device
 * 			management, error reporting, status indication through LEDs, and 
 * 			system state monitoring. It manages system initialization, device
 * 			naming, RGB LED control, link status indication, and error level
 * 			reporting with visual feedback.
 * 			All system service interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_H_
#define CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_H_

#include "globalConfig.h"
/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup SYSTEM_SERVICE System Service
 * @{
 */

/**
 * @defgroup SYSTEM_PUBLIC_DEFINES System service public defines
 * @{
 */
#define SYSTEM_TASK_NAME				CONF_SYSTEM_TASK_NAME			/*!< System service task name */
#define SYSTEM_TASK_PRIO				CONF_SYSTEM_TASK_PRIO			/*!< System service task priority */
#define SYSTEM_TASK_STACK_SIZE			CONF_SYSTEM_TASK_STACK_SIZE		/*!< System service task stack size */

#define	SYSTEM_ERROR_STATUS_DIODE_PORT	CONF_SYSTEM_ERROR_STATUS_DIODE_PORT	/*!< Error status LED GPIO port */
#define	SYSTEM_ERROR_STATUS_DIODE_PIN	CONF_SYSTEM_ERROR_STATUS_DIODE_PIN	/*!< Error status LED GPIO pin */

#define	SYSTEM_LINK_STATUS_DIODE_PORT	CONF_SYSTEM_LINK_STATUS_DIODE_PORT	/*!< Link status LED GPIO port */
#define	SYSTEM_LINK_STATUS_DIODE_PIN	CONF_SYSTEM_LINK_STATUS_DIODE_PIN	/*!< Link status LED GPIO pin */

/**
 * @}
 */

/**
 * @defgroup SYSTEM_PUBLIC_TYPES System service public data types
 * @{
 */

/**
 * @brief System service state
 */
typedef enum
{
	SYSTEM_STATE_INIT,				/*!< System service initialization state */
	SYSTEM_STATE_SERVICE,			/*!< System service is in service state */
	SYSTEM_STATE_ERROR				/*!< System service is in error state */
}system_state_t;

/**
 * @brief System service return status
 */
typedef enum
{
	SYSTEM_STATUS_OK,				/*!< System operation successful */
	SYSTEM_STATUS_ERROR				/*!< System operation failed */
}system_status_t;

/**
 * @brief System error severity levels
 */
typedef enum
{
	SYSTEM_ERROR_LEVEL_LOW,			/*!< Low severity error */
	SYSTEM_ERROR_LEVEL_MEDIUM,		/*!< Medium severity error */
	SYSTEM_ERROR_LEVEL_HIGH			/*!< High severity error */
}system_error_level_t;


/**
 * @brief System link status states
 */
typedef enum
{
	SYSTEM_LINK_STATUS_UP,			/*!< Link is established and active */
	SYSTEM_LINK_STATUS_DOWN			/*!< Link is down or inactive */
}system_link_status_t;


/**
 * @brief RGB color value structure
 */
typedef struct
{
	uint8_t red;					/*!< Red color component (0-255) */
	uint8_t green;					/*!< Green color component (0-255) */
	uint8_t blue;					/*!< Blue color component (0-255) */
}system_rgb_value_t;


/**
 * @}
 */


/**
 * @defgroup SYSTEM_PUBLIC_FUNCTIONS System service interface functions
 * @{
 */

/**
 * @brief	Initialize the system service
 * @retval	::system_status_t
 */
system_status_t SYSTEM_Init();
/**
 * @brief	Start the system service
 * @retval	::system_status_t
 */
system_status_t SYSTEM_Start();
/**
 * @brief	Report system error with specified severity level
 * @param	errorLevel: Error severity level. See ::system_error_level_t
 * @retval	::system_status_t
 */
system_status_t SYSTEM_ReportError(system_error_level_t errorLevel);
/**
 * @brief	Set the system link status indication
 * @param	linkStatus: Link status to set. See ::system_link_status_t
 * @retval	::system_status_t
 */
system_status_t SYSTEM_SetLinkStatus(system_link_status_t linkStatus);
/**
 * @brief	Set the device name
 * @param	deviceName: Null-terminated string containing the device name
 * @retval	::system_status_t
 */
system_status_t SYSTEM_SetDeviceName(const char* deviceName);
/**
 * @brief	Get the current device name
 * @param	deviceName: Buffer to store the device name
 * @param	deviceNameSize: Pointer to variable containing buffer size, updated with actual name length
 * @retval	::system_status_t
 */
system_status_t SYSTEM_GetDeviceName(char* deviceName, uint32_t* deviceNameSize);
/**
 * @brief	Set RGB LED color values
 * @param	value: RGB color values structure. See ::system_rgb_value_t
 * @retval	::system_status_t
 */
/**
 * @brief   Set device serial number
 *
 * @param   deviceSerial Pointer to null-terminated string containing device serial
 *
 * @retval  ::SYSTEM_STATUS_OK       Serial successfully set
 * @retval  ::SYSTEM_STATUS_ERROR    Invalid input or internal error
 *
 * @note    Maximum allowed length is defined by CONF_SYSTEM_DEFAULT_DEVICE_SERIAL_MAX
 */
system_status_t SYSTEM_SetDeviceSerial(const char* deviceSerial);

/**
 * @brief   Get device serial number
 *
 * @param   deviceSerial Pointer to buffer where serial will be copied
 * @param   deviceSerialSize Pointer to variable where serial length will be stored
 *
 * @retval  ::SYSTEM_STATUS_OK       Serial successfully retrieved
 * @retval  ::SYSTEM_STATUS_ERROR    Invalid input or internal error
 *
 * @note    Output string is null-terminated
 * @note    Caller must ensure buffer is large enough
 */
system_status_t SYSTEM_GetDeviceSerial(char* deviceSerial, uint32_t* deviceSerialSize);

/**
 * @brief   Set firmware version string
 *
 * @param   fwVersion Pointer to null-terminated string containing firmware version
 *
 * @retval  ::SYSTEM_STATUS_OK       Firmware version successfully set
 * @retval  ::SYSTEM_STATUS_ERROR    Invalid input or internal error
 *
 * @note    Maximum allowed length is defined by CONF_SYSTEM_DEFAULT_FW_VERSION_MAX
 */
system_status_t SYSTEM_SetFWVersion(const char* fwVersion);

/**
 * @brief   Get firmware version string
 *
 * @param   fwVersion Pointer to buffer where firmware version will be copied
 * @param   fwVersionSize Pointer to variable where string length will be stored
 *
 * @retval  ::SYSTEM_STATUS_OK       Firmware version successfully retrieved
 * @retval  ::SYSTEM_STATUS_ERROR    Invalid input or internal error
 *
 * @note    Output string is null-terminated
 * @note    Caller must ensure buffer is large enough
 */
system_status_t SYSTEM_GetFWVersion(char* fwVersion, uint32_t* fwVersionSize);


system_status_t SYSTEM_Restart(void);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_H_ */
