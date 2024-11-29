/*
 * dpcontrol.h
 *
 *  Created on: Nov 5, 2023
 *      Author: Haris
 */
#include "globalConfig.h"

#define DPCONTROL_TASK_NAME				CONF_DPCONTROL_TASK_NAME
#define DPCONTROL_TASK_PRIO				CONF_DPCONTROL_TASK_PRIO
#define DPCONTROL_TASK_STACK			CONF_DPCONTROL_TASK_STACK_SIZE


#define	DPCONTROL_LOAD_DISABLE_PORT		CONF_DPCONTROL_LOAD_DISABLE_PORT
#define	DPCONTROL_LOAD_DISABLE_PIN		CONF_DPCONTROL_LOAD_DISABLE_PIN

#define	DPCONTROL_BAT_DISABLE_PORT		CONF_DPCONTROL_BAT_DISABLE_PORT
#define	DPCONTROL_BAT_DISABLE_PIN		CONF_DPCONTROL_BAT_DISABLE_PIN


typedef enum{
	DPCONTROL_STATUS_OK = 0,
	DPCONTROL_STATUS_ERROR
}dpcontrol_status_t;

typedef enum
{
	DPCONTROL_ACTIVE_STATUS_DISABLE = 0,
	DPCONTROL_ACTIVE_STATUS_ENABLE
}dpcontrol_active_status_t;

typedef enum
{
	DPCONTROL_LOAD_STATE_DISABLE = 0,
	DPCONTROL_LOAD_STATE_ENABLE
}dpcontrol_load_state_t;

typedef enum
{
	DPCONTROL_BAT_STATE_DISABLE = 0,
	DPCONTROL_BAT_STATE_ENABLE
}dpcontrol_bat_state_t;

typedef enum
{
	DPCONTROL_STATE_INIT,
	DPCONTROL_STATE_SERVICE,
	DPCONTROL_STATE_ERROR
}dpcontrol_state_t;

dpcontrol_status_t 	DPCONTROL_Init(uint32_t initTimeout);
dpcontrol_status_t 	DPCONTROL_SetValue(uint32_t value, uint32_t timeout);
dpcontrol_status_t 	DPCONTROL_SetActivestatus(dpcontrol_active_status_t activeStatus, uint32_t timeout);
dpcontrol_status_t  DPCONTROL_SetLoadState(dpcontrol_load_state_t state, uint32_t timeout);
dpcontrol_status_t  DPCONTROL_SetBatState(dpcontrol_bat_state_t state, uint32_t timeout);
