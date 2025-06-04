/**
 ******************************************************************************
 * @file   	control.c
 *
 * @brief  	Control service is in charge to receive and process control messages.
 * 			This service communicates with others services defined within this
 * 			firmware. Communications is mostly related to the configuration of
 * 			corresponding service or obtaining status messages from certain service
 * 			defined within control message content.
 * 			All control service logic is implemented within this file
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	December 2023
 ******************************************************************************
 */
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "lwip.h"
#include "lwip/api.h"
#include "lwip/sockets.h"

#include "control.h"
#include "logging.h"
#include "system.h"
#include "sstream.h"
#include "energy_debugger.h"
#include "CMParse/cmparse.h"
#include "dpcontrol.h"
#include "charger.h"

/**
 * @defgroup SERVICES Service
 * @{
 */

/**
 * @defgroup CONTROL_SERVICE Control service
 * @{
 */
/**
 * @defgroup CONTROL_PRIVATE_STRUCTURES Control service private structures defines
 * @{
 */
typedef struct
{
	TaskHandle_t		taskHandle;
	SemaphoreHandle_t	initSig;
	SemaphoreHandle_t	guard;
	char				requestBuffer[CONTROL_BUFFER_SIZE];
	char				responseBuffer[CONTROL_BUFFER_SIZE];
	uint16_t			responseBufferSize;
	control_state_t		state;
	uint32_t			disconnectionCounter;
	uint32_t			numberOfStatusLinks;
}control_data_t;

typedef struct
{
	TaskHandle_t			taskHandle;
	SemaphoreHandle_t		initSig;
	SemaphoreHandle_t		guard;
	QueueHandle_t			messageQueue;
	control_state_t			state;
	control_link_state_t	linkState;
	char					messageBuffer[CONTROL_BUFFER_SIZE];
	uint32_t				messageBufferSize;
}control_status_link_data_t;

typedef struct
{
	char							message[CONTROL_BUFFER_SIZE];
	uint32_t						messageSize;
	contol_status_message_type_t	type;
}control_status_message_t;
/**
 * @}
 */
/**
 * @defgroup CONTROL_PRIVATE_DATA Control service private data instances
 * @{
 */
static control_data_t				prvCONTROL_DATA;
static control_status_link_data_t	prvCONTROL_STATUS_LINK_DATA[CONTROL_STATUS_LINK_MAX_NO];
/**
 * @}
 */
/**
 * @defgroup CONTROL_PRIVATE_FUNCTIONS Control service private functions
 * @{
 */

/**
 * @brief	Prepare response in case of error
 * @param	response: buffer where response message will be stored
 * @param	responseSize: response message size
 * @retval	void
 */
static void inline prvCONTROL_PrepareErrorResponse(char* response, uint16_t* responseSize)
{
	uint32_t	tmpIncreaseSize  = 0;
	char* tmpResponsePtr = response;
	tmpIncreaseSize = strlen(CONTROL_RESPONSE_ERROR_STATUS_MSG);
	memcpy(tmpResponsePtr, CONTROL_RESPONSE_ERROR_STATUS_MSG, tmpIncreaseSize);
	tmpResponsePtr	+= tmpIncreaseSize;
	*responseSize	+= tmpIncreaseSize;

	tmpIncreaseSize = strlen(" 1");
	memcpy(tmpResponsePtr, " 1", tmpIncreaseSize);
	tmpResponsePtr	+= tmpIncreaseSize;
	*responseSize	+= tmpIncreaseSize;

	memcpy(tmpResponsePtr, "\r\n", 2);
	tmpResponsePtr	+= 2;
	*responseSize	+= 2;
}
/**
 * @brief	Prepare response in case when request is successfully process
 * @param	response: buffer where response message will be stored
 * @param	responseSize: response message size
 * @param	msg: message that will be integrated between ::CONTROL_RESPONSE_OK_STATUS_MSG and end of the message defined within "\r\n"
 * @param	msgSize: size of the ::msg
 * @param	responseSize: response message size
 * @retval	void
 */
static void inline prvCONTROL_PrepareOkResponse(char* response, uint16_t* responseSize, char* msg, uint32_t msgSize)
{
	uint32_t	tmpIncreaseSize  = 0;
	char* tmpResponsePtr = response;
	tmpIncreaseSize = strlen(CONTROL_RESPONSE_OK_STATUS_MSG);
	memcpy(tmpResponsePtr, CONTROL_RESPONSE_OK_STATUS_MSG, tmpIncreaseSize);
	tmpResponsePtr	+= tmpIncreaseSize;
	*responseSize	+= tmpIncreaseSize;

	memcpy(tmpResponsePtr, " ", 1);
	tmpResponsePtr	+= 1;
	*responseSize	+= 1;

	memcpy(tmpResponsePtr, msg, msgSize);
	tmpResponsePtr	+= msgSize;
	*responseSize	+= msgSize;

	memcpy(tmpResponsePtr, "\r\n", 2);
	tmpResponsePtr	+= 2;
	*responseSize	+= 2;
}
/**
 * @brief	Get device name from system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_UndefinedCommand(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	*responseSize = 0;

	prvCONTROL_PrepareErrorResponse(response, responseSize);
	return;
}
/**
 * @brief	Get device name from system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_GetDeviceName(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	uint32_t 	deviceNameSize;
	char tmpDeviceName[CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX];
	memset(tmpDeviceName, 0, CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX);

	*responseSize = 0;

	if(SYSTEM_GetDeviceName(tmpDeviceName, &deviceNameSize) != SYSTEM_STATUS_OK  )
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}

	prvCONTROL_PrepareOkResponse(response, responseSize, tmpDeviceName, deviceNameSize);
	LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Device name successfully obtained\r\n");
}
/**
 * @brief	Set device name by utilazing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetDeviceName(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t	value;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}

	if(SYSTEM_SetDeviceName(value.value) != SYSTEM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}

	prvCONTROL_PrepareOkResponse(response, responseSize, "", 0);
	LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Device name successfully set\r\n");
}

/**
 * @brief	Set RGB color
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetRGBColor(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t	value;
	uint32_t		intValue;
	system_rgb_value_t rgbValue;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "r", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}
	sscanf(value.value, "%lu", &intValue);

	rgbValue.red = (uint8_t)intValue;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "g", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}
	sscanf(value.value, "%lu", &intValue);

	rgbValue.green = (uint8_t)intValue;

	memset(&value, 0, sizeof(cmparse_value_t));

	if(CMPARSE_GetArgValue(arguments, argumentsLength, "b", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}
	sscanf(value.value, "%lu", &intValue);

	rgbValue.blue = (uint8_t)intValue;

	if(SYSTEM_SetRGB(rgbValue) != SYSTEM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}

	prvCONTROL_PrepareOkResponse(response, responseSize, "", 0);
	LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "RGB Color sucessfully set\r\n");
}

/**
 * @brief	Set device resolution by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetResolution(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					valueNumber;
	uint32_t					streamID;
	sstream_connection_info*  	connectionInfo;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain device resolution from control message\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &valueNumber);

	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream connection info\r\n");
		return;
	}

	if(SSTREAM_SetResolution(connectionInfo, valueNumber, 1000) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set %d resolution\r\n", value);
		return;
	}
	prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
}

/**
 * @brief	Set device ADC buffer number of samples
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetSamplesNo(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					samplesNo;
	uint32_t					streamID;
	sstream_connection_info*  	connectionInfo;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n", samplesNo);
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain number of samples from control message\r\n", samplesNo);
		return;
	}
	sscanf(value.value, "%lu", &samplesNo);

	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream connection info\r\n");
		return;
	}

	if(SSTREAM_SetSamplesNo(connectionInfo, samplesNo, 1000) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set %d number of samples\r\n", value);
		return;
	}
	prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
}

/**
 * @brief	Get device resolution by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_GetResolution(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	sstream_adc_resolution_t 	adcResolution;
	sstream_connection_info*  	connectionInfo;
	char						adcResolutionString[5];
	uint32_t					adcResolutionStringLength = 0;
	uint32_t					streamID;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n");
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(adcResolutionString, 0, 5);
	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}
	adcResolution = SSTREAM_GetResolution(connectionInfo, 1000);
	adcResolutionStringLength = sprintf(adcResolutionString, "%d", adcResolution);
	prvCONTROL_PrepareOkResponse(response, responseSize, adcResolutionString, adcResolutionStringLength);
}

/**
 * @brief	Get ADC value
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_GetADCValue(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					channel;
	uint32_t 					adcValue;
	sstream_connection_info*  	connectionInfo;
	char						adcValueString[5];
	uint32_t					adcValueStringLength = 0;
	uint32_t					streamID;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n");
		return;
	}
	sscanf(value.value, "%lu", &streamID);


	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "ch", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n");
		return;
	}
	sscanf(value.value, "%lu", &channel);

	memset(adcValueString, 0, 5);
	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}
	if(SSTREAM_GetAdcValue(connectionInfo, channel,&adcValue, 1000) == SSTREAM_STATUS_OK)
	{
		adcValueStringLength = sprintf(adcValueString, "%d", adcValue);
		prvCONTROL_PrepareOkResponse(response, responseSize, adcValueString, adcValueStringLength);
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to read adc\r\n", value);
		return;
	}

}
/**
 * @brief	Enable or disable DAC
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetDACActiveStatus(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					enableStatus;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain enable value\r\n");
		return;
	}
	sscanf(value.value, "%lu", &enableStatus);


	if(DPCONTROL_SetDACStatus(enableStatus, 1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Active status successfully set\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set active status\r\n");
		return;
	}

}

static void prvCONTROL_GetDACActiveStatus(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	dpcontrol_dac_status_t	activeState = 0;
	char						activeStateString[10];
	uint32_t					activeStateStringLength = 0;

	if(DPCONTROL_GetDACStatus(&activeState, 1000) != DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get DAC status\r\n");
	}
	else
	{
		memset(activeStateString, 0, 10);
		activeStateStringLength = sprintf(activeStateString, "%u", (int)activeState);
		prvCONTROL_PrepareOkResponse(response, responseSize, activeStateString, activeStateStringLength);
	}
}


/**
 * @brief	Enable charging
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargingEnable(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	if(CHARGER_SetChargingState(CHARGER_CHARGING_ENABLE, 1000) == CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service",  LOGGING_MSG_TYPE_INFO, "Charging successfully enabled\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to enable charging\r\n");
		return;
	}
}

/**
 * @brief	Disable charging
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargingDisable(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	if(CHARGER_SetChargingState(CHARGER_CHARGING_DISABLE, 1000) == CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Charging successfully disabled\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to disable Charging\r\n");
		return;
	}
}

static void prvCONTROL_ChargingGet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	charger_charging_state_t	chargingState = 0;
	char						chargingStateString[10];
	uint32_t					chargingStateStringLength = 0;

	if(CHARGER_GetChargingState(&chargingState, 1000) != DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get Charging status\r\n");
	}
	else
	{
		memset(chargingStateString, 0, 10);
		chargingStateStringLength = sprintf(chargingStateString, "%u", (int)chargingState);
		prvCONTROL_PrepareOkResponse(response, responseSize, chargingStateString, chargingStateStringLength);
	}
}

/**
 * @brief	Set charging current value
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargingCurrentSet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					current;
	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain current value\r\n");
		return;
	}
	sscanf(value.value, "%lu", &current);

	if(CHARGER_SetChargingCurrent(current, 1000) == CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Charging current %d [mA] set\r\n", current);
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set charging current\r\n");
		return;
	}
}
static void prvCONTROL_ChargingCurrentGet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	uint16_t					chargingCurrent = 0;
	char						chargingCurrentString[10];
	uint32_t					chargingCurrentStringLength = 0;

	if(CHARGER_GetChargingCurrent(&chargingCurrent, 1000) != CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get Charging current\r\n");
	}
	else
	{
		memset(chargingCurrentString, 0, 10);
		chargingCurrentStringLength = sprintf(chargingCurrentString, "%u", (int)chargingCurrent);
		prvCONTROL_PrepareOkResponse(response, responseSize, chargingCurrentString, chargingCurrentStringLength);
	}
}

static void prvCONTROL_ChargingTermCurrentSet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					current;
	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain current value\r\n");
		return;
	}
	sscanf(value.value, "%lu", &current);

	if(CHARGER_SetChargingTermCurrent(current, 1000) == CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Charging termination current set to %d [%]\r\n", current);
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set charging termination current\r\n");
		return;
	}
}

static void prvCONTROL_ChargingTermCurrentGet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	uint16_t					chargingTermCurrent = 0;
	char						chargingTermCurrentString[10];
	uint32_t					chargingTermCurrentStringLength = 0;

	if(CHARGER_GetChargingTermCurrent(&chargingTermCurrent, 1000) != CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get Charging status\r\n");
	}
	else
	{
		memset(chargingTermCurrentString, 0, 10);
		chargingTermCurrentStringLength = sprintf(chargingTermCurrentString, "%u", (int)chargingTermCurrent);
		prvCONTROL_PrepareOkResponse(response, responseSize, chargingTermCurrentString, chargingTermCurrentStringLength);
	}
}

static void prvCONTROL_ChargingTermVoltageSet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	float						voltage;
	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain current value\r\n");
		return;
	}
	sscanf(value.value, "%f", &voltage);

	if(CHARGER_SetChargingTermVoltage(voltage, 1000) == CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Charging termination voltage set to %.2f [V]\r\n", voltage);
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set charging termination voltage\r\n");
		return;
	}
}

static void prvCONTROL_ChargingTermVoltageGet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	float						chargingTermVoltage = 0;
	char						chargingTermVoltageString[10];
	uint32_t					chargingTermVoltageStringLength = 0;

	if(CHARGER_GetChargingTermVoltage(&chargingTermVoltage, 1000) != CHARGER_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get Charging termination voltage\r\n");
	}
	else
	{
		memset(chargingTermVoltageString, 0, 10);
		chargingTermVoltageStringLength = sprintf(chargingTermVoltageString, "%.2f", chargingTermVoltage);
		prvCONTROL_PrepareOkResponse(response, responseSize, chargingTermVoltageString, chargingTermVoltageStringLength);
	}
}

/**
 * @brief	Get charger register content
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_ChargerReadReg(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					regAddr;
	uint8_t						regVal;
	char						responseContent[50];
	uint32_t					responseContentSize;
	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "reg", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain current value\r\n");
		return;
	}
	sscanf(value.value, "%x", &regAddr);

	if(CHARGER_GetRegContent(regAddr, &regVal, 1000) == CHARGER_STATUS_OK)
	{
		responseContentSize = 0;
		memset(responseContent, 50, 0);
		responseContentSize = sprintf(responseContent, "OK: 0x%x",regVal);
		prvCONTROL_PrepareOkResponse(response, responseSize, responseContent, responseContentSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Reg  %d successfully read\r\n", regAddr);
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to read register current\r\n");
		return;
	}
}

/**
 * @brief	Enable load
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetLoadEnable(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	if(DPCONTROL_SetLoadState(DPCONTROL_LOAD_STATE_ENABLE, 1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Load status successfully set\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set load status\r\n");
		return;
	}
}
/**
 * @brief	Disable load
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetLoadDisable(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	if(DPCONTROL_SetLoadState(DPCONTROL_LOAD_STATE_DISABLE, 1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Load status successfully set\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set load status\r\n");
		return;
	}
}

static void prvCONTROL_GetLoad(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	dpcontrol_load_state_t		loadState = 0;
	char						loadStateString[10];
	uint32_t					loadStateStringLength = 0;

	if(DPCONTROL_GetLoadState(&loadState, 1000) != DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get load status\r\n");
	}
	else
	{
		memset(loadStateString, 0, 10);
		loadStateStringLength = sprintf(loadStateString, "%u", (int)loadState);
		prvCONTROL_PrepareOkResponse(response, responseSize, loadStateString, loadStateStringLength);
	}
}


/**
 * @brief	Enable battery
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetBatEnable(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	if(DPCONTROL_SetBatState(DPCONTROL_LOAD_STATE_ENABLE, 1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Load status successfully set\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set load status\r\n");
		return;
	}
}

static void prvCONTROL_GetBat(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	dpcontrol_bat_state_t		batState = 0;
	char						batStateString[10];
	uint32_t					batStateStringLength = 0;

	if(DPCONTROL_GetBatState(&batState, 1000) != DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get bat disable status\r\n");
	}
	else
	{
		memset(batStateString, 0, 10);
		batStateStringLength = sprintf(batStateString, "%u", (int)batState);
		prvCONTROL_PrepareOkResponse(response, responseSize, batStateString, batStateStringLength);
	}
}

/**
 * @brief	Enable power path
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetPPathEnable(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	if(DPCONTROL_SetPPathState(DPCONTROL_PPATH_STATE_ENABLE, 1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Power Path status successfully set\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set Power Path status\r\n");
		return;
	}
}

/**
 * @brief	Disable power path
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetPPathDisable(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	if(DPCONTROL_SetPPathState(DPCONTROL_PPATH_STATE_DISABLE, 1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Power Path status successfully set\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set Power Path status\r\n");
		return;
	}
}

static void prvCONTROL_GetPPath(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	dpcontrol_ppath_state_t		ppathState = 0;
	char						ppathStateString[10];
	uint32_t					ppathStateStringLength = 0;

	if(DPCONTROL_GetPPathState(&ppathState, 1000) != DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get ppath status\r\n");
	}
	else
	{
		memset(ppathStateString, 0, 10);
		ppathStateStringLength = sprintf(ppathStateString, "%u", (int)ppathState);
		prvCONTROL_PrepareOkResponse(response, responseSize, ppathStateString, ppathStateStringLength);
	}
}

static void prvCONTROL_GetUVoltage(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	dpcontrol_protection_state_t	state = 0;
	char							stateString[10];
	uint32_t						stateStringLength = 0;

	if(DPCONTROL_GetUVoltageState(&state, 1000) != DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get protection state\r\n");
	}
	else
	{
		memset(stateString, 0, 10);
		stateStringLength = sprintf(stateString, "%u", (int)state);
		prvCONTROL_PrepareOkResponse(response, responseSize, stateString, stateStringLength);
	}
}

static void prvCONTROL_GetOVoltage(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	dpcontrol_protection_state_t	state = 0;
	char							stateString[10];
	uint32_t						stateStringLength = 0;

	if(DPCONTROL_GetOVoltageState(&state, 1000) != DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get protection state\r\n");
	}
	else
	{
		memset(stateString, 0, 10);
		stateStringLength = sprintf(stateString, "%u", (int)state);
		prvCONTROL_PrepareOkResponse(response, responseSize, stateString, stateStringLength);
	}
}

static void prvCONTROL_GetOCurrent(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	dpcontrol_protection_state_t	state = 0;
	char							stateString[10];
	uint32_t						stateStringLength = 0;

	if(DPCONTROL_GetOCurrentState(&state, 1000) != DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get protection state\r\n");
	}
	else
	{
		memset(stateString, 0, 10);
		stateStringLength = sprintf(stateString, "%u", (int)state);
		prvCONTROL_PrepareOkResponse(response, responseSize, stateString, stateStringLength);
	}
}

/**
 * @brief	Trigger protection latch
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_LatchTrigger(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	if(DPCONTROL_LatchTriger(1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Latch successfully triggered\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to trigger latch\r\n");
		return;
	}
}

/**
 * @brief	Disable battery
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetBatDisable(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	if(DPCONTROL_SetBatState(DPCONTROL_BAT_STATE_DISABLE, 1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Load status successfully set\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set load status\r\n");
		return;
	}
}

/**
 * @brief	Enable or disable battery
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetBatteryState(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					enableStatus;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain enable value\r\n");
		return;
	}
	sscanf(value.value, "%lu", &enableStatus);

	if(DPCONTROL_SetBatState(enableStatus, 1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Battery status successfully set\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set battery status\r\n");
		return;
	}

}

/**
 * @brief	Set DAC value
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetDACValue(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					dacValue;
	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain enable value\r\n");
		return;
	}
	sscanf(value.value, "%lu", &dacValue);

	if(DPCONTROL_SetValue(dacValue, 1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "DAC value %d set\r\n", dacValue);
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set DAC value\r\n");
		return;
	}
}

static void prvCONTROL_GetDACValue(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	uint32_t						value = 0;
	char							valueString[10];
	uint32_t						valueStringLength = 0;

	if(DPCONTROL_GetValue(&value, 1000) != DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get DAC value\r\n");
	}
	else
	{
		memset(valueString, 0, 10);
		valueStringLength = sprintf(valueString, "%u", (int)value);
		prvCONTROL_PrepareOkResponse(response, responseSize, valueString, valueStringLength);
	}
}

/**
 * @brief	Set device clock div by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetClkdiv(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					valueNumber;
	uint32_t					streamID;
	sstream_connection_info*  	connectionInfo;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain device resolution from control message\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &valueNumber);

	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream connection info\r\n");
		return;
	}

	if(SSTREAM_SetClkDiv(connectionInfo, valueNumber, 1000) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set clock div  %d \r\n", value);
		return;
	}
	prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
}

/**
 * @brief	Get device clock div by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_GetClkdiv(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	sstream_adc_clk_div_t 		adcClkDiv;
	sstream_connection_info*  	connectionInfo;
	char						adcClkDivString[5];
	uint32_t					adcClkDivStringLength = 0;
	uint32_t					streamID;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n");
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(adcClkDivString, 0, 5);
	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain connection info\r\n");
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}
	adcClkDiv = SSTREAM_GetClkDiv(connectionInfo, 1000);
	adcClkDivStringLength = sprintf(adcClkDivString, "%d", adcClkDiv);
	prvCONTROL_PrepareOkResponse(response, responseSize, adcClkDivString, adcClkDivStringLength);
	LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Device clock div successfully obtained\r\n");
}

/**
 * @brief	Set device sample time by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetSamplingtime(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					prescaler;
	uint32_t					period;
	uint32_t					valueNumber;
	uint32_t					streamID;
	sstream_connection_info*  	connectionInfo;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "period", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain period from control message\r\n");
		return;
	}
	sscanf(value.value, "%lu", &period);
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "prescaler", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain prescaler from control message\r\n");
		return;
	}
	sscanf(value.value, "%lu", &prescaler);

	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream connection info\r\n");
		return;
	}

	if(SSTREAM_SetSamplingPeriod(connectionInfo, prescaler, period, 1000) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set sampling time %d\r\n", valueNumber);
		return;
	}
	prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
}

/**
 * @brief	Get device sample time by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_GetSamplingtime(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t			 		resolution;
	sstream_connection_info*  	connectionInfo;
	char						resolutionString[10];
	uint32_t					resolutionStringLength = 0;
	uint32_t					streamID;

	memset(&value, 0, sizeof(cmparse_value_t));

	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n");
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(resolutionString, 0, 10);
	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain connection info\r\n");
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}
	resolution = SSTREAM_GetSamplingPeriod(connectionInfo, 1000);
	resolutionStringLength = sprintf(resolutionString, "%lu", resolution);
	prvCONTROL_PrepareOkResponse(response, responseSize, resolutionString, resolutionStringLength);
}
/**
 * @brief	Set device averaging ratio by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetChSamplingtime(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					valueNumber;
	uint32_t					streamID;
	sstream_connection_info*  	connectionInfo;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain sampling time from control message\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &valueNumber);

	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream connection info\r\n");
		return;
	}

	if(SSTREAM_SetChannelSamplingTime(connectionInfo, 1, valueNumber, 1000) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set channel 0 sampling time %d\r\n", valueNumber);
		return;
	}

	if(SSTREAM_SetChannelSamplingTime(connectionInfo, 2, valueNumber, 1000) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set channel 1 sampling time %d\r\n", valueNumber);
		return;
	}
	prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
}

/**
 * @brief	Get device averaging ratio by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_GetChSamplingtime(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	sstream_adc_sampling_time_t	chstime1;
	sstream_connection_info*  	connectionInfo;
	char						chstimeString[10];
	uint32_t					chstimeStringLength = 0;
	uint32_t					streamID;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n");
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(chstimeString, 0, 10);
	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain connection info\r\n");
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}
	chstime1 = SSTREAM_GetChannelSamplingTime(connectionInfo, 1, 1000);
	chstimeStringLength = sprintf(chstimeString, "%lu", chstime1);
	prvCONTROL_PrepareOkResponse(response, responseSize, chstimeString, chstimeStringLength);
}
/**
 * @brief	Set device averaging ratio by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetAveragingratio(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					valueNumber;
	uint32_t					streamID;
	sstream_connection_info*  	connectionInfo;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain sampling time from control message\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &valueNumber);

	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream connection info\r\n");
		return;
	}

	if(SSTREAM_SetChannelAvgRatio(connectionInfo, 1, valueNumber, 1000) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set channel 1 averaging ratio %d\r\n", valueNumber);
		return;
	}
	prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
}

/**
 * @brief	Get device averaging ratio by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_GetAveragingratio(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	sstream_adc_ch_avg_ratio_t	ch1AveragingRatio;
	sstream_connection_info*  	connectionInfo;
	char						ch1AveragingRatioString[10];
	uint32_t					ch1AveragingRatioStringLength = 0;
	uint32_t					streamID;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n");
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(ch1AveragingRatioString, 0, 10);
	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain connection info\r\n");
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}
	ch1AveragingRatio = SSTREAM_GetChannelAvgRatio(connectionInfo, 1, 1000);
	ch1AveragingRatioStringLength = sprintf(ch1AveragingRatioString, "%lu", ch1AveragingRatio);
	prvCONTROL_PrepareOkResponse(response, responseSize, ch1AveragingRatioString, ch1AveragingRatioStringLength);
}

/**
 * @brief	Set device voltage offset by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetVoltageoffset(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					valueNumber;
	uint32_t					streamID;
	sstream_connection_info*  	connectionInfo;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain sampling time from control message\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &valueNumber);

	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream connection info\r\n");
		return;
	}

	if(SSTREAM_SetChannelOffset(connectionInfo, SSTREAM_AIN_VOLTAGE_CHANNEL, valueNumber, 1000) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set voltage offset %d\r\n", valueNumber);
		return;
	}
	prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
}

/**
 * @brief	Get device voltage offset by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_GetVoltageoffset(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					voltageOffset;
	sstream_connection_info*  	connectionInfo;
	char						voltageOffsetString[10];
	uint32_t					voltageOffsetStringLength = 0;
	uint32_t					streamID;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n");
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(voltageOffsetString, 0, 10);
	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain connection info\r\n");
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}
	voltageOffset = SSTREAM_GetChannelOffset(connectionInfo, SSTREAM_AIN_VOLTAGE_CHANNEL, 1000);
	voltageOffsetStringLength = sprintf(voltageOffsetString, "%lu", voltageOffset);
	prvCONTROL_PrepareOkResponse(response, responseSize, voltageOffsetString, voltageOffsetStringLength);
}

/**
 * @brief	Set device current offset by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_SetCurrentoffset(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					valueNumber;
	uint32_t					streamID;
	sstream_connection_info*  	connectionInfo;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain sampling time from control message\r\n", valueNumber);
		return;
	}
	sscanf(value.value, "%lu", &valueNumber);

	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream connection info\r\n");
		return;
	}

	if(SSTREAM_SetChannelOffset(connectionInfo, SSTREAM_AIN_CURRENT_CHANNEL, valueNumber, 1000) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set voltage offset %d\r\n", valueNumber);
		return;
	}
	prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
}

/**
 * @brief	Get device current offset by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_GetCurrentoffset(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					voltageOffset;
	sstream_connection_info*  	connectionInfo;
	char						voltageOffsetString[10];
	uint32_t					voltageOffsetStringLength = 0;
	uint32_t					streamID;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n");
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(voltageOffsetString, 0, 10);
	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain connection info\r\n");
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}
	voltageOffset = SSTREAM_GetChannelOffset(connectionInfo, SSTREAM_AIN_CURRENT_CHANNEL, 1000);
	voltageOffsetStringLength = sprintf(voltageOffsetString, "%lu", voltageOffset);
	prvCONTROL_PrepareOkResponse(response, responseSize, voltageOffsetString, voltageOffsetStringLength);
}

/**
 * @brief	Get ADC input clk
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_GetADCInputClk(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					adcClk;
	sstream_connection_info*  	connectionInfo;
	char						adcClkString[10];
	uint32_t					adcClkStringLength = 0;
	uint32_t					streamID;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n");
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	memset(adcClkString, 0, 10);
	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain connection info\r\n");
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}
	adcClk = SSTREAM_GetAdcInputClk(connectionInfo, 1000);
	adcClkStringLength = sprintf(adcClkString, "%lu", adcClk);
	prvCONTROL_PrepareOkResponse(response, responseSize, adcClkString, adcClkStringLength);
}

static void prvCONTROL_EPLinkCreate(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t						value;
	energy_debugger_connection_info		connectionInfo = {0};
	char								streamIDString[5];
	uint32_t							streamIDStringLength = 0;
	ip_addr_t							ip = {0};

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "ip", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain ip address\r\n");
		return;
	}
	//sscanf(value.value, "%hhu.%hhu.%hhu.%hhu", &connectionInfo.serverIp[0], &connectionInfo.serverIp[1], &connectionInfo.serverIp[2], &connectionInfo.serverIp[3]);
	ipaddr_aton(value.value, &ip);
	connectionInfo.serverIp[0] = (uint8_t)ip.addr;
	connectionInfo.serverIp[1] = (uint8_t)(ip.addr>>8);
	connectionInfo.serverIp[2] = (uint8_t)(ip.addr>>16);
	connectionInfo.serverIp[3] = (uint8_t)(ip.addr>>24);

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "port", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain port number\r\n");
		return;
	}
	sscanf(value.value, "%hu", &connectionInfo.serverport);

	if(ENERGY_DEBUGGER_CreateLink(&connectionInfo, 2000) != ENERGY_DEBUGGER_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to create eplink channel\r\n");
		return;
	}
	streamIDStringLength = sprintf(streamIDString, "%lu", connectionInfo.id);
	prvCONTROL_PrepareOkResponse(response, responseSize, streamIDString, streamIDStringLength);
	LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "EP Link successfully created\r\n");
}

static void prvCONTROL_StreamCreate(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	sstream_connection_info		connectionInfo = {0};
	char						streamIDString[5];
	uint32_t					streamIDStringLength = 0;
	ip_addr_t					ip = {0};

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "ip", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain ip address\r\n");
		return;
	}
	//sscanf(value.value, "%hhu.%hhu.%hhu.%hhu", &connectionInfo.serverIp[0], &connectionInfo.serverIp[1], &connectionInfo.serverIp[2], &connectionInfo.serverIp[3]);
	ipaddr_aton(value.value, &ip);
	connectionInfo.serverIp[0] = (uint8_t)ip.addr;
	connectionInfo.serverIp[1] = (uint8_t)(ip.addr>>8);
	connectionInfo.serverIp[2] = (uint8_t)(ip.addr>>16);
	connectionInfo.serverIp[3] = (uint8_t)(ip.addr>>24);

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "port", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain port number\r\n");
		return;
	}
	sscanf(value.value, "%hu", &connectionInfo.serverport);

	if(SSTREAM_CreateChannel(&connectionInfo, 3000) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to create stream channel\r\n");
		return;
	}
	streamIDStringLength = sprintf(streamIDString, "%lu", connectionInfo.id);
	prvCONTROL_PrepareOkResponse(response, responseSize, streamIDString, streamIDStringLength);
	LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Stream successfully created\r\n");
}

/**
 * @brief	Start samples streaming
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_StreamStart(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					streamID;
	uint32_t					adcValue;
	sstream_adc_t				adc = 0;
	sstream_connection_info*  	connectionInfo;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n");
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	if(CMPARSE_GetArgValue(arguments, argumentsLength, "adc", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain adc value\r\n", adcValue);
		return;
	}
	sscanf(value.value, "%lu", &adcValue);

	adc = adcValue;

	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream connection info\r\n");
		return;
	}

	if(SSTREAM_Start(connectionInfo, adc, 3000) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to start stream\r\n");
		return;
	}
	prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
}

/**
 * @brief	Stop samples streaming
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_StreamStop(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	uint32_t					streamID;
	sstream_connection_info*  	connectionInfo;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "sid", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream ID\r\n");
		return;
	}
	sscanf(value.value, "%lu", &streamID);

	if(SSTREAM_GetConnectionByID(&connectionInfo, streamID) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain stream connection info\r\n");
		return;
	}

	if(SSTREAM_Stop(connectionInfo, 1000) != SSTREAM_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to stop stream\r\n");
		return;
	}
	prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
}

//TODO: This is just for testing purposes. It should never be used as it is now. Remove!
control_status_link_instance_t statusLinkInstance;
/**
 * @brief	Create status link
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_CreateStatusLink(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{

	cmparse_value_t					value;
	control_status_link_ip_info_t	connectionInfo = {0};
	ip_addr_t						ip = {0};
	uint32_t						size = 0;
	char 							instanceNoStr[10];

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "ip", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain ip address\r\n");
		return;
	}
	//sscanf(value.value, "%hhu.%hhu.%hhu.%hhu", &connectionInfo.serverIp[0], &connectionInfo.serverIp[1], &connectionInfo.serverIp[2], &connectionInfo.serverIp[3]);
	ipaddr_aton(value.value, &ip);
	connectionInfo.ip[0] = (uint8_t)ip.addr;
	connectionInfo.ip[1] = (uint8_t)(ip.addr>>8);
	connectionInfo.ip[2] = (uint8_t)(ip.addr>>16);
	connectionInfo.ip[3] = (uint8_t)(ip.addr>>24);

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "port", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain port number\r\n");
		return;
	}
	sscanf(value.value, "%hu", &connectionInfo.portNo);

	if(CONTROL_StatusLinkCreate(&statusLinkInstance, connectionInfo, 2000) != CONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to create status link\r\n");
		return;
	}

	size = sprintf(instanceNoStr,"%d",(int)statusLinkInstance.linkInstanceNo);
	prvCONTROL_PrepareOkResponse(response, responseSize, instanceNoStr, size);

}
//TODO: This function is introduced for testing purposes only. Remove it in production phase!
/**
 * @brief	Send message over status link
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_StatusLinkSendMessage(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t	value;

	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		return;
	}

	if(CONTROL_StatusLinkSendMessage(value.value, CONTROL_STATUS_MESSAGE_TYPE_INFO, 2000) != CONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to send stream message\r\n");
		return;
	}
	prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
}
/**
 * @brief	Main control service task
 * @param	pvParameter: value forwarded during task creation
 * @retval	void
 */
static void prvCONTROL_TaskFunc(void* pvParameter)
{
	int 			sock, newconn, size;
    struct sockaddr_in address, remotehost;
	int				err;
	struct timeval	tv;
	uint32_t		tmpval;

	for(;;){
		switch(prvCONTROL_DATA.state)
		{
		case CONTROL_STATE_INIT:

			memset(prvCONTROL_DATA.requestBuffer, 	0, CONTROL_BUFFER_SIZE);
			memset(prvCONTROL_DATA.responseBuffer, 	0, CONTROL_BUFFER_SIZE);
			prvCONTROL_DATA.responseBufferSize = 0;

			LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Control Service started\r\n");
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if(sock < 0){
				LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "There is a problem to create TCP socket\r\n");
				prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
				break;
			}
			LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "TCP connection successfully created\r\n");

			address.sin_family = AF_INET;
			address.sin_port = htons(CONFIG_CONTROL_SERVER_PORT);
			address.sin_addr.s_addr = INADDR_ANY;

			err = bind(sock, (struct sockaddr *)&address, sizeof (address));
			if(err < 0){
				LOGGING_Write("Control Service",LOGGING_MSG_TYPE_ERROR,  "There is a problem to bind TCP socket\r\n");
				prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
				break;
			}
			LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "TCP Connection successfully bound to port %d\r\n", CONTROL_SERVER_PORT);

			listen(sock, 5);
		    size = sizeof(remotehost);

			if(xSemaphoreGive(prvCONTROL_DATA.initSig) != pdTRUE)
			{
				LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR,  "There is a problem with init semaphore\r\n");
				prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
				break;
			}

			prvCONTROL_DATA.state = CONTROL_STATE_SERVICE;

			break;
		case CONTROL_STATE_SERVICE:
			LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Wait for new connection on port %d\r\n", CONTROL_SERVER_PORT);
			newconn = accept(sock, (struct sockaddr *)&remotehost, (socklen_t *)&size);

			if(xSemaphoreTake(prvCONTROL_DATA.guard, portMAX_DELAY) != pdPASS)
			{
				LOGGING_Write("Control Service",LOGGING_MSG_TYPE_ERROR,  "There is a problem to take guard semaphore\r\n");
				prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
				break;
			}

			prvCONTROL_DATA.disconnectionCounter = 0;

			if(xSemaphoreGive(prvCONTROL_DATA.guard) != pdPASS)
			{
				LOGGING_Write("Control Service",LOGGING_MSG_TYPE_ERROR,  "There is a problem to release guard semaphore\r\n");
				prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
				break;
			}

			LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO,  "New connection accepted\r\n");
			LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO,  "Connection id: %d\r\n", newconn);

			tv.tv_sec = 1;
			tv.tv_usec = 0;
			if(setsockopt(newconn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)) != 0)
			{
				LOGGING_Write("Control Service", LOGGING_MSG_TYPE_WARNNING,  "Unable to set socket option\r\n");
				close(newconn);
				break;
			}
			while(1){
				err = read(newconn, prvCONTROL_DATA.requestBuffer, CONTROL_BUFFER_SIZE);
				if(err == 0){
					break;
				}
				if(err < 0 )
				{
					if(xSemaphoreTake(prvCONTROL_DATA.guard, portMAX_DELAY) != pdPASS)
					{
						LOGGING_Write("Control Service",LOGGING_MSG_TYPE_ERROR,  "There is a problem to take guard semaphore\r\n");
						prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
						break;
					}

					tmpval = prvCONTROL_DATA.disconnectionCounter;

					if(xSemaphoreGive(prvCONTROL_DATA.guard) != pdPASS)
					{
						LOGGING_Write("Control Service",LOGGING_MSG_TYPE_ERROR,  "There is a problem to release guard semaphore\r\n");
						prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
						break;
					}
					if(tmpval != 0) break;
					else continue;
				}
				if(CMPARSE_Execute(prvCONTROL_DATA.requestBuffer, prvCONTROL_DATA.responseBuffer, &prvCONTROL_DATA.responseBufferSize) != CMPARSE_STATUS_OK)
				{
					LOGGING_Write("Control Service", LOGGING_MSG_TYPE_WARNNING, "There is error during control message parsing procedure\r\n");
					memcpy(prvCONTROL_DATA.responseBuffer, "ERROR 0",strlen("ERROR 0"));
					prvCONTROL_DATA.responseBufferSize = strlen("ERROR 0");
				}
				else
				{
					LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Control message:%s\r\n", prvCONTROL_DATA.requestBuffer);
					LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Response:       %s", prvCONTROL_DATA.responseBuffer);
				}
				err = write(newconn, prvCONTROL_DATA.responseBuffer, prvCONTROL_DATA.responseBufferSize);
				if(err < 0)
				{
					LOGGING_Write("Control Service", LOGGING_MSG_TYPE_WARNNING, "There is a problem to send message\r\n");
				}
				/* Reinit buffers */
				memset(prvCONTROL_DATA.requestBuffer, 	0, CONTROL_BUFFER_SIZE);
				memset(prvCONTROL_DATA.responseBuffer, 	0, CONTROL_BUFFER_SIZE);
				prvCONTROL_DATA.responseBufferSize = 0;
			}
			LOGGING_Write("Control Service", LOGGING_MSG_TYPE_WARNNING,  "Connection closed\r\n");
			close(newconn);
			break;
		case CONTROL_STATE_ERROR:
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(portMAX_DELAY);
			break;
		}

	}
}
/**
 * @brief	Status link task
 * @param	pvParameter: pointer to link status instance
 * @retval	void
 */
static void prvCONTROL_StatusLinkTaskFunc(void* pvParameter)
{
	control_status_link_instance_t linkInstance;
	control_status_message_t message;
	memset(&message, 0, sizeof(control_status_message_t));
	memcpy(&linkInstance, pvParameter, sizeof(control_status_link_instance_t));
    struct netconn *conn;
    err_t 			connect_err;
	ip_addr_t 		remote_ip;

	for(;;){
		switch(prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].state)
		{
		case CONTROL_STATE_INIT:

			conn = netconn_new(NETCONN_TCP);

			IP_ADDR4(&remote_ip, linkInstance.ipInfo.ip[0], linkInstance.ipInfo.ip[1], linkInstance.ipInfo.ip[2], linkInstance.ipInfo.ip[3]);

			LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_INFO,  "Try to create status link connection with server:\r\n");
			LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_INFO,  "Server IP: %d.%d.%d.%d\r\n",
					linkInstance.ipInfo.ip[0],
					linkInstance.ipInfo.ip[1],
					linkInstance.ipInfo.ip[2],
					linkInstance.ipInfo.ip[3]);
			LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_INFO,  "Server Port: %d\r\n",linkInstance.ipInfo.portNo);

			connect_err = netconn_connect(conn, &remote_ip, linkInstance.ipInfo.portNo);

			if(connect_err != ERR_OK)
			{
				LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_ERROR,  "There is a problem to connect to status link server\r\n");
				prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
				break;
			}

			LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_INFO,  "Device is successfully connected to status link server\r\n");
			prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].linkState = CONTROL_LINK_STATE_UP;

			if(xSemaphoreGive(prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].initSig) != pdTRUE)
			{
				LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_ERROR,  "There is a problem with init semaphore\r\n");
				prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
				break;
			}

			LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_INFO,  "Status link with id %d sucesfully created\r\n");
			prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].state = CONTROL_STATE_SERVICE;
			prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].linkState = CONTROL_LINK_STATE_UP;
			break;
		case CONTROL_STATE_SERVICE:
			if(xQueueReceive(prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].messageQueue, &message, portMAX_DELAY) != pdPASS)
			{
				LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_ERROR,  "Unable to read status message queue\r\n");
				prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
				break;
			}
			switch(message.type)
			{
			case CONTROL_STATUS_MESSAGE_TYPE_ACTION:
				prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].messageBuffer[0] = 1;
				prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].messageBufferSize += 1;
				break;
			case CONTROL_STATUS_MESSAGE_TYPE_INFO:
				prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].messageBuffer[0] = 0;
				prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].messageBufferSize += 1;
				break;
			}
			memcpy(&prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].messageBuffer[1], message.message, message.messageSize);
			prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].messageBufferSize += message.messageSize;
			if(netconn_write(conn,
					prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].messageBuffer,
					prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].messageBufferSize,
					NETCONN_COPY) != ERR_OK)
			{
				LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_WARNNING,  "Unable to send status message\r\n");
			}
			else
			{
				LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_INFO,  "Status message successfully sent\r\n");
			}
			memset(&message, 0, sizeof(control_status_message_t));
			memset(prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].messageBuffer, 0, CONTROL_BUFFER_SIZE);
			prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].messageBufferSize = 0;
			break;
		case CONTROL_STATE_ERROR:
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(portMAX_DELAY);
			break;
		}

	}
}
/**
 * @}
 */
control_status_t 	CONTROL_Init(uint32_t initTimeout){

	if(xTaskCreate(
			prvCONTROL_TaskFunc,
			CONTROL_TASK_NAME,
			CONTROL_TASK_STACK,
			NULL,
			CONTROL_TASK_PRIO, &prvCONTROL_DATA.taskHandle) != pdPASS) return CONTROL_STATUS_ERROR;

	prvCONTROL_DATA.initSig = xSemaphoreCreateBinary();

	if(prvCONTROL_DATA.initSig == NULL) return CONTROL_STATUS_ERROR;

	prvCONTROL_DATA.guard = xSemaphoreCreateMutex();

	prvCONTROL_DATA.disconnectionCounter = 0;
	prvCONTROL_DATA.numberOfStatusLinks  = 0;

	if(prvCONTROL_DATA.guard == NULL) return CONTROL_STATUS_ERROR;

	prvCONTROL_DATA.state = CONTROL_STATE_INIT;
	memset(prvCONTROL_STATUS_LINK_DATA, 0, CONFIG_CONTROL_STATUS_LINK_MAX_NO*sizeof(control_status_link_data_t));

	if(xSemaphoreTake(prvCONTROL_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdTRUE) return CONTROL_STATUS_ERROR;

	/* Add commands */
	CMPARSE_AddCommand("", 								prvCONTROL_UndefinedCommand);
	CMPARSE_AddCommand("device hello", 					prvCONTROL_GetDeviceName);
	CMPARSE_AddCommand("device setname", 				prvCONTROL_SetDeviceName);
	CMPARSE_AddCommand("device slink create", 			prvCONTROL_CreateStatusLink);
	CMPARSE_AddCommand("device slink send", 			prvCONTROL_StatusLinkSendMessage);
	CMPARSE_AddCommand("device eplink create", 			prvCONTROL_EPLinkCreate);
	CMPARSE_AddCommand("device stream create", 			prvCONTROL_StreamCreate);
	CMPARSE_AddCommand("device stream start", 			prvCONTROL_StreamStart);
	CMPARSE_AddCommand("device stream stop", 			prvCONTROL_StreamStop);

	CMPARSE_AddCommand("device adc chresolution set", 	prvCONTROL_SetResolution);
	CMPARSE_AddCommand("device adc chresolution get", 	prvCONTROL_GetResolution);
	CMPARSE_AddCommand("device adc chclkdiv set", 		prvCONTROL_SetClkdiv);
	CMPARSE_AddCommand("device adc chclkdiv get", 		prvCONTROL_GetClkdiv);
	CMPARSE_AddCommand("device adc chstime set", 		prvCONTROL_SetChSamplingtime);
	CMPARSE_AddCommand("device adc chstime get", 		prvCONTROL_GetChSamplingtime);
	CMPARSE_AddCommand("device adc chavrratio set", 	prvCONTROL_SetAveragingratio);
	CMPARSE_AddCommand("device adc chavrratio get", 	prvCONTROL_GetAveragingratio);
	CMPARSE_AddCommand("device adc speriod set", 		prvCONTROL_SetSamplingtime);
	CMPARSE_AddCommand("device adc speriod get", 		prvCONTROL_GetSamplingtime);
	CMPARSE_AddCommand("device adc voffset set", 		prvCONTROL_SetVoltageoffset);
	CMPARSE_AddCommand("device adc voffset get", 		prvCONTROL_GetVoltageoffset);
	CMPARSE_AddCommand("device adc coffset set", 		prvCONTROL_SetCurrentoffset);
	CMPARSE_AddCommand("device adc coffset get", 		prvCONTROL_GetCurrentoffset);
	CMPARSE_AddCommand("device adc clk get", 			prvCONTROL_GetADCInputClk);
	CMPARSE_AddCommand("device adc value get", 			prvCONTROL_GetADCValue);
	CMPARSE_AddCommand("device adc samplesno set", 		prvCONTROL_SetSamplesNo);

	CMPARSE_AddCommand("device dac enable set", 		prvCONTROL_SetDACActiveStatus);
	CMPARSE_AddCommand("device dac enable get", 		prvCONTROL_GetDACActiveStatus);
	CMPARSE_AddCommand("device dac value set", 			prvCONTROL_SetDACValue);
	CMPARSE_AddCommand("device dac value get", 			prvCONTROL_GetDACValue);

	CMPARSE_AddCommand("device load enable", 			prvCONTROL_SetLoadEnable);
	CMPARSE_AddCommand("device load disable", 			prvCONTROL_SetLoadDisable);
	CMPARSE_AddCommand("device load get", 				prvCONTROL_GetLoad);

	CMPARSE_AddCommand("device bat enable", 			prvCONTROL_SetBatEnable);
	CMPARSE_AddCommand("device bat disable", 			prvCONTROL_SetBatDisable);
	CMPARSE_AddCommand("device bat get", 				prvCONTROL_GetBat);

	CMPARSE_AddCommand("device ppath enable", 			prvCONTROL_SetPPathEnable);
	CMPARSE_AddCommand("device ppath disable", 			prvCONTROL_SetPPathDisable);
	CMPARSE_AddCommand("device ppath get", 				prvCONTROL_GetPPath);


	CMPARSE_AddCommand("device uvoltage get", 		    prvCONTROL_GetUVoltage);
	CMPARSE_AddCommand("device ovoltage get", 		    prvCONTROL_GetOVoltage);
	CMPARSE_AddCommand("device ocurrent get", 		    prvCONTROL_GetOCurrent);

	CMPARSE_AddCommand("device latch trigger", 			prvCONTROL_LatchTrigger);


	CMPARSE_AddCommand("device rgb setcolor",     		prvCONTROL_SetRGBColor);


	CMPARSE_AddCommand("charger charging enable",       prvCONTROL_ChargingEnable);
	CMPARSE_AddCommand("charger charging disable",      prvCONTROL_ChargingDisable);
	CMPARSE_AddCommand("charger charging get",      	prvCONTROL_ChargingGet);


	CMPARSE_AddCommand("charger charging current set",  	prvCONTROL_ChargingCurrentSet);
	CMPARSE_AddCommand("charger charging current get",  	prvCONTROL_ChargingCurrentGet);
	CMPARSE_AddCommand("charger charging termcurrent set",  prvCONTROL_ChargingTermCurrentSet);
	CMPARSE_AddCommand("charger charging termcurrent get",  prvCONTROL_ChargingTermCurrentGet);
	CMPARSE_AddCommand("charger charging termvoltage set",  prvCONTROL_ChargingTermVoltageSet);
	CMPARSE_AddCommand("charger charging termvoltage get",  prvCONTROL_ChargingTermVoltageGet);
	CMPARSE_AddCommand("charger reg read",  				prvCONTROL_ChargerReadReg);


	return CONTROL_STATUS_OK;
}

control_status_t 	CONTROL_LinkClosed()
{
	if(xSemaphoreTake(prvCONTROL_DATA.guard, portMAX_DELAY) != pdPASS)
	{
		return CONTROL_STATUS_ERROR;
	}

	prvCONTROL_DATA.disconnectionCounter += 1;

	if(xSemaphoreGive(prvCONTROL_DATA.guard) != pdPASS)
	{
		return CONTROL_STATUS_ERROR;
	}

	return CONTROL_STATUS_OK;
}

control_status_t 	CONTROL_StatusLinkCreate(control_status_link_instance_t* statusLinkInstance, control_status_link_ip_info_t statusServerIp, uint32_t timeout)
{
	if(prvCONTROL_DATA.numberOfStatusLinks > CONFIG_CONTROL_STATUS_LINK_MAX_NO) return CONTROL_STATUS_ERROR;
	statusLinkInstance->linkInstanceNo = prvCONTROL_DATA.numberOfStatusLinks;
	memcpy(&statusLinkInstance->ipInfo, &statusServerIp, sizeof(control_status_link_ip_info_t));

	prvCONTROL_STATUS_LINK_DATA[statusLinkInstance->linkInstanceNo].initSig = xSemaphoreCreateBinary();

	if(prvCONTROL_STATUS_LINK_DATA[statusLinkInstance->linkInstanceNo].initSig == NULL) return CONTROL_STATUS_ERROR;

	prvCONTROL_STATUS_LINK_DATA[statusLinkInstance->linkInstanceNo].guard = xSemaphoreCreateMutex();

	if(prvCONTROL_STATUS_LINK_DATA[statusLinkInstance->linkInstanceNo].guard == NULL) return CONTROL_STATUS_ERROR;

	prvCONTROL_STATUS_LINK_DATA[statusLinkInstance->linkInstanceNo].messageQueue =
			xQueueCreate(CONTROL_STATUS_LINK_MESSAGES_MAX_NO, sizeof(control_status_message_t));

	if(prvCONTROL_STATUS_LINK_DATA[statusLinkInstance->linkInstanceNo].messageQueue == NULL) return CONTROL_STATUS_ERROR;

	prvCONTROL_STATUS_LINK_DATA[statusLinkInstance->linkInstanceNo].state = CONTROL_STATE_INIT;
	prvCONTROL_STATUS_LINK_DATA[statusLinkInstance->linkInstanceNo].linkState = CONTROL_LINK_STATE_DOWN;

	if(xTaskCreate(prvCONTROL_StatusLinkTaskFunc,
				CONTROL_STATUS_LINK_TASK_NAME,
				CONTROL_STATUS_LINK_TASK_STACK,
				statusLinkInstance,
				CONTROL_STATUS_LINK_TASK_PRIO,
				&prvCONTROL_STATUS_LINK_DATA[prvCONTROL_DATA.numberOfStatusLinks].taskHandle) != pdPASS) return CONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvCONTROL_STATUS_LINK_DATA[statusLinkInstance->linkInstanceNo].initSig, timeout) != pdPASS) return CONTROL_STATUS_ERROR;

	if(prvCONTROL_STATUS_LINK_DATA[statusLinkInstance->linkInstanceNo].linkState != CONTROL_LINK_STATE_UP) return CONTROL_STATUS_ERROR;

	prvCONTROL_DATA.numberOfStatusLinks += 1;

	return CONTROL_STATUS_OK;

}

control_status_t 	CONTROL_StatusLinkSendMessage(const char* message, contol_status_message_type_t type, uint32_t timeout)
{
	if(prvCONTROL_STATUS_LINK_DATA[0].linkState != CONTROL_LINK_STATE_UP) return CONTROL_STATUS_ERROR;
	uint32_t messageSize = strlen(message);
	control_status_message_t messageData;
	if(messageSize > CONTROL_BUFFER_SIZE) return CONTROL_STATUS_ERROR;
	memcpy(messageData.message, message, messageSize);
	messageData.messageSize = messageSize;
	messageData.type = type;
	if(xQueueSend(prvCONTROL_STATUS_LINK_DATA[0].messageQueue,&messageData,timeout) != pdPASS) return CONTROL_STATUS_ERROR;
	return CONTROL_STATUS_OK;
}

control_status_t 	CONTROL_StatusLinkSendMessageFromISR(const char* message, contol_status_message_type_t type, uint32_t timeout)
{
	if(prvCONTROL_STATUS_LINK_DATA[0].linkState != CONTROL_LINK_STATE_UP) return CONTROL_STATUS_ERROR;
	uint32_t messageSize = strlen(message);
	control_status_message_t messageData;
	BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
	if(messageSize > CONTROL_BUFFER_SIZE) return CONTROL_STATUS_ERROR;
	memcpy(messageData.message, message, messageSize);
	messageData.messageSize = messageSize;
	messageData.type = type;
	if(xQueueSendFromISR(prvCONTROL_STATUS_LINK_DATA[0].messageQueue,&messageData, &pxHigherPriorityTaskWoken) != pdPASS) return CONTROL_STATUS_ERROR;
	portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
	return CONTROL_STATUS_OK;
}
/**
 * @}
 */
/**
 * @}
 */
