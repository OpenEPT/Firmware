/**
 ******************************************************************************
 * @file   	logging.h
 * @brief  	...
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_LOGGING_LOGGING_H_
#define CORE_MIDDLEWARES_SERVICES_LOGGING_LOGGING_H_

#include "globalConfig.h"

#define LOGGING_INTERNAL_BUFFER_SIZE	300
#define LOGGING_QUEUE_LENGTH			30
#define	LOGGING_MAX_NUMBER_OF_CHANNELS	1
#define	LOGGING_TRANSMIT_TIMOEUT		1000

#define LOGGING_TASK_NAME				CONFIG_LOGGING_TASK_NAME
#define LOGGING_TASK_PRIO				CONFIG_LOGGING_PRIO
#define LOGGING_TASK_STACK				CONFIG_LOGGING_STACK_SIZE


typedef enum{
	LOGGING_STATUS_OK,
	LOGGING_STATUS_ERROR
}logging_status_t;

typedef enum{
	LOGGING_MSG_TYPE_INFO,
	LOGGING_MSG_TYPE_WARNNING,
	LOGGING_MSG_TYPE_ERROR
}logging_msg_type_t;

typedef enum
{
	LOGGING_STATE_INIT,
	LOGGING_STATE_SERVICE,
	LOGGING_STATE_ERROR
}logging_state_t;

typedef enum
{
	LOGGING_INITIALIZATION_STATUS_NOINIT	=	0,
	LOGGING_INITIALIZATION_STATUS_INIT		=	1
}logging_initialization_status_t;


logging_status_t 	LOGGING_Init(uint32_t initTimeout);
logging_status_t 	LOGGING_Write(char* serviceName, logging_msg_type_t msgType, char* message, ...);

#endif /* CORE_MIDDLEWARES_SERVICES_LOGGING_LOGGING_H_ */
