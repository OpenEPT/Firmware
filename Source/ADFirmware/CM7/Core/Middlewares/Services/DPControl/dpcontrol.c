/*
 * dpcontrol.c
 *
 *  Created on: Nov 5, 2023
 *      Author: Haris
 */
#include 	<stdarg.h>
#include 	<string.h>

#include 	"FreeRTOS.h"
#include 	"task.h"
#include 	"semphr.h"


#include	"dpcontrol.h"
#include	"logging.h"
#include 	"system.h"
#include 	"drv_aout.h"
#include 	"drv_gpio.h"

#define 	DPCONTROL_MASK_SET_VALUE			0x00000001
#define 	DPCONTROL_MASK_SET_ACTIVE_STATUS	0x00000002
#define 	DPCONTROL_MASK_SET_LOAD_STATE		0x00000004
#define 	DPCONTROL_MASK_SET_BAT_STATE		0x00000008

typedef struct
{
	uint32_t					data;
	dpcontrol_active_status_t	active;
}dpcontrol_aout_data_t;

typedef struct
{
	dpcontrol_state_t				state;
	QueueHandle_t					txMsgQueue;
	SemaphoreHandle_t				initSig;
	SemaphoreHandle_t				guard;
	TaskHandle_t					taskHandle;
	dpcontrol_aout_data_t			aoutData;
	dpcontrol_load_state_t			loadState;
	dpcontrol_bat_state_t			batState;
}dpcontrol_data_t;

static	dpcontrol_data_t				prvDPCONTROL_DATA;

static void prvDPCONTROL_TaskFunc(void* pvParameters){
	uint32_t	value;
	uint32_t	aoutValue;
	dpcontrol_active_status_t activeStatus;
	dpcontrol_load_state_t		loadState;
	dpcontrol_bat_state_t		batState;
	drv_gpio_pin_init_conf_t 	loadPinConfig;
	drv_gpio_pin_init_conf_t 	batPinConfig;

	for(;;){
		switch(prvDPCONTROL_DATA.state)
		{
		case DPCONTROL_STATE_INIT:
			loadPinConfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
			loadPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

			if(DRV_GPIO_Port_Init(DPCONTROL_LOAD_DISABLE_PORT) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize load control port\r\n");
			}
			if(DRV_GPIO_Pin_Init(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, &loadPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize load control pin\r\n");
			}

			if(DRV_GPIO_Port_Init(DPCONTROL_BAT_DISABLE_PORT) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize battery control port\r\n");
			}
			if(DRV_GPIO_Pin_Init(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, &loadPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize battery control pin\r\n");
			}


			LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Discharge Profile Control service successfully initialized\r\n");
			prvDPCONTROL_DATA.state	= DPCONTROL_STATE_SERVICE;
			xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			break;
		case DPCONTROL_STATE_SERVICE:
			xTaskNotifyWait(0x0, 0xFFFFFFFF, &value, portMAX_DELAY);
			if(value & DPCONTROL_MASK_SET_VALUE)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				aoutValue = prvDPCONTROL_DATA.aoutData.data;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				if(DRV_AOUT_SetValue(aoutValue) != DRV_AOUT_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to set DAC value\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "DAC value %d successfully set\r\n", aoutValue);
					xSemaphoreGive(prvDPCONTROL_DATA.initSig);
				}
			}
			if(value & DPCONTROL_MASK_SET_ACTIVE_STATUS)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				activeStatus = prvDPCONTROL_DATA.aoutData.active;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				if(DRV_AOUT_SetEnable(activeStatus) != DRV_AOUT_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to set active status\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Active status (%d) successfully set\r\n", activeStatus);
					xSemaphoreGive(prvDPCONTROL_DATA.initSig);
				}
			}
			if(value & DPCONTROL_MASK_SET_LOAD_STATE)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				loadState = prvDPCONTROL_DATA.loadState;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				switch(loadState)
				{
				case DPCONTROL_LOAD_STATE_DISABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to disable load\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Load successfully disabled\r\n");
					}
					break;
				case DPCONTROL_LOAD_STATE_ENABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to enable load\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Load successfully enabled\r\n");
					}
					break;
				}
				xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			}
			if(value & DPCONTROL_MASK_SET_BAT_STATE)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				batState = prvDPCONTROL_DATA.batState;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				switch(batState)
				{
				case DPCONTROL_BAT_STATE_DISABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to disable battery\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Battery successfully disabled\r\n");
					}
					break;
				case DPCONTROL_BAT_STATE_ENABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to enable battery\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Battery successfully enabled\r\n");
					}
					break;
				}
				xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			}
			break;
		case DPCONTROL_STATE_ERROR:
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(portMAX_DELAY);
			break;
		}
	}
}

dpcontrol_status_t DPCONTROL_Init(uint32_t initTimeout)
{
	memset(&prvDPCONTROL_DATA, 0, sizeof(dpcontrol_data_t));

	if(xTaskCreate(
			prvDPCONTROL_TaskFunc,
			DPCONTROL_TASK_NAME,
			DPCONTROL_TASK_STACK,
			NULL,
			DPCONTROL_TASK_PRIO,
			&prvDPCONTROL_DATA.taskHandle) != pdPASS) return DPCONTROL_STATUS_ERROR;

	prvDPCONTROL_DATA.initSig = xSemaphoreCreateBinary();

	if(prvDPCONTROL_DATA.initSig == NULL) return DPCONTROL_STATUS_ERROR;

	prvDPCONTROL_DATA.guard = xSemaphoreCreateMutex();

	if(prvDPCONTROL_DATA.guard == NULL) return DPCONTROL_STATUS_ERROR;

	prvDPCONTROL_DATA.state = DPCONTROL_STATE_INIT;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}


dpcontrol_status_t 	DPCONTROL_SetValue(uint32_t value, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.aoutData.data = value;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_VALUE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;
	return DPCONTROL_STATUS_OK;
}

dpcontrol_status_t 	DPCONTROL_SetActivestatus(dpcontrol_active_status_t activeStatus, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.aoutData.active = activeStatus;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_ACTIVE_STATUS, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;
	return DPCONTROL_STATUS_OK;

}

dpcontrol_status_t  DPCONTROL_SetLoadState(dpcontrol_load_state_t state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.loadState = state;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_LOAD_STATE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}

dpcontrol_status_t  DPCONTROL_SetBatState(dpcontrol_bat_state_t state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.batState = state;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_BAT_STATE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}

