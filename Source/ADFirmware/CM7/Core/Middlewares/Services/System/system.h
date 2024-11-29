/**
 ******************************************************************************
 * @file   	system.h
 * @brief  	...
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_H_
#define CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_H_

#include "globalConfig.h"

#define SYSTEM_TASK_NAME				CONF_SYSTEM_TASK_NAME
#define SYSTEM_TASK_PRIO				CONF_SYSTEM_TASK_PRIO
#define SYSTEM_TASK_STACK_SIZE			CONF_SYSTEM_TASK_STACK_SIZE

#define	SYSTEM_ERROR_STATUS_DIODE_PORT	CONF_SYSTEM_ERROR_STATUS_DIODE_PORT
#define	SYSTEM_ERROR_STATUS_DIODE_PIN	CONF_SYSTEM_ERROR_STATUS_DIODE_PIN

#define	SYSTEM_LINK_STATUS_DIODE_PORT	CONF_SYSTEM_LINK_STATUS_DIODE_PORT
#define	SYSTEM_LINK_STATUS_DIODE_PIN	CONF_SYSTEM_LINK_STATUS_DIODE_PIN

typedef enum
{
	SYSTEM_STATE_INIT,
	SYSTEM_STATE_SERVICE,
	SYSTEM_STATE_ERROR
}system_state_t;

typedef enum
{
	SYSTEM_STATUS_OK,
	SYSTEM_STATUS_ERROR
}system_status_t;

typedef enum
{
	SYSTEM_ERROR_LEVEL_LOW,
	SYSTEM_ERROR_LEVEL_MEDIUM,
	SYSTEM_ERROR_LEVEL_HIGH
}system_error_level_t;


typedef enum
{
	SYSTEM_LINK_STATUS_UP,
	SYSTEM_LINK_STATUS_DOWN
}system_link_status_t;

typedef struct
{
	uint8_t red;
	uint8_t green;
	uint8_t blue;
}system_rgb_value_t;


system_status_t SYSTEM_Init();
system_status_t SYSTEM_Start();
system_status_t SYSTEM_ReportError(system_error_level_t errorLevel);
system_status_t SYSTEM_SetLinkStatus(system_link_status_t linkStatus);
system_status_t SYSTEM_SetDeviceName(const char* deviceName);
system_status_t SYSTEM_GetDeviceName(char* deviceName, uint32_t* deviceNameSize);
system_status_t SYSTEM_SetRGB(system_rgb_value_t value);

#endif /* CORE_MIDDLEWARES_SERVICES_SYSTEM_SYSTEM_H_ */
