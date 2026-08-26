/**
 ******************************************************************************
 * @file    system.c
 *
 * @brief   System service is the core service responsible for system 
 *          initialization, error handling, status reporting, and device 
 *          configuration. It manages LED indicators, RGB status colors,
 *          device naming, and other system-wide functionality.
 *
 * @author  Haris Turkmanovic
 * @email   haris.turkmanovic@gmail.com
 * @date    November 2023
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_C_
#define CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_C_


#include <stdint.h>
#include <string.h>

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


#include "lwip.h"

#include "drv_system.h"
#include "drv_gpio.h"
#include "drv_timer.h"
#include "ads9224r.h"

#include "system.h"
#include "logging.h"
#include "network.h"
#include "control.h"
#include "sstream.h"
#include "energy_debugger.h"
#include "dpcontrol.h"
#include "charger.h"
#include "eez_dib.h"
#include "fsystem.h"
#include "configuration.h"
#include "Bringup/bringup.h"


/**
 * @defgroup SERVICES Service
 * @{
 */

/**
 * @defgroup SYSTEM_SERVICE System service
 * @{
 */

/**
 * @defgroup SYSTEM_DEFINES System task defines and default values
 * @{
 */
#define  SYSTEM_MASK_RGB_SET_COLOR	0x00000001  /**< Task notification flag for setting RGB LED color */
/**
 * @}
 */

/**
 * @defgroup SYSTEM_PRIVATE_STRUCTURES System service private structures
 * @{
 */
/**
 * @brief Structure holding system service data
 */

typedef  struct
{
	system_state_t 			state;         /**< Current state of the system service */
	SemaphoreHandle_t 		initSig;       /**< Semaphore for signaling initialization completion */
	system_link_status_t	linkStatus;    /**< Current link status (UP/DOWN) */
	system_rgb_value_t		rgbValue;      /**< Current RGB LED values */
	SemaphoreHandle_t		guard;         /**< Mutex for thread-safe parameter access */
	char					deviceName[CONF_CONFIGURATION_MAX_PARAM_VALUESIZE]; /**< Device name storage */
	char                    deviceSerial[CONF_CONFIGURATION_MAX_PARAM_VALUESIZE];
	char                    deviceFwVersion[CONF_CONFIGURATION_MAX_PARAM_VALUESIZE];
}system_data_t;
/**
 * @}
 */

/**
 * @defgroup SYSTEM_PRIVATE_DATA System service private data
 * @{
 */
static system_data_t prvSYSTEM_DATA;		/**< System service data instance */


static TaskHandle_t  prvSYSTEM_TASK_HANDLE;	/**< System task handle */
/**
 * @}
 */

/**
 * @defgroup SYSTEM_PRIVATE_FUNCTIONS System service private functions
 * @{
 */

/**
 * @brief Set RGB LED PWM duty cycle values
 *
 * This function updates PWM duty cycle values for RGB LED channels.
 *
 * @param red     Red channel intensity value
 * @param blue    Blue channel intensity value
 * @param green   Green channel intensity value
 *
 * @retval SYSTEM_STATUS_OK     RGB state successfully updated
 * @retval SYSTEM_STATUS_ERROR  Failed to update RGB PWM channels
 */
static system_status_t prvSYSTEM_SetRGBState(uint8_t red, uint8_t blue, uint8_t green)
{
	if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_4, red, portMAX_DELAY) != DRV_TIMER_STATUS_OK) return SYSTEM_STATUS_OK;
	if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_3, green, portMAX_DELAY) != DRV_TIMER_STATUS_OK) return SYSTEM_STATUS_OK;
	if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_2, blue, portMAX_DELAY) != DRV_TIMER_STATUS_OK) return SYSTEM_STATUS_OK;
	return SYSTEM_STATUS_ERROR;
}
/**
 * @brief Acquisition state change callback
 *
 * This callback is triggered whenever stream
 * acquisition state changes.
 *
 * @param id Stream identifier
 * @param state New acquisition state
 *
 * @retval None
 */
static void prvSYSTEM_AcquisitionStateChanged(uint32_t id, sstream_acquisition_state_t state)
{

}

/**
 * @brief Initialize PWM timer used for RGB LED control
 *
 * @retval SYSTEM_STATUS_OK     PWM successfully initialized
 * @retval SYSTEM_STATUS_ERROR  PWM initialization failed
 */
static system_status_t prvSYSTEM_InitPWM(void)
{
	drv_timer_channel_config_t pwmTimerChConfig;
	drv_timer_config_t         pwmTimerConfig;

	pwmTimerConfig.mode       = DRV_TIMER_COUNTER_MODE_UP;
	pwmTimerConfig.prescaler  = 2000;
	pwmTimerConfig.preload    = DRV_TIMER_PRELOAD_DISABLE;
	pwmTimerConfig.div        = DRV_TIMER_DIV_1;
	pwmTimerConfig.period     = 256;

	if(DRV_Timer_Init_Instance(DRV_TIMER_1, &pwmTimerConfig) != DRV_TIMER_STATUS_OK)
	{
		return SYSTEM_STATUS_ERROR;
	}

	pwmTimerChConfig.mode = DRV_TIMER_CHANNEL_MODE_PWM1;

	if(DRV_Timer_Channel_Init(DRV_TIMER_1, DRV_TIMER_CHANNEL_2, &pwmTimerChConfig) != DRV_TIMER_STATUS_OK)
	{
		return SYSTEM_STATUS_ERROR;
	}

	if(DRV_Timer_Channel_Init(DRV_TIMER_1, DRV_TIMER_CHANNEL_3, &pwmTimerChConfig) != DRV_TIMER_STATUS_OK)
	{
		return SYSTEM_STATUS_ERROR;
	}

	if(DRV_Timer_Channel_Init(DRV_TIMER_1, DRV_TIMER_CHANNEL_4, &pwmTimerChConfig) != DRV_TIMER_STATUS_OK)
	{
		return SYSTEM_STATUS_ERROR;
	}

	return SYSTEM_STATUS_OK;
}



/**
 * @brief Main system service task function
 * 
 * This task is responsible for:
 *  - Initializing system hardware (GPIOs, timers, LEDs)
 *  - Setting up PWM for RGB LEDs
 *  - Initializing and starting other services
 *  - Processing system events and state changes
 *  - Managing system indicators
 * 
 * The task transitions through multiple states:
 *  - SYSTEM_STATE_INIT: Performs hardware and service initialization
 *  - SYSTEM_STATE_SERVICE: Processes system events and notifications
 *  - SYSTEM_STATE_ERROR: Handles system errors
 * 
 * @param None
 * @retval None
 */

static void prvSYSTEM_Task()
{
	uint32_t					notifyValue;

	uint8_t def;
	char buffer[CONF_CONFIGURATION_MAX_PARAM_VALUESIZE];

	prvSYSTEM_DATA.linkStatus    = SYSTEM_LINK_STATUS_DOWN;
	prvSYSTEM_DATA.rgbValue.red = 0;
	prvSYSTEM_DATA.rgbValue.green =0;
	prvSYSTEM_DATA.rgbValue.blue = 50;

	for(;;)
	{
		switch(prvSYSTEM_DATA.state)
		{
		case SYSTEM_STATE_INIT:
			notifyValue = 0;

			if(DRV_SYSTEM_InitDrivers() != DRV_SYSTEM_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			if(prvSYSTEM_InitPWM() != SYSTEM_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}

			if(LOGGING_Init(2000) != LOGGING_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "Logging service successfully initialized\r\n");

			if(FSYSTEM_Init(2000) != FSYSTEM_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
			}
			LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "File system service successfully initialized\r\n");

			if(CONFIGURATION_Init(2000) != CONFIGURATION_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "Configuration service successfully initialized\r\n");



			/* Device Name */
			if(CONFIGURATION_GetParameter_String("DEV_NAME", buffer, sizeof(buffer), &def) == CONFIGURATION_STATUS_OK)
			{
				memset(prvSYSTEM_DATA.deviceName, 0, sizeof(prvSYSTEM_DATA.deviceName));
				memcpy(prvSYSTEM_DATA.deviceName, buffer, strlen(buffer));

				LOGGING_Write("System", LOGGING_MSG_TYPE_INFO,"Device name loaded: %s\r\n",prvSYSTEM_DATA.deviceName);
			}
			else
			{
				LOGGING_Write("System", LOGGING_MSG_TYPE_WARNING,"Failed to load device name\r\n");
			}

			/* HW Serial */
			if(CONFIGURATION_GetParameter_String("HW_SERIAL", buffer, sizeof(buffer), &def) == CONFIGURATION_STATUS_OK)
			{
				memset(prvSYSTEM_DATA.deviceSerial, 0, sizeof(prvSYSTEM_DATA.deviceSerial));
				memcpy(prvSYSTEM_DATA.deviceSerial, buffer, strlen(buffer));

				LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "Device serial loaded: %s\r\n", prvSYSTEM_DATA.deviceSerial);
			}
			else
			{
				LOGGING_Write("System", LOGGING_MSG_TYPE_WARNING, "Failed to load device serial\r\n");
			}

			/* FW Version */
			if(CONFIGURATION_GetParameter_String("FW_VERSION", buffer, sizeof(buffer), &def) == CONFIGURATION_STATUS_OK)
			{
				memset(prvSYSTEM_DATA.deviceFwVersion, 0, sizeof(prvSYSTEM_DATA.deviceFwVersion));
				memcpy(prvSYSTEM_DATA.deviceFwVersion, buffer, strlen(buffer));

				LOGGING_Write("System", LOGGING_MSG_TYPE_INFO,"FW version loaded: %s\r\n",prvSYSTEM_DATA.deviceFwVersion);
			}
			else
			{
				LOGGING_Write("System", LOGGING_MSG_TYPE_WARNING,"Failed to load FW version\r\n");
			}

			if(CHARGER_Init(2000) != CHARGER_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "Charger service successfully initialized\r\n");

			if(ENERGY_DEBUGGER_Init(2000) != ENERGY_DEBUGGER_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "Energy debugger service successfully initialized\r\n");

			if(NETWORK_Init(2000) != NETWORK_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "Network service successfully initialized\r\n");
			if(CONTROL_Init(2000) != CONTROL_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "Control service successfully initialized\r\n");

			if(SSTREAM_Init() != SSTREAM_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "Samples Stream service successfully initialized\r\n");

			if(SSTREAM_RegisterAcquisitionStateChangeCB(prvSYSTEM_AcquisitionStateChanged) != SSTREAM_STATUS_OK)
			{
				LOGGING_Write("System", LOGGING_MSG_TYPE_ERROR, "Unable to register\r\n");

			}

			if(DPCONTROL_Init(2000) != DPCONTROL_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "Discharge Profile Control service successfully initialized\r\n");

//			if(EEZ_DIB_Init(2000) != EEZ_DIB_STATUS_OK)
//			{
//				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
//				break;
//			}
//			LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "EEZ DIB service successfully initialized\r\n");

			xSemaphoreGive(prvSYSTEM_DATA.initSig);
			prvSYSTEM_SetRGBState(prvSYSTEM_DATA.rgbValue.red, prvSYSTEM_DATA.rgbValue.blue, prvSYSTEM_DATA.rgbValue.green);
			prvSYSTEM_DATA.state = SYSTEM_STATE_SERVICE;
			break;
		case SYSTEM_STATE_SERVICE:
			/*Main application logic goes here*/
			xTaskNotifyWait(0x0, 0xffffffff, &notifyValue, portMAX_DELAY);
			if((notifyValue & SYSTEM_MASK_RGB_SET_COLOR) != 0)
			{
				prvSYSTEM_SetRGBState(prvSYSTEM_DATA.rgbValue.red, prvSYSTEM_DATA.rgbValue.blue, prvSYSTEM_DATA.rgbValue.green);
			}
			break;
		case SYSTEM_STATE_ERROR:
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(pdMS_TO_TICKS(portMAX_DELAY));
			break;
		}

	}
}


system_status_t SYSTEM_Init()
{
#if(CONF_BRINGUP_ENABLE == 0)
	prvSYSTEM_DATA.state = SYSTEM_STATE_INIT;

	if(xTaskCreate(prvSYSTEM_Task,
			SYSTEM_TASK_NAME,
			SYSTEM_TASK_STACK_SIZE,
			NULL,
			SYSTEM_TASK_PRIO,
			&prvSYSTEM_TASK_HANDLE) != pdTRUE) return SYSTEM_STATUS_ERROR;

	prvSYSTEM_DATA.initSig = xSemaphoreCreateBinary();

	if(prvSYSTEM_DATA.initSig == NULL) return SYSTEM_STATUS_ERROR;

	prvSYSTEM_DATA.guard = xSemaphoreCreateMutex();

	if(prvSYSTEM_DATA.guard == NULL) return SYSTEM_STATUS_ERROR;
#else
	BRINGUP_Init();
#endif


	return SYSTEM_STATUS_OK;
}


system_status_t SYSTEM_Start()
{
	if(osKernelInitialize() != osOK) return SYSTEM_STATUS_ERROR;
	if(osKernelStart() != osOK) return SYSTEM_STATUS_ERROR;
	/*Never ends here*/
	return SYSTEM_STATUS_ERROR;
}

system_status_t SYSTEM_ReportError(system_error_level_t errorLevel)
{
	prvSYSTEM_DATA.rgbValue.red = 50;
	prvSYSTEM_DATA.rgbValue.blue = 0;
	prvSYSTEM_DATA.rgbValue.green = 0;
	switch(errorLevel)
	{
	case SYSTEM_ERROR_LEVEL_LOW:
		prvSYSTEM_SetRGBState(prvSYSTEM_DATA.rgbValue.red, prvSYSTEM_DATA.rgbValue.blue, prvSYSTEM_DATA.rgbValue.green);
			break;
	case SYSTEM_ERROR_LEVEL_MEDIUM:
		prvSYSTEM_SetRGBState(prvSYSTEM_DATA.rgbValue.red, prvSYSTEM_DATA.rgbValue.blue, prvSYSTEM_DATA.rgbValue.green);
		break;
	case SYSTEM_ERROR_LEVEL_HIGH:
		prvSYSTEM_SetRGBState(prvSYSTEM_DATA.rgbValue.red, prvSYSTEM_DATA.rgbValue.blue, prvSYSTEM_DATA.rgbValue.green);
		break;
	}
	return SYSTEM_STATUS_OK;
}

system_status_t SYSTEM_SetLinkStatus(system_link_status_t linkStatus)
{
	system_status_t returnValue = SYSTEM_STATUS_OK;
	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	system_rgb_value_t rgb;

	prvSYSTEM_DATA.linkStatus = linkStatus;


	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;

	if(linkStatus == SYSTEM_LINK_STATUS_UP)
	{
		rgb.red = 0;
		rgb.green = SYSTEM_RGB_DEFAULT_BRIGHTNESS;
		rgb.blue = SYSTEM_RGB_DEFAULT_BRIGHTNESS;
		if(SYSTEM_SetRGB(rgb) != SYSTEM_STATUS_OK) returnValue = SYSTEM_STATUS_ERROR;
	}
	else
	{
		rgb.red = SYSTEM_RGB_DEFAULT_BRIGHTNESS;
		rgb.green = 0;
		rgb.blue = SYSTEM_RGB_DEFAULT_BRIGHTNESS;
		if(SYSTEM_SetRGB(rgb) != SYSTEM_STATUS_OK) returnValue = SYSTEM_STATUS_ERROR;
	}


	return returnValue;
}

system_status_t SYSTEM_SetDeviceName(const char* deviceName)
{
	if(deviceName == NULL) return SYSTEM_STATUS_ERROR;

	size_t len = strnlen(deviceName, CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX);

	if(len >= CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX) return SYSTEM_STATUS_ERROR;

	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	memset(prvSYSTEM_DATA.deviceName, 0, CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX);
	memcpy(prvSYSTEM_DATA.deviceName, deviceName, len);

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;

	return SYSTEM_STATUS_OK;
}

system_status_t SYSTEM_GetDeviceName(char* deviceName, uint32_t* deviceNameSize)
{
	if(deviceName == NULL || deviceNameSize == NULL) return SYSTEM_STATUS_ERROR;

	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	size_t len = strnlen(prvSYSTEM_DATA.deviceName, CONF_CONFIGURATION_MAX_PARAM_VALUESIZE);

	memcpy(deviceName, prvSYSTEM_DATA.deviceName, len);
	deviceName[len] = '\0';
	*deviceNameSize = len;

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;

	return SYSTEM_STATUS_OK;
}

system_status_t SYSTEM_SetDeviceSerial(const char* deviceSerial)
{
	if(deviceSerial == NULL) return SYSTEM_STATUS_ERROR;

	size_t len = strnlen(deviceSerial, CONF_CONFIGURATION_MAX_PARAM_VALUESIZE);

	if(len >= CONF_CONFIGURATION_MAX_PARAM_VALUESIZE) return SYSTEM_STATUS_ERROR;

	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	memset(prvSYSTEM_DATA.deviceSerial, 0, CONF_CONFIGURATION_MAX_PARAM_VALUESIZE);
	memcpy(prvSYSTEM_DATA.deviceSerial, deviceSerial, len);

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;

	return SYSTEM_STATUS_OK;
}

system_status_t SYSTEM_GetDeviceSerial(char* deviceSerial, uint32_t* deviceSerialSize)
{
	if(deviceSerial == NULL || deviceSerialSize == NULL) return SYSTEM_STATUS_ERROR;

	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	size_t len = strnlen(prvSYSTEM_DATA.deviceSerial, CONF_CONFIGURATION_MAX_PARAM_VALUESIZE);

	memcpy(deviceSerial, prvSYSTEM_DATA.deviceSerial, len);
	deviceSerial[len] = '\0';
	*deviceSerialSize = len;

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;

	return SYSTEM_STATUS_OK;
}

system_status_t SYSTEM_SetFWVersion(const char* fwVersion)
{
	if(fwVersion == NULL) return SYSTEM_STATUS_ERROR;

	size_t len = strnlen(fwVersion, CONF_CONFIGURATION_MAX_PARAM_VALUESIZE);

	if(len >= CONF_CONFIGURATION_MAX_PARAM_VALUESIZE) return SYSTEM_STATUS_ERROR;

	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	memset(prvSYSTEM_DATA.deviceFwVersion, 0, CONF_CONFIGURATION_MAX_PARAM_VALUESIZE);
	memcpy(prvSYSTEM_DATA.deviceFwVersion, fwVersion, len);

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;

	return SYSTEM_STATUS_OK;
}

system_status_t SYSTEM_GetFWVersion(char* fwVersion, uint32_t* fwVersionSize)
{
	if(fwVersion == NULL || fwVersionSize == NULL) return SYSTEM_STATUS_ERROR;

	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	size_t len = strnlen(prvSYSTEM_DATA.deviceFwVersion, CONF_CONFIGURATION_MAX_PARAM_VALUESIZE);

	memcpy(fwVersion, prvSYSTEM_DATA.deviceFwVersion, len);
	fwVersion[len] = '\0';
	*fwVersionSize = len;

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;

	return SYSTEM_STATUS_OK;
}

system_status_t SYSTEM_SetRGB(system_rgb_value_t value)
{
	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	prvSYSTEM_DATA.rgbValue.red = value.red;
	prvSYSTEM_DATA.rgbValue.blue = value.blue;
	prvSYSTEM_DATA.rgbValue.green = value.green;

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;

	if(xTaskNotify(prvSYSTEM_TASK_HANDLE, SYSTEM_MASK_RGB_SET_COLOR, eSetBits) != pdTRUE) return SYSTEM_STATUS_ERROR;

	return SYSTEM_STATUS_OK;
}

system_status_t SYSTEM_Restart(void)
{
    LOGGING_Write("System", LOGGING_MSG_TYPE_WARNING, "System restarting...\r\n");

    vTaskDelay(pdMS_TO_TICKS(100));

    NVIC_SystemReset();

    return SYSTEM_STATUS_OK;
}
/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */
#endif /* CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_C_ */
