/**
 ******************************************************************************
 * @file   	sstream.c
 *
 * @brief
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	Februar 2024
 ******************************************************************************
 */
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "logging.h"
#include "system.h"

#include "sstream.h"

#include "drv_ain.h"


#define  	SSTREAM_TASK_START_BIT					0x00000001
#define  	SSTREAM_TASK_STOP_BIT					0x00000002
#define  	SSTREAM_TASK_STREAM_BIT					0x00000004
#define  	SSTREAM_TASK_SET_ADC_RESOLUTION_BIT		0x00000008
#define  	SSTREAM_TASK_SET_ADC_STIME_BIT			0x00000010
#define  	SSTREAM_TASK_SET_ADC_CLOCK_DIV_BIT		0x00000020
#define  	SSTREAM_TASK_SET_ADC_AVERAGING_RATIO	0x00000040



typedef struct
{
	TaskHandle_t				taskHandle;
	SemaphoreHandle_t			initSig;
	SemaphoreHandle_t			guard;
	sstream_state_t				state;
	sstream_connection_info		connectionInfo;
	sstream_acquisition_state_t	acquisitionState;
	drv_ain_adc_config_t		ainConfig;
}channel_data_t;

typedef struct
{
	channel_data_t				connections[CONF_SSTREAM_CONNECTIONS_MAX_NO];
	uint32_t					activeConnectionsNo;
}sstream_data_t;


static sstream_data_t								prvSSTREAM_DATA;

static void prvSSTREAM_TaskFunc(void* pvParam)
{
	uint32_t				notifyValue = 0;
	TickType_t				blockingTime = portMAX_DELAY;
	channel_data_t		*connectionData;
	connectionData = (channel_data_t*)pvParam;
	for(;;)
	{
		switch(connectionData->state)
		{
		case SSTREAM_STATE_INIT:
			if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem with init semaphore\r\n");
				connectionData->state = SSTREAM_STATE_ERROR;
				break;
			}
			connectionData->state = SSTREAM_STATE_SERVICE;
			break;
		case SSTREAM_STATE_SERVICE:
			notifyValue = ulTaskNotifyTake(pdTRUE, blockingTime);
			if(notifyValue & SSTREAM_TASK_START_BIT)
			{
				connectionData->acquisitionState = SSTREAM_ACQUISITION_STATE_START;
			}
			if(notifyValue & SSTREAM_TASK_STREAM_BIT)
			{
				connectionData->acquisitionState = SSTREAM_ACQUISITION_STATE_STREAM;
			}
			if(notifyValue & SSTREAM_TASK_STOP_BIT)
			{
				connectionData->acquisitionState = SSTREAM_ACQUISITION_STATE_STOP;
			}
			break;
		case SSTREAM_STATE_ERROR:
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(portMAX_DELAY);
			break;
		}
	}
}

sstream_status_t			SSTREAM_Init(void)
{
	memset(&prvSSTREAM_DATA, 0, sizeof(sstream_data_t));
	return SSTREAM_STATUS_OK;
}
sstream_status_t			SSTREAM_CreateChannel(sstream_connection_info* connectionHandler, uint32_t timeout)
{
	if(prvSSTREAM_DATA.activeConnectionsNo > SSTREAM_CONNECTIONS_MAX_NO) return SSTREAM_STATUS_ERROR;
	uint32_t currentId = prvSSTREAM_DATA.activeConnectionsNo;
	memcpy(&prvSSTREAM_DATA.connections[currentId].connectionInfo, connectionHandler,  sizeof(sstream_connection_info));
	if(xTaskCreate(
			prvSSTREAM_TaskFunc,
			SSTREAM_TASK_NAME,
			SSTREAM_TASK_STACK_SIZE,
			&prvSSTREAM_DATA.connections[currentId],
			SSTREAM_TASK_PRIO,
			&prvSSTREAM_DATA.connections[currentId].taskHandle) != pdPASS) return SSTREAM_STATUS_ERROR;

	prvSSTREAM_DATA.connections[currentId].initSig = xSemaphoreCreateBinary();
	if(prvSSTREAM_DATA.connections[currentId].initSig == NULL) return SSTREAM_STATUS_ERROR;
	prvSSTREAM_DATA.connections[currentId].guard = xSemaphoreCreateMutex();
	if(prvSSTREAM_DATA.connections[currentId].guard == NULL) return SSTREAM_STATUS_ERROR;
	if(xSemaphoreTake(prvSSTREAM_DATA.connections[currentId].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	connectionHandler->id = currentId;
	prvSSTREAM_DATA.activeConnectionsNo += 1;
	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_Start(sstream_connection_info* connectionHandler)
{
	if(xTaskNotify(prvSSTREAM_DATA.connections[connectionHandler->id].taskHandle,
			SSTREAM_TASK_START_BIT | SSTREAM_TASK_STREAM_BIT,
			eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;
	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_Stop(sstream_connection_info* connectionHandler)
{
	if(xTaskNotify(prvSSTREAM_DATA.connections[connectionHandler->id].taskHandle,
			SSTREAM_TASK_STOP_BIT,
			eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;
	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_SetResolution(sstream_connection_info* connectionHandler, sstream_adc_resolution_t resolution, uint32_t timeout)
{
	if(xSemaphoreTake(prvSSTREAM_DATA.connections[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	prvSSTREAM_DATA.connections[connectionHandler->id].ainConfig.resolution = resolution;
	if(xSemaphoreGive(prvSSTREAM_DATA.connections[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;
	if(xTaskNotify(prvSSTREAM_DATA.connections[connectionHandler->id].taskHandle,
			SSTREAM_TASK_SET_ADC_RESOLUTION_BIT,
			eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;
	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_SetChannelSamplingTime(sstream_connection_info* connectionHandler, uint32_t channel, sstream_adc_sampling_time_t stime, uint32_t timeout)
{
	if(xSemaphoreTake(prvSSTREAM_DATA.connections[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	if(channel == 1)
	{
		prvSSTREAM_DATA.connections[connectionHandler->id].ainConfig.ch1.sampleTime = stime;
	}
	if(channel == 2)
	{
		prvSSTREAM_DATA.connections[connectionHandler->id].ainConfig.ch2.sampleTime = stime;
	}
	if(xSemaphoreGive(prvSSTREAM_DATA.connections[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;
	if(xTaskNotify(prvSSTREAM_DATA.connections[connectionHandler->id].taskHandle,
			SSTREAM_TASK_SET_ADC_STIME_BIT,
			eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;
	return SSTREAM_STATUS_OK;
}
sstream_adc_resolution_t		SSTREAM_GetResolution(sstream_connection_info* connectionHandler, uint32_t timeout)
{
	sstream_adc_resolution_t resolution;
	if(xSemaphoreTake(prvSSTREAM_DATA.connections[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	resolution = prvSSTREAM_DATA.connections[connectionHandler->id].ainConfig.resolution;
	if(xSemaphoreGive(prvSSTREAM_DATA.connections[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;
	return resolution;
}
sstream_adc_sampling_time_t		SSTREAM_GetChannelSamplingSpeed(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t timeout)
{
	sstream_adc_sampling_time_t stime;
	if(xSemaphoreTake(prvSSTREAM_DATA.connections[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	if(channel == 1)
	{
		stime = prvSSTREAM_DATA.connections[connectionHandler->id].ainConfig.ch1.sampleTime;
	}
	if(channel == 2)
	{
		stime = prvSSTREAM_DATA.connections[connectionHandler->id].ainConfig.ch2.sampleTime;
	}
	if(xSemaphoreGive(prvSSTREAM_DATA.connections[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;
	return stime;
}


