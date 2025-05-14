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


#define	DPCONTROL_GPIO_DISABLE_PORT		CONF_DPCONTROL_GPIO_DISABLE_PORT
#define	DPCONTROL_GPIO_DISABLE_PIN		CONF_DPCONTROL_GPIO_DISABLE_PIN


#define	DPCONTROL_LATCH_PORT			CONF_DPCONTROL_LATCH_PORT
#define	DPCONTROL_LATCH_PIN				CONF_DPCONTROL_LATCH_PIN


typedef enum{
	DPCONTROL_STATUS_OK = 0,
	DPCONTROL_STATUS_ERROR
}dpcontrol_status_t;

typedef enum
{
	DPCONTROL_DAC_STATUS_DISABLE = 0,
	DPCONTROL_DAC_STATUS_ENABLE
}dpcontrol_dac_status_t;

typedef enum
{
	DPCONTROL_LOAD_STATE_DISABLE = 0,
	DPCONTROL_LOAD_STATE_ENABLE
}dpcontrol_load_state_t;

typedef enum
{
	DPCONTROL_PPATH_STATE_DISABLE = 0,
	DPCONTROL_PPATH_STATE_ENABLE
}dpcontrol_ppath_state_t;

typedef enum
{
	DPCONTROL_BAT_STATE_DISABLE = 0,
	DPCONTROL_BAT_STATE_ENABLE
}dpcontrol_bat_state_t;

typedef enum
{
	DPCONTROL_PROTECTION_STATE_DISABLE = 0,
	DPCONTROL_PROTECTION_STATE_ENABLE
}dpcontrol_protection_state_t;

typedef enum
{
	DPCONTROL_STATE_INIT,
	DPCONTROL_STATE_SERVICE,
	DPCONTROL_STATE_ERROR
}dpcontrol_state_t;

dpcontrol_status_t 	DPCONTROL_Init(uint32_t initTimeout);
dpcontrol_status_t 	DPCONTROL_SetValue(uint32_t value, uint32_t timeout);
dpcontrol_status_t 	DPCONTROL_GetValue(uint32_t* value, uint32_t timeout);
dpcontrol_status_t 	DPCONTROL_SetDACStatus(dpcontrol_dac_status_t activeStatus, uint32_t timeout);
dpcontrol_status_t 	DPCONTROL_GetDACStatus(dpcontrol_dac_status_t* activeStatus, uint32_t timeout);
dpcontrol_status_t  DPCONTROL_SetLoadState(dpcontrol_load_state_t state, uint32_t timeout);
dpcontrol_status_t  DPCONTROL_GetLoadState(dpcontrol_load_state_t* state, uint32_t timeout);
dpcontrol_status_t  DPCONTROL_SetBatState(dpcontrol_bat_state_t state, uint32_t timeout);
dpcontrol_status_t  DPCONTROL_GetBatState(dpcontrol_bat_state_t* state, uint32_t timeout);
dpcontrol_status_t  DPCONTROL_SetPPathState(dpcontrol_ppath_state_t state, uint32_t timeout);
dpcontrol_status_t  DPCONTROL_GetPPathState(dpcontrol_ppath_state_t* state, uint32_t timeout);


dpcontrol_status_t  DPCONTROL_GetUVoltageState(dpcontrol_protection_state_t* state, uint32_t timeout);
dpcontrol_status_t  DPCONTROL_GetOVoltageState(dpcontrol_protection_state_t* state, uint32_t timeout);
dpcontrol_status_t  DPCONTROL_GetOCurrentState(dpcontrol_protection_state_t* state, uint32_t timeout);


dpcontrol_status_t  DPCONTROL_LatchTriger(uint32_t timeout);
