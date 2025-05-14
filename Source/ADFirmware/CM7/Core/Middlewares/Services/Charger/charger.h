/*
 * charger.h
 *
 *  Created on: Apr 12, 2025
 *      Author: elektronika
 */

#ifndef CORE_MIDDLEWARES_SERVICES_CHARGER_CHARGER_H_
#define CORE_MIDDLEWARES_SERVICES_CHARGER_CHARGER_H_
#include "globalConfig.h"

#define CHARGER_TASK_NAME				CONF_CHARGER_TASK_NAME
#define CHARGER_TASK_PRIO				CONF_CHARGER_PRIO
#define CHARGER_TASK_STACK				CONF_CHARGER_STACK_SIZE


typedef enum{
	CHARGER_STATUS_OK,
	CHARGER_STATUS_ERROR
}charger_status_t;

typedef enum
{
	CHARGER_STATE_INIT,
	CHARGER_STATE_SERVICE,
	CHARGER_STATE_ERROR
}charger_state_t;

typedef enum
{
	CHARGER_CHARGING_DISABLE = 0,
	CHARGER_CHARGING_ENABLE
}charger_charging_state_t;


charger_status_t 	CHARGER_Init(uint32_t initTimeout);
charger_status_t	CHARGER_SetChargingState(charger_charging_state_t state, uint32_t initTimeout);
charger_status_t	CHARGER_GetChargingState(charger_charging_state_t* state, uint32_t initTimeout);
charger_status_t	CHARGER_SetChargingCurrent(uint16_t current, uint32_t initTimeout);
charger_status_t	CHARGER_GetChargingCurrent(uint16_t* current, uint32_t initTimeout);
charger_status_t	CHARGER_SetChargingTermCurrent(uint16_t current, uint32_t initTimeout);
charger_status_t	CHARGER_GetChargingTermCurrent(uint16_t* current, uint32_t initTimeout);
charger_status_t	CHARGER_SetChargingTermVoltage(float voltage, uint32_t initTimeout);
charger_status_t	CHARGER_GetChargingTermVoltage(float* voltage, uint32_t initTimeout);
charger_status_t	CHARGER_GetRegContent(uint8_t regAddr, uint8_t* regData, uint32_t initTimeout);


#endif /* CORE_MIDDLEWARES_SERVICES_CHARGER_CHARGER_H_ */
