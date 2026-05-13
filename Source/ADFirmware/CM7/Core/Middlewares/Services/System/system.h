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
 * @defgroup SYSTEM_SERVICE System service
 *
 * @brief Core system management service
 *
 * System service is responsible for:
 *  - System initialization
 *  - Hardware initialization
 *  - RGB status indication
 *  - Link status indication
 *  - Device information management
 *  - Error reporting
 *  - Central service startup
 *
 * The service acts as a top-level middleware component
 * responsible for initialization and coordination of
 * all major application services.
 *
 * @{
 */

/**
 * @defgroup SYSTEM_PUBLIC_DEFINES System service public defines
 * @{
 */
#define SYSTEM_TASK_NAME				CONF_SYSTEM_TASK_NAME				/*!< System service task name */
#define SYSTEM_TASK_PRIO				CONF_SYSTEM_TASK_PRIO				/*!< System service task priority */
#define SYSTEM_TASK_STACK_SIZE			CONF_SYSTEM_TASK_STACK_SIZE			/*!< System service task stack size */
#define SYSTEM_RGB_DEFAULT_BRIGHTNESS   CONF_SYSTEM_RGB_DEFAULT_BRIGHTNESS	/*!< System service RGB Diode brightness */

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
 * @brief Initialize system service
 *
 * This function creates system task and initializes
 * internal synchronization resources.
 *
 * @retval SYSTEM_STATUS_OK     System service successfully initialized
 * @retval SYSTEM_STATUS_ERROR  System service initialization failed
 */
system_status_t SYSTEM_Init(void);

/**
 * @brief Start RTOS scheduler
 *
 * This function initializes CMSIS-RTOS kernel and
 * starts task scheduling.
 *
 * @retval SYSTEM_STATUS_OK     Scheduler successfully started
 * @retval SYSTEM_STATUS_ERROR  Scheduler start failed
 *
 * @note Function should never return on success
 */
system_status_t SYSTEM_Start(void);

/**
 * @brief Report system error state
 *
 * This function updates system RGB indication
 * according to specified error level.
 *
 * @param errorLevel System error severity level
 *
 * @retval SYSTEM_STATUS_OK     Error state successfully reported
 * @retval SYSTEM_STATUS_ERROR  Error indication failed
 */
system_status_t SYSTEM_ReportError(system_error_level_t errorLevel);

/**
 * @brief Update system link status
 *
 * This function updates internal link state and
 * RGB indication according to current link status.
 *
 * @param linkStatus System link status value
 *
 * @retval SYSTEM_STATUS_OK     Link status successfully updated
 * @retval SYSTEM_STATUS_ERROR  Failed to update link status
 *
 * @note Function is thread-safe
 */
system_status_t SYSTEM_SetLinkStatus(system_link_status_t linkStatus);

/**
 * @brief Set device name
 *
 * This function updates internal device name value.
 *
 * @param deviceName Null-terminated device name string
 *
 * @retval SYSTEM_STATUS_OK     Device name successfully updated
 * @retval SYSTEM_STATUS_ERROR  Invalid input or internal error
 *
 * @note Function is thread-safe
 */
system_status_t SYSTEM_SetDeviceName(const char* deviceName);

/**
 * @brief Get device name
 *
 * This function copies internal device name
 * to user provided buffer.
 *
 * @param deviceName Pointer to output buffer
 * @param deviceNameSize Pointer to output string length
 *
 * @retval SYSTEM_STATUS_OK     Device name successfully retrieved
 * @retval SYSTEM_STATUS_ERROR  Invalid input or internal error
 *
 * @note Output string is null-terminated
 * @note Function is thread-safe
 */
system_status_t SYSTEM_GetDeviceName(char* deviceName, uint32_t* deviceNameSize);

/**
 * @brief Set device serial number
 *
 * This function updates internal device serial value.
 *
 * @param deviceSerial Null-terminated serial string
 *
 * @retval SYSTEM_STATUS_OK     Device serial successfully updated
 * @retval SYSTEM_STATUS_ERROR  Invalid input or internal error
 *
 * @note Function is thread-safe
 */
system_status_t SYSTEM_SetDeviceSerial(const char* deviceSerial);

/**
 * @brief Get device serial number
 *
 * This function copies internal device serial
 * to user provided buffer.
 *
 * @param deviceSerial Pointer to output buffer
 * @param deviceSerialSize Pointer to output string length
 *
 * @retval SYSTEM_STATUS_OK     Device serial successfully retrieved
 * @retval SYSTEM_STATUS_ERROR  Invalid input or internal error
 *
 * @note Output string is null-terminated
 * @note Function is thread-safe
 */
system_status_t SYSTEM_GetDeviceSerial(char* deviceSerial, uint32_t* deviceSerialSize);

/**
 * @brief Set firmware version string
 *
 * This function updates internal firmware version value.
 *
 * @param fwVersion Null-terminated firmware version string
 *
 * @retval SYSTEM_STATUS_OK     Firmware version successfully updated
 * @retval SYSTEM_STATUS_ERROR  Invalid input or internal error
 *
 * @note Function is thread-safe
 */
system_status_t SYSTEM_SetFWVersion(const char* fwVersion);

/**
 * @brief Get firmware version string
 *
 * This function copies internal firmware version
 * to user provided buffer.
 *
 * @param fwVersion Pointer to output buffer
 * @param fwVersionSize Pointer to output string length
 *
 * @retval SYSTEM_STATUS_OK     Firmware version successfully retrieved
 * @retval SYSTEM_STATUS_ERROR  Invalid input or internal error
 *
 * @note Output string is null-terminated
 * @note Function is thread-safe
 */
system_status_t SYSTEM_GetFWVersion(char* fwVersion, uint32_t* fwVersionSize);

/**
 * @brief Set RGB LED color
 *
 * This function updates internal RGB values and
 * notifies system task to apply PWM changes.
 *
 * @param value RGB color structure
 *
 * @retval SYSTEM_STATUS_OK     RGB color successfully updated
 * @retval SYSTEM_STATUS_ERROR  Failed to update RGB color
 *
 * @note Function is thread-safe
 */
system_status_t SYSTEM_SetRGB(system_rgb_value_t value);

/**
 * @brief Restart MCU
 *
 * This function performs software reset using
 * NVIC system reset mechanism.
 *
 * @retval SYSTEM_STATUS_OK Reset successfully requested
 *
 * @note Function should never return after reset
 */
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
