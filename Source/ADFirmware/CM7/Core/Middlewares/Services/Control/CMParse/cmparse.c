/*
 * cmparse.c
 *
 *  Created on: Dec 28, 2023
 *      Author: Haris
 */
#include "cmparse_util.h"
#include "cmparse.h"

#include "string.h"

static cmparse_command_t 		prvCMPARSE_COMMANDS[CMPARSE_MAX_COMMANDS_NO];
static uint16_t					prvCMPARSE_REGISTERED_COMMANDS_NO;
static CommandCallBack			prvCMPARSE_DEFAULT_CALLBACK;

static cmparse_command_type_t prvCMPARSE_GetCommandType(const char* command)
{
	const char* tmpCmndPtr = command;

	if(*tmpCmndPtr == 0xA5 && *(tmpCmndPtr+1)== 0xA5)
	{
		//Message mark detected
		if(*(tmpCmndPtr+2) == 'B') return CMPARSE_COMMAND_TYPE_BIN;
		if(*(tmpCmndPtr+2) == 'H') return CMPARSE_COMMAND_TYPE_ASCII;
	}

	return CMPARSE_COMMAND_TYPE_UKNOWN;
}


static CommandCallBack prvCMPARSE_FindCallback(const char* command)
{
	uint16_t commandIterator = 0;
	while(commandIterator < prvCMPARSE_REGISTERED_COMMANDS_NO)
	{
		if(strncmp(
				prvCMPARSE_COMMANDS[commandIterator].command,
				command,
				prvCMPARSE_COMMANDS[commandIterator].commandLength) == 0)
		{
			return prvCMPARSE_COMMANDS[commandIterator].callback;
		}
		commandIterator += 1;
	}

	return NULL;

}

static cmparse_status_t prvCMPARSE_GetArguments(const char* command, char* argBuffer, uint16_t* argSize)
{

	uint16_t commandIterator = 0;
	char*	tmp = 0;
	char*	tmpArgBufferPtr = argBuffer;
	uint16_t 	argBufferMaxSize = *argSize;
	uint16_t 	argBufferTmpSize = 0;
	uint16_t	commandLength = strlen(command);
	uint16_t	processedCommandCharNumber = 0;

	while(commandIterator < prvCMPARSE_REGISTERED_COMMANDS_NO)
	{
		processedCommandCharNumber =  0;
		commandIterator += 1;
		tmp = strstr(command, prvCMPARSE_COMMANDS[commandIterator].command);
		if(tmp == NULL)continue;
		processedCommandCharNumber += prvCMPARSE_COMMANDS[commandIterator].commandLength;
		tmp += processedCommandCharNumber;
		//Skip whitespace
		while(*tmp == ' '){
			processedCommandCharNumber += 1;
			tmp ++;
		}

		while((*tmp != 0) && (processedCommandCharNumber  < commandLength) && (*tmp != '\r' && *tmp != '\n') && (argBufferTmpSize < argBufferMaxSize))
		{
			*tmpArgBufferPtr = *tmp;

			tmpArgBufferPtr ++;
			tmp ++;
			argBufferTmpSize += 1;
			processedCommandCharNumber += 1;
		}
		*argSize = argBufferTmpSize;
		return CMPARSE_STATUS_OK;

	}
	return CMPARSE_STATUS_ERROR;
}
static cmparse_status_t prvCMPARSE_GetArgumentsBin(const char* command, char* argBuffer, uint16_t* argSize, uint32_t payloadSize)
{

	uint16_t commandIterator = 0;
	char*	tmp = 0;
	char*	tmpArgBufferPtr = argBuffer;
	uint16_t 	argBufferMaxSize = *argSize;
	uint16_t 	argBufferTmpSize = 0;
	uint16_t	processedCommandCharNumber = 0;

	while(commandIterator < prvCMPARSE_REGISTERED_COMMANDS_NO)
	{
		processedCommandCharNumber =  0;
		commandIterator += 1;
		tmp = strstr(command, prvCMPARSE_COMMANDS[commandIterator].command);
		if(tmp == NULL)continue;
		processedCommandCharNumber += prvCMPARSE_COMMANDS[commandIterator].commandLength;
		tmp += processedCommandCharNumber;
		//Skip whitespace
		while(*tmp == ' '){
			processedCommandCharNumber += 1;
			tmp ++;
		}
		//Copy everything
		while((processedCommandCharNumber < payloadSize) && (argBufferTmpSize < argBufferMaxSize))
		{
			*tmpArgBufferPtr = *tmp;

			tmpArgBufferPtr ++;
			tmp ++;
			argBufferTmpSize += 1;
			processedCommandCharNumber += 1;
		}
		//Check integrity
		if(*(tmpArgBufferPtr - 1) != '\n' || *(tmpArgBufferPtr - 2) != '\r') return CMPARSE_STATUS_ERROR;
		*argSize = argBufferTmpSize;
		return CMPARSE_STATUS_OK;

	}
	return CMPARSE_STATUS_ERROR;
}

cmparse_status_t	CMPARSE_Init()
{
	memset(prvCMPARSE_COMMANDS, 0, CMPARSE_MAX_COMMANDS_NO*sizeof(cmparse_command_t));
	prvCMPARSE_REGISTERED_COMMANDS_NO = 0;
	prvCMPARSE_DEFAULT_CALLBACK = 0;
	return CMPARSE_STATUS_OK;
}

cmparse_status_t	CMPARSE_AddCommand(const char* command, CommandCallBack callback)
{
	if(prvCMPARSE_REGISTERED_COMMANDS_NO == CMPARSE_MAX_COMMANDS_NO) return CMPARSE_STATUS_ERROR;
	if(strlen(command) > CMPARSE_MAX_COMMAND_NAME_LENGTH) return CMPARSE_STATUS_ERROR;
	if(strlen(command) == 0)
	{
		prvCMPARSE_DEFAULT_CALLBACK = callback;
		return CMPARSE_STATUS_OK;
	}

	prvCMPARSE_COMMANDS[prvCMPARSE_REGISTERED_COMMANDS_NO].commandLength = strlen(command);
	memcpy(prvCMPARSE_COMMANDS[prvCMPARSE_REGISTERED_COMMANDS_NO].command,
			command,
			prvCMPARSE_COMMANDS[prvCMPARSE_REGISTERED_COMMANDS_NO].commandLength);

	prvCMPARSE_COMMANDS[prvCMPARSE_REGISTERED_COMMANDS_NO].callback = callback;

	prvCMPARSE_REGISTERED_COMMANDS_NO += 1;
	return CMPARSE_STATUS_OK;
}

cmparse_status_t 	CMPARSE_Execute(const char* command, char* response, uint16_t* responseSize)
{
	cmparse_command_type_t commandType;
	uint16_t payloadSize = 0;
	CommandCallBack commandCallBack = NULL;
	char arguments[CMPARSE_MAX_ARG_BUFFER_SIZE];
	memset(arguments, 0, CMPARSE_MAX_ARG_BUFFER_SIZE);
	uint16_t argumentsBufferSize = CMPARSE_MAX_ARG_BUFFER_SIZE;

	commandType = prvCMPARSE_GetCommandType(command);

	if(commandType == CMPARSE_COMMAND_TYPE_UKNOWN) return CMPARSE_STATUS_ERROR;

	if(commandType == CMPARSE_COMMAND_TYPE_ASCII)
	{
		commandCallBack = prvCMPARSE_FindCallback(command + 3);
	}

	if(commandType == CMPARSE_COMMAND_TYPE_BIN)
	{
		commandCallBack = prvCMPARSE_FindCallback(command + 5);
		payloadSize = *((uint16_t*)(command + 3));
	}

	if(commandCallBack == NULL){
		if(prvCMPARSE_DEFAULT_CALLBACK == NULL) return CMPARSE_STATUS_ERROR;
		prvCMPARSE_DEFAULT_CALLBACK(NULL, 0, response, responseSize);
	}
	else
	{
		if(commandType == CMPARSE_COMMAND_TYPE_ASCII)
		{
			if(prvCMPARSE_GetArguments(command + 3, arguments, &argumentsBufferSize) != CMPARSE_STATUS_OK) return CMPARSE_STATUS_ERROR;
		}
		if(commandType == CMPARSE_COMMAND_TYPE_BIN)
		{
			if(prvCMPARSE_GetArgumentsBin(command + 5, arguments, &argumentsBufferSize, payloadSize) != CMPARSE_STATUS_OK) return CMPARSE_STATUS_ERROR;
		}

		commandCallBack(arguments, argumentsBufferSize, response, responseSize);
	}



	return CMPARSE_STATUS_OK;
}

char*					CMPARSE_GetArgParameters(char* argBuffer, uint32_t* argBufferSize, cmparse_value_t* key, cmparse_value_t* value)
{
	char* 		tmpArgBuffPtr = argBuffer;
	uint32_t	tmpArgBufferSizeProcessed = 0;
	if(*argBufferSize == 0) return NULL;
	key->size	= 0;
	value->size = 0;
	//arguments are in format -key=value
	while(*tmpArgBuffPtr != '-'){
		tmpArgBufferSizeProcessed += 1;
		tmpArgBuffPtr += 1;
		if(tmpArgBufferSizeProcessed == *argBufferSize) return NULL;
	}
	tmpArgBufferSizeProcessed += 1;
	tmpArgBuffPtr += 1;
	while(*tmpArgBuffPtr != '=')
	{
		key->value[key->size] = *tmpArgBuffPtr;
		key->size += 1;
		tmpArgBufferSizeProcessed += 1;
		tmpArgBuffPtr += 1;
		if(tmpArgBufferSizeProcessed == *argBufferSize) return NULL;
	}
	tmpArgBufferSizeProcessed += 1;
	tmpArgBuffPtr += 1;
	while(*tmpArgBuffPtr != ' ' && tmpArgBufferSizeProcessed < *argBufferSize)
	{
		value->value[value->size] = *tmpArgBuffPtr;
		value->size += 1;
		tmpArgBufferSizeProcessed += 1;
		tmpArgBuffPtr += 1;
	}
	return tmpArgBuffPtr;

}

char*					CMPARSE_GetArgParametersBin(char* argBuffer, uint32_t* argBufferSize, cmparse_value_t* key, cmparse_value_t* value, uint16_t argSize, uint8_t** argValue)
{
	char* 		tmpArgBuffPtr = argBuffer;
	uint32_t	tmpArgBufferSizeProcessed = 0;
	if(*argBufferSize == 0) return NULL;
	key->size	= 0;
	value->size = 0;
	//arguments are in format -key=value
	while(*tmpArgBuffPtr != '-'){
		tmpArgBufferSizeProcessed += 1;
		tmpArgBuffPtr += 1;
		if(tmpArgBufferSizeProcessed == *argBufferSize) return NULL;
	}
	tmpArgBufferSizeProcessed += 1;
	tmpArgBuffPtr += 1;
	while(*tmpArgBuffPtr != '=')
	{
		key->value[key->size] = *tmpArgBuffPtr;
		key->size += 1;
		tmpArgBufferSizeProcessed += 1;
		tmpArgBuffPtr += 1;
		if(tmpArgBufferSizeProcessed == *argBufferSize) return NULL;
	}
	tmpArgBufferSizeProcessed += 1;
	tmpArgBuffPtr += 1;
	*argValue = tmpArgBuffPtr;
	while(*tmpArgBuffPtr != ' ' && tmpArgBufferSizeProcessed < *argBufferSize && value->size < argSize)
	{
		value->size += 1;
		tmpArgBufferSizeProcessed += 1;
		tmpArgBuffPtr += 1;
	}
	return tmpArgBuffPtr;

}




cmparse_status_t		CMPARSE_GetArgValue(const char* argBuffer, uint32_t argBufferSize, const char* key, cmparse_value_t* value)
{
	cmparse_value_t localKeyValue;
	cmparse_value_t localValue;
	uint32_t				unprocessedArgBufferSize 	= 	argBufferSize;
	char*				unprocessedArgBufferPtr			=	argBuffer;

	do{
		unprocessedArgBufferPtr = CMPARSE_GetArgParameters(
				unprocessedArgBufferPtr,
				&unprocessedArgBufferSize,
				&localKeyValue,
				&localValue
		);
		if(unprocessedArgBufferPtr == NULL) return CMPARSE_STATUS_ERROR;
		if(strncmp(key, localKeyValue.value, strlen(key)) == 0)
		{
			value->size = localValue.size;
			memcpy(value->value, localValue.value, value->size);
			return CMPARSE_STATUS_OK;
		}
	}while(unprocessedArgBufferPtr != NULL);

	return CMPARSE_STATUS_ERROR;
}

cmparse_status_t		CMPARSE_GetArgValueBin(const char* argBuffer, uint32_t argBufferSize, const char* key, cmparse_value_bin_t* value, uint32_t size)
{
	cmparse_value_t localKeyValue;
	cmparse_value_t localValue;
	uint8_t*        argValue;
	uint32_t		unprocessedArgBufferSize 	= 	argBufferSize;
	char*			unprocessedArgBufferPtr			=	argBuffer;

	do{
		unprocessedArgBufferPtr = CMPARSE_GetArgParametersBin(
				unprocessedArgBufferPtr,
				&unprocessedArgBufferSize,
				&localKeyValue,
				&localValue,
				size,
				&argValue
		);
		if(unprocessedArgBufferPtr == NULL) return CMPARSE_STATUS_ERROR;
		if(strncmp(key, localKeyValue.value, strlen(key)) == 0)
		{
			value->size = size;
			memcpy(value->value, argValue, size);
			return CMPARSE_STATUS_OK;
		}
		memset(value, 0, sizeof(cmparse_value_bin_t));
	}while(unprocessedArgBufferPtr != NULL);

	return CMPARSE_STATUS_ERROR;
}

