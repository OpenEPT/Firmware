/*
 * charger.c
 *
 *  Created on: Apr 12, 2025
 *      Author: elektronika
 */
#include 	"FreeRTOS.h"
#include 	"task.h"
#include 	"queue.h"
#include 	"semphr.h"

#include 	"charger.h"
#include 	"system.h"
#include 	"logging.h"
#include 	"control.h"

#include 	"bq25150.h"


#define  	CHARGER_TASK_SET_CHARGING_STATUS				0x00000001
#define  	CHARGER_TASK_SET_CURRENT_CHARGING_VALUE			0x00000002
#define  	CHARGER_TASK_SET_CURRENT_TERMINATION_VALUE		0x00000004
#define  	CHARGER_TASK_SET_VOLTAGE_TERMINATION_VALUE		0x00000008
#define  	CHARGER_TASK_REG_READ							0x00000010
#define  	CHARGER_TASK_PROCESS_INT						0x00000020


#define		CHARGER_DEFAULT_CURRENT_TERMINATION_VALUE	5 //%
#define		CHARGER_DEFAULT_VOLTAGE_TERMINATION_VALUE	4.35 //%
#define		CHARGER_DEFAULT_CURRENT_CHARGING_VALUE		100 //mA
#define		CHARGER_DEFAULT_CURRENT_ILIM_VALUE			3 	//200 mA
#define		CHARGER_DEFAULT_CHARGING_STATE				0 	//Disable
#define		CHARGER_DEFAULT_WD_STATE					0 	//Disable


typedef struct
{
	uint8_t addr;
	uint8_t data;
}charger_reg_content_t;


typedef struct
{
	float 					terminationVoltage;
	uint8_t					terminationCurrent;
	uint16_t 				chargingCurrent;
	bq25250_ilim_value_t 	currentLimit;
	bq25150_charge_status	chargingStatus;
	bq25150_wdg_status		wdStatus;
}charger_charging_info_t;

typedef struct
{
	charger_state_t					state;
	SemaphoreHandle_t				initSig;
	SemaphoreHandle_t				guard;
	TaskHandle_t					taskHandle;
	charger_charging_info_t			chargingInfo;
	charger_reg_content_t			regContent;
	uint16_t						chargerIntStatus;
	uint8_t							adcIntStatus;
	uint8_t							timerIntStatus;
}charger_data_t;


static	charger_data_t				prvCHARGER_DATA;

static void prvCHARGER_CB()
{
	BaseType_t *pxHigherPriorityTaskWoken = pdFALSE;
	xTaskNotifyFromISR(prvCHARGER_DATA.taskHandle, CHARGER_TASK_PROCESS_INT, eSetBits, pxHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}


static void prvCHARGER_TaskFunc(void* pvParameters)
{
	uint32_t				notifyValue = 0;
	uint16_t				intMask = 0xFFFF;
	for(;;){
		switch(prvCHARGER_DATA.state)
		{
		case CHARGER_STATE_INIT:
			if(BQ25150_Init() != BQ25150_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to initialize BQ25150\r\n");
				break;
			}

			if(BQ25150_Ping(1000) != BQ25150_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to establish connection with charger\r\n");
				break;
			}

			intMask &= ~(BQ25150_MASK_CHARGE_DONE | BQ25150_MASK_BAT_UVLO_FAULT);

//			if(BQ25150_SetChargerIntMask(intMask, 1000) != BQ25150_STATUS_OK)
//			{
//				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
//				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to disable all charger interrupts \r\n");
//				break;
//			}
//
//			if(BQ25150_SetADCIntMask(0xFF, 1000) != BQ25150_STATUS_OK)
//			{
//				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
//				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to disable all ADC interrupts \r\n");
//				break;
//			}
//
//			if(BQ25150_SetTimerIntMask(0xFF, 1000) != BQ25150_STATUS_OK)
//			{
//				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
//				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to disable all Timer interrupts \r\n");
//				break;
//			}

			if(BQ25150_RegCallback(prvCHARGER_CB) != BQ25150_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to register CB\r\n");
				break;
			}

			if(BQ25150_Charge_SetStatus(prvCHARGER_DATA.chargingInfo.chargingStatus, 1000) != BQ25150_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to disable charger \r\n");
				break;
			}

			if(BQ25150_ILim_Set(prvCHARGER_DATA.chargingInfo.currentLimit, 1000) != BQ25150_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set charger input current limit\r\n");
				break;
			}

			if(BQ25150_WDG_SetStatus(prvCHARGER_DATA.chargingInfo.wdStatus, 1000) != BQ25150_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to disable WD\r\n");
				break;
			}

			if(BQ25150_Charge_RegVoltage_Set(prvCHARGER_DATA.chargingInfo.terminationVoltage, 1000) != BQ25150_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set Termination voltage\r\n");
				break;
			}

			if(BQ25150_Charge_TermCurrent_Set(prvCHARGER_DATA.chargingInfo.terminationCurrent, 1000) != BQ25150_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set Termination current\r\n");
				break;
			}
			if(BQ25150_Charge_Current_Set(prvCHARGER_DATA.chargingInfo.chargingCurrent, 1000) != BQ25150_STATUS_OK)
			{
				prvCHARGER_DATA.state	= CHARGER_STATE_ERROR;
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set charging current \r\n");
				break;
			}

			LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Charger successfully initialized \r\n");

			prvCHARGER_DATA.state	= CHARGER_STATE_SERVICE;
			xSemaphoreGive(prvCHARGER_DATA.initSig);
			break;
		case CHARGER_STATE_SERVICE:
			notifyValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
			if(notifyValue & CHARGER_TASK_SET_CHARGING_STATUS)
			{
				if(BQ25150_Charge_SetStatus(prvCHARGER_DATA.chargingInfo.chargingStatus, 1000) != BQ25150_STATUS_OK)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set charging status \r\n");
				}
				else
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Charging status successfully set\r\n");
				}
				xSemaphoreGive(prvCHARGER_DATA.initSig);
			}
			if(notifyValue & CHARGER_TASK_SET_CURRENT_CHARGING_VALUE)
			{
				if(BQ25150_Charge_Current_Set(prvCHARGER_DATA.chargingInfo.chargingCurrent, 1000) != BQ25150_STATUS_OK)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set charging current \r\n");
				}
				else
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Charging current set\r\n");
				}
				xSemaphoreGive(prvCHARGER_DATA.initSig);
			}
			if(notifyValue & CHARGER_TASK_SET_CURRENT_TERMINATION_VALUE)
			{
				if(BQ25150_Charge_TermCurrent_Set(prvCHARGER_DATA.chargingInfo.terminationCurrent, 1000) != BQ25150_STATUS_OK)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set charging termination current \r\n");
				}
				else
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Charging termination current set\r\n");
				}
				xSemaphoreGive(prvCHARGER_DATA.initSig);
			}
			if(notifyValue & CHARGER_TASK_SET_VOLTAGE_TERMINATION_VALUE)
			{
				if(BQ25150_Charge_RegVoltage_Set(prvCHARGER_DATA.chargingInfo.terminationVoltage, 1000) != BQ25150_STATUS_OK)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to set charging termination voltage \r\n");
				}
				else
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Charging termination voltage set\r\n");
				}
				xSemaphoreGive(prvCHARGER_DATA.initSig);
			}
			if(notifyValue & CHARGER_TASK_REG_READ)
			{
				if(BQ25150_ReadReg(prvCHARGER_DATA.regContent.addr, &prvCHARGER_DATA.regContent.data, 1000) != BQ25150_STATUS_OK)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_ERROR,  "Unable to read register \r\n");
				}
				else
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Register successfully read\r\n");
				}
				xSemaphoreGive(prvCHARGER_DATA.initSig);
			}
			if(notifyValue & CHARGER_TASK_PROCESS_INT)
			{
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Interrupt detected \r\n");
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Read interrupt status \r\n");
				BQ25150_GetChargerIntFlags(&prvCHARGER_DATA.chargerIntStatus, 1000);
				BQ25150_GetADCIntFlags(&prvCHARGER_DATA.adcIntStatus, 1000);
				BQ25150_GetTimerIntFlags(&prvCHARGER_DATA.timerIntStatus, 1000);

				if(prvCHARGER_DATA.chargerIntStatus & BQ25150_MASK_CHARGE_DONE)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_WARNNING,  "Charging done \r\n");
					CONTROL_StatusLinkSendMessage("charger charging done\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
					prvCHARGER_DATA.chargerIntStatus &= ~BQ25150_MASK_CHARGE_DONE;
				}
				if(prvCHARGER_DATA.chargerIntStatus & BQ25150_MASK_BAT_UVLO_FAULT)
				{
					LOGGING_Write("Charger service", LOGGING_MSG_TYPE_WARNNING,  "Under voltage detected \r\n");
					CONTROL_StatusLinkSendMessage("charger uvoltage\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
					prvCHARGER_DATA.chargerIntStatus &= ~BQ25150_MASK_CHARGE_DONE;
				}

				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Charger Int:  0x%02X \r\n", prvCHARGER_DATA.chargerIntStatus);
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "ADC Int: 		0x%02X \r\n", prvCHARGER_DATA.adcIntStatus);
				LOGGING_Write("Charger service", LOGGING_MSG_TYPE_INFO,  "Timer Int:    0x%02X \r\n", prvCHARGER_DATA.timerIntStatus);

			}
			break;
		case CHARGER_STATE_ERROR:
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(portMAX_DELAY);
			break;
		}
	}
}

charger_status_t 	CHARGER_Init(uint32_t initTimeout)
{
	if(xTaskCreate(
			prvCHARGER_TaskFunc,
			CHARGER_TASK_NAME,
			CHARGER_TASK_STACK,
			NULL,
			CHARGER_TASK_PRIO,
			&prvCHARGER_DATA.taskHandle) != pdPASS) return CHARGER_STATUS_ERROR;

	prvCHARGER_DATA.initSig = xSemaphoreCreateBinary();

	if(prvCHARGER_DATA.initSig == NULL) return CHARGER_STATUS_ERROR;

	prvCHARGER_DATA.guard = xSemaphoreCreateMutex();

	if(prvCHARGER_DATA.guard == NULL) return CHARGER_STATUS_ERROR;

	prvCHARGER_DATA.chargingInfo.chargingCurrent = CHARGER_DEFAULT_CURRENT_CHARGING_VALUE;
	prvCHARGER_DATA.chargingInfo.terminationCurrent = CHARGER_DEFAULT_CURRENT_TERMINATION_VALUE;
	prvCHARGER_DATA.chargingInfo.terminationVoltage = CHARGER_DEFAULT_VOLTAGE_TERMINATION_VALUE;
	prvCHARGER_DATA.chargingInfo.currentLimit = CHARGER_DEFAULT_CURRENT_ILIM_VALUE;
	prvCHARGER_DATA.chargingInfo.chargingStatus = CHARGER_DEFAULT_CHARGING_STATE;
	prvCHARGER_DATA.chargingInfo.wdStatus = CHARGER_DEFAULT_WD_STATE;

	prvCHARGER_DATA.state = CHARGER_STATE_INIT;

	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t	CHARGER_SetChargingState(charger_charging_state_t state, uint32_t initTimeout)
{
	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	prvCHARGER_DATA.chargingInfo.chargingStatus = state;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(xTaskNotify(prvCHARGER_DATA.taskHandle,
			CHARGER_TASK_SET_CHARGING_STATUS,
			eSetBits) != pdPASS) return CHARGER_STATUS_ERROR;


	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t	CHARGER_GetChargingState(charger_charging_state_t* state, uint32_t initTimeout)
{
	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	*state = prvCHARGER_DATA.chargingInfo.chargingStatus;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t	CHARGER_SetChargingCurrent(uint16_t current, uint32_t initTimeout)
{
	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	prvCHARGER_DATA.chargingInfo.chargingCurrent = current;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(xTaskNotify(prvCHARGER_DATA.taskHandle,
			CHARGER_TASK_SET_CURRENT_CHARGING_VALUE,
			eSetBits) != pdPASS) return CHARGER_STATUS_ERROR;


	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t	CHARGER_GetChargingCurrent(uint16_t* current, uint32_t initTimeout)
{
	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	*current = prvCHARGER_DATA.chargingInfo.chargingCurrent;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t	CHARGER_SetChargingTermCurrent(uint16_t current, uint32_t initTimeout)
{
	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	prvCHARGER_DATA.chargingInfo.terminationCurrent = current;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(xTaskNotify(prvCHARGER_DATA.taskHandle,
			CHARGER_TASK_SET_CURRENT_TERMINATION_VALUE,
			eSetBits) != pdPASS) return CHARGER_STATUS_ERROR;


	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}
charger_status_t	CHARGER_GetChargingTermCurrent(uint16_t* current, uint32_t initTimeout)
{
	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	*current = prvCHARGER_DATA.chargingInfo.terminationCurrent;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}
charger_status_t	CHARGER_SetChargingTermVoltage(float voltage, uint32_t initTimeout)
{
	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	prvCHARGER_DATA.chargingInfo.terminationVoltage = voltage;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(xTaskNotify(prvCHARGER_DATA.taskHandle,
			CHARGER_TASK_SET_VOLTAGE_TERMINATION_VALUE,
			eSetBits) != pdPASS) return CHARGER_STATUS_ERROR;


	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}
charger_status_t	CHARGER_GetChargingTermVoltage(float* voltage, uint32_t initTimeout)
{
	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	*voltage = prvCHARGER_DATA.chargingInfo.terminationVoltage;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	return CHARGER_STATUS_OK;
}

charger_status_t	CHARGER_GetRegContent(uint8_t regAddr, uint8_t* regData, uint32_t initTimeout)
{
	if(xSemaphoreTake(prvCHARGER_DATA.guard, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CHARGER_STATUS_ERROR;

	prvCHARGER_DATA.regContent.addr = regAddr;
	prvCHARGER_DATA.regContent.data = 0;

	if(xSemaphoreGive(prvCHARGER_DATA.guard) != pdTRUE) return CHARGER_STATUS_ERROR;

	if(xTaskNotify(prvCHARGER_DATA.taskHandle,
			CHARGER_TASK_REG_READ,
			eSetBits) != pdPASS) return CHARGER_STATUS_ERROR;


	if(xSemaphoreTake(prvCHARGER_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return CHARGER_STATUS_ERROR;

	*regData = prvCHARGER_DATA.regContent.data;

	return CHARGER_STATUS_OK;
}
