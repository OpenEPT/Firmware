/*
 * system.c
 *
 *  Created on: Nov 5, 2023
 *      Author: Haris
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

#define  SYSTEM_MASK_RGB_SET_COLOR	0x00000001

typedef  struct
{
	system_state_t 			state;
	SemaphoreHandle_t 		initSig;
	system_link_status_t	linkStatus;
	system_rgb_value_t		rgbValue;
	SemaphoreHandle_t		guard;
	char					deviceName[CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX];
}system_data_t;

static system_data_t prvSYSTEM_DATA;


static TaskHandle_t  prvSYSTEM_TASK_HANDLE;

static void	prvBUTTON_Callback(drv_gpio_pin pin)
{

}

static system_status_t prvSYSTEM_SetRGBState(red, blue, green)
{
	if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_2, red, portMAX_DELAY) != DRV_TIMER_STATUS_OK) return SYSTEM_STATUS_OK;
	if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_3, green, portMAX_DELAY) != DRV_TIMER_STATUS_OK) return SYSTEM_STATUS_OK;
	if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_4, blue, portMAX_DELAY) != DRV_TIMER_STATUS_OK) return SYSTEM_STATUS_OK;
	return SYSTEM_STATUS_ERROR;
}

static void prvSYSTEM_AcquisitionStateChanged(uint32_t id, sstream_acquisition_state_t state)
{
//	if(state == SSTREAM_ACQUISITION_STATE_ACTIVE)
//	{
//		LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "Acquistion state changed to active\r\n");
//		EEZ_DIB_SetAcquisitionState(EEZ_DIB_ACQUISIIION_STATE_ACTIVE, id, 0);
//
//	}
//	else if(state == SSTREAM_ACQUISITION_STATE_INACTIVE)
//	{
//		LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "Acquistion state changed to inactive\r\n");
//		EEZ_DIB_SetAcquisitionState(EEZ_DIB_ACQUISIIION_STATE_INACTIVE, id, 0);
//	}
//	else
//	{
//		LOGGING_Write("System", LOGGING_MSG_TYPE_INFO, "Acquistion state changed to undefined state\r\n");
//		EEZ_DIB_SetAcquisitionState(EEZ_DIB_ACQUISIIION_STATE_UNDEF, id, 0);
//	}

}

static void prvSYSTEM_Task()
{
	drv_gpio_pin_init_conf_t 	userLedConf;
	drv_timer_channel_config_t 	pwmTimerChConfig;
	drv_timer_config_t			pwmTimerConfig;
	uint32_t					notifyValue;

	prvSYSTEM_DATA.linkStatus    = SYSTEM_LINK_STATUS_DOWN;
	prvSYSTEM_DATA.rgbValue.red = 0;
	prvSYSTEM_DATA.rgbValue.green = 0;
	prvSYSTEM_DATA.rgbValue.blue = 50;

	for(;;)
	{
		switch(prvSYSTEM_DATA.state)
		{
		case SYSTEM_STATE_INIT:
			userLedConf.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
			userLedConf.pullState = DRV_GPIO_PIN_PULL_NOPULL;
			uint32_t	state = 0x01;
			notifyValue = 0;

			if(DRV_SYSTEM_InitDrivers() != DRV_SYSTEM_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			DRV_GPIO_Port_Init(SYSTEM_LINK_STATUS_DIODE_PORT);
			DRV_GPIO_Port_Init(SYSTEM_ERROR_STATUS_DIODE_PORT);
			DRV_GPIO_Pin_Init(SYSTEM_LINK_STATUS_DIODE_PORT, SYSTEM_LINK_STATUS_DIODE_PIN, &userLedConf);
			DRV_GPIO_Pin_Init(SYSTEM_ERROR_STATUS_DIODE_PORT, SYSTEM_ERROR_STATUS_DIODE_PIN, &userLedConf);
			DRV_GPIO_Pin_EnableInt(DRV_GPIO_PORT_C, 13, 5, prvBUTTON_Callback);

			memset(prvSYSTEM_DATA.deviceName, 0, CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX);
			memcpy(prvSYSTEM_DATA.deviceName, CONF_SYSTEM_DEFAULT_DEVICE_NAME, strlen(CONF_SYSTEM_DEFAULT_DEVICE_NAME));

			pwmTimerConfig.mode 		= DRV_TIMER_COUNTER_MODE_UP;
			pwmTimerConfig.prescaler 	= 2000;
			pwmTimerConfig.preload		= DRV_TIMER_PRELOAD_DISABLE;
			pwmTimerConfig.div			= DRV_TIMER_DIV_1;
			pwmTimerConfig.period		= 256;
			if(DRV_Timer_Init_Instance(DRV_TIMER_1, &pwmTimerConfig) != DRV_TIMER_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			pwmTimerChConfig.mode = DRV_TIMER_CHANNEL_MODE_PWM1;
			if(DRV_Timer_Channel_Init(DRV_TIMER_1, DRV_TIMER_CHANNEL_2, &pwmTimerChConfig) != DRV_TIMER_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			if(DRV_Timer_Channel_Init(DRV_TIMER_1, DRV_TIMER_CHANNEL_3, &pwmTimerChConfig) != DRV_TIMER_STATUS_OK)
			{
				prvSYSTEM_DATA.state = SYSTEM_STATE_ERROR;
				break;
			}
			if(DRV_Timer_Channel_Init(DRV_TIMER_1, DRV_TIMER_CHANNEL_4, &pwmTimerChConfig) != DRV_TIMER_STATUS_OK)
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
		if(DRV_GPIO_Pin_SetState(SYSTEM_ERROR_STATUS_DIODE_PORT, SYSTEM_ERROR_STATUS_DIODE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK) return SYSTEM_STATUS_ERROR;
		break;
	case SYSTEM_ERROR_LEVEL_MEDIUM:
		prvSYSTEM_SetRGBState(prvSYSTEM_DATA.rgbValue.red, prvSYSTEM_DATA.rgbValue.blue, prvSYSTEM_DATA.rgbValue.green);
		if(DRV_GPIO_Pin_SetState(SYSTEM_ERROR_STATUS_DIODE_PORT, SYSTEM_ERROR_STATUS_DIODE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK) return SYSTEM_STATUS_ERROR;
		break;
	case SYSTEM_ERROR_LEVEL_HIGH:
		prvSYSTEM_SetRGBState(prvSYSTEM_DATA.rgbValue.red, prvSYSTEM_DATA.rgbValue.blue, prvSYSTEM_DATA.rgbValue.green);
		if(DRV_GPIO_Pin_SetState(SYSTEM_ERROR_STATUS_DIODE_PORT, SYSTEM_ERROR_STATUS_DIODE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK) return SYSTEM_STATUS_ERROR;
		break;
	}
	return SYSTEM_STATUS_OK;
}

system_status_t SYSTEM_SetLinkStatus(system_link_status_t linkStatus)
{
	system_status_t returnValue = SYSTEM_STATUS_OK;
	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	prvSYSTEM_DATA.linkStatus = linkStatus;

	if(linkStatus == SYSTEM_LINK_STATUS_UP)
	{
		if(DRV_GPIO_Pin_SetState(SYSTEM_LINK_STATUS_DIODE_PORT, SYSTEM_LINK_STATUS_DIODE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK) returnValue = SYSTEM_STATUS_ERROR;
	}
	else
	{
		if(DRV_GPIO_Pin_SetState(SYSTEM_LINK_STATUS_DIODE_PORT, SYSTEM_LINK_STATUS_DIODE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK) returnValue = SYSTEM_STATUS_ERROR;
	}

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;

	return returnValue;
}

system_status_t SYSTEM_SetDeviceName(const char* deviceName)
{
	if(strlen(deviceName) > CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX) return SYSTEM_STATE_ERROR;

	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	memset(prvSYSTEM_DATA.deviceName, 0, CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX);
	memcpy(prvSYSTEM_DATA.deviceName, deviceName, strlen(deviceName));

	if(xSemaphoreGive(prvSYSTEM_DATA.guard) != pdTRUE) return SYSTEM_STATUS_ERROR;
	return SYSTEM_STATUS_OK;
}

system_status_t SYSTEM_GetDeviceName(char* deviceName, uint32_t* deviceNameSize)
{
	if(xSemaphoreTake(prvSYSTEM_DATA.guard, portMAX_DELAY) != pdTRUE) return SYSTEM_STATUS_ERROR;

	memcpy(deviceName, prvSYSTEM_DATA.deviceName, strlen(prvSYSTEM_DATA.deviceName));
	*deviceNameSize = strlen(prvSYSTEM_DATA.deviceName);

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

#endif /* CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_C_ */
