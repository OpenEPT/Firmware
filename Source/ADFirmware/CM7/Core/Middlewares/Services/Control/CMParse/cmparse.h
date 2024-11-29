/*
 * cmparse.h
 *
 *  Created on: Dec 28, 2023
 *      Author: Haris
 */

#ifndef CORE_MIDDLEWARES_SERVICES_CONTROL_CMPARSE_CMPARSE_H_
#define CORE_MIDDLEWARES_SERVICES_CONTROL_CMPARSE_CMPARSE_H_

#include "cmparse_util.h"
#include "stdint.h"

#define CMPARSE_MAX_COMMANDS_NO		50
#define CMPARSE_MAX_VALUE_SIZE		100
#define CMPARSE_MAX_ARG_BUFFER_SIZE	100

typedef enum{
	CMPARSE_STATUS_OK,
	CMPARSE_STATUS_ERROR
}cmparse_status_t;

typedef struct
{
	char	value[CMPARSE_MAX_VALUE_SIZE];
	uint32_t	size;
}cmparse_value_t;


cmparse_status_t		CMPARSE_Init();
cmparse_status_t		CMPARSE_AddCommand(const char* command, CommandCallBack callback);
cmparse_status_t 		CMPARSE_Execute(const char* command, char* response, uint16_t* responseSize);
char*					CMPARSE_GetArgParameters(char* argBuffer, uint32_t* argBufferSize, cmparse_value_t* key, cmparse_value_t* value);
cmparse_status_t		CMPARSE_GetArgValue(const char* argBuffer, uint32_t argBufferSize, const char* key, cmparse_value_t* value);

#endif /* CORE_MIDDLEWARES_SERVICES_CONTROL_CMPARSE_CMPARSE_H_ */
