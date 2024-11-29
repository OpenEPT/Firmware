/*
 * cmparse_util.h
 *
 *  Created on: Dec 28, 2023
 *      Author: Haris
 */

#ifndef CORE_MIDDLEWARES_SERVICES_CONTROL_CMPARSE_CMPARSE_UTIL_H_
#define CORE_MIDDLEWARES_SERVICES_CONTROL_CMPARSE_CMPARSE_UTIL_H_

#include "stdint.h"

#define CMPARSE_MAX_COMMAND_NAME_LENGTH	50

typedef void (*CommandCallBack)(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize);

typedef struct
{
	char command[CMPARSE_MAX_COMMAND_NAME_LENGTH];
	uint32_t commandLength;
	CommandCallBack callback;
}cmparse_command_t;

#endif /* CORE_MIDDLEWARES_SERVICES_CONTROL_CMPARSE_CMPARSE_UTIL_H_ */
