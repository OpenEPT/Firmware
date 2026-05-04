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
#include "fsystem.h"
#include "configuration.h"
#include "network.h"

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
	uint16_t 			serverPort;
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

	memcpy(tmpResponsePtr, "H", 1);
	tmpResponsePtr	+= 1;
	*responseSize	+= 1;

	memcpy(tmpResponsePtr, msg, msgSize);
	tmpResponsePtr	+= msgSize;
	*responseSize	+= msgSize;

	memcpy(tmpResponsePtr, "\r\n", 2);
	tmpResponsePtr	+= 2;
	*responseSize	+= 2;
}

static void inline prvCONTROL_PrepareOkResponseBin(char* response, uint16_t* responseSize, char* msg, uint32_t msgSize)
{
    uint32_t tmpIncreaseSize = 0;
    char* tmpResponsePtr = response;

    /* OK */
    tmpIncreaseSize = strlen(CONTROL_RESPONSE_OK_STATUS_MSG);
    memcpy(tmpResponsePtr, CONTROL_RESPONSE_OK_STATUS_MSG, tmpIncreaseSize);
    tmpResponsePtr += tmpIncreaseSize;
    *responseSize += tmpIncreaseSize;

    /* space */
    memcpy(tmpResponsePtr, " ", 1);
    tmpResponsePtr += 1;
    *responseSize += 1;

    /* B */
    memcpy(tmpResponsePtr, "B", 1);
    tmpResponsePtr += 1;
    *responseSize += 1;

    /* === SIZE (2 bytes, big-endian) === */
    uint16_t payloadSize = (uint16_t)msgSize;

    tmpResponsePtr[0] = (payloadSize >> 8) & 0xFF;
    tmpResponsePtr[1] = (payloadSize >> 0) & 0xFF;

    tmpResponsePtr += 2;
    *responseSize += 2;

    /* payload */
    memcpy(tmpResponsePtr, msg, msgSize);
    tmpResponsePtr += msgSize;
    *responseSize += msgSize;

    /* CRLF */
    memcpy(tmpResponsePtr, "\r\n", 2);
    tmpResponsePtr += 2;
    *responseSize += 2;
}
/**
 * @brief	Get device name from system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
/**
 * @brief	Get DAC
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
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
/**
 * @brief	Get charging status
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
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
/**
 * @brief	Get charging current value
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
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
/**
 * @brief	Set charging termination current value
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
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
/**
 * @brief	Get charging termination current value
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
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
/**
 * @brief	Set charger termination voltage content
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
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
/**
 * @brief	Get charger termination voltage content
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
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

static void prvCONTROL_FSystemFormat(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    if(FSYSTEM_FormatBD(10000) == FSYSTEM_STATUS_OK)
    {
        prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "FSYSTEM BD format executed\r\n");
    }
    else
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "FSYSTEM BD format failed\r\n");
    }
}

static void prvCONTROL_FSystemRead(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    cmparse_value_t value;
    uint32_t offset;
    uint32_t size;

    static uint8_t readBuffer[FSYSTEM_BD_CHUNK_SIZE];
    uint32_t readSize = 0;

    memset(&value, 0, sizeof(cmparse_value_t));
    if(CMPARSE_GetArgValue(arguments, argumentsLength, "offset", &value) != CMPARSE_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        return;
    }
    sscanf(value.value, "%lu", &offset);

    memset(&value, 0, sizeof(cmparse_value_t));
    if(CMPARSE_GetArgValue(arguments, argumentsLength, "size", &value) != CMPARSE_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        return;
    }
    sscanf(value.value, "%lu", &size);

    if(size > FSYSTEM_BD_CHUNK_SIZE)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        return;
    }

    if(FSYSTEM_ReadBDChunk(offset, (char*)readBuffer, size, &readSize, 2000) != FSYSTEM_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "FSYSTEM BD read failed\r\n");
        return;
    }

    /* vracamo RAW podatke direktno */
    prvCONTROL_PrepareOkResponseBin(response, responseSize, (char*)readBuffer, readSize);

    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "FSYSTEM BD read OK (offset=%lu size=%lu)\r\n", offset, readSize);
}

static void prvCONTROL_FSystemWrite(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    cmparse_value_t value;
    cmparse_value_bin_t valueBin;
    uint32_t offset;
    uint32_t size;

    char* dataPtr;
    uint32_t dataSize;

    memset(&value, 0, sizeof(cmparse_value_t));
    if(CMPARSE_GetArgValue(arguments, argumentsLength, "offset", &value) != CMPARSE_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        return;
    }
    sscanf(value.value, "%lu", &offset);

    memset(&value, 0, sizeof(cmparse_value_t));
    if(CMPARSE_GetArgValue(arguments, argumentsLength, "size", &value) != CMPARSE_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        return;
    }
    sscanf(value.value, "%lu", &size);

    memset(&value, 0, sizeof(cmparse_value_t));
    if(CMPARSE_GetArgValueBin(arguments, argumentsLength, "data", &valueBin, size) != CMPARSE_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        return;
    }

    dataPtr  = valueBin.value;
    dataSize = valueBin.size;

    if(dataSize != size)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "FSYSTEM BD write size mismatch\r\n");
        return;
    }

    if(FSYSTEM_WriteBDChunk(offset, dataPtr, &dataSize, 2000) != FSYSTEM_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "FSYSTEM BD write failed\r\n");
        return;
    }

    prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);

    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "FSYSTEM BD write OK (offset=%lu size=%lu)\r\n", offset, size);
}
static void prvCONTROL_FSystemBDSizeGet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    uint32_t bdSize = FSYSTEM_BD_SIZE;

    char sizeStr[16];
    snprintf(sizeStr, sizeof(sizeStr), "%lu", bdSize);

    prvCONTROL_PrepareOkResponse(response, responseSize, sizeStr, strlen(sizeStr));

    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "FSYSTEM BD size get: %lu\r\n", bdSize);
}

static void prvCONTROL_ParamCalGet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    float vref, voff, vcor, coff, ccor;
    uint8_t def;

    char resp[128];
    uint32_t respLen = 0;

    if(CONFIGURATION_GetParameter_Float("CAL_V_REF", &vref, &def) != CONFIGURATION_STATUS_OK ||
       CONFIGURATION_GetParameter_Float("CAL_V_OFF", &voff, &def) != CONFIGURATION_STATUS_OK ||
       CONFIGURATION_GetParameter_Float("CAL_V_COR", &vcor, &def) != CONFIGURATION_STATUS_OK ||
       CONFIGURATION_GetParameter_Float("CAL_C_OFF", &coff, &def) != CONFIGURATION_STATUS_OK ||
       CONFIGURATION_GetParameter_Float("CAL_C_COR", &ccor, &def) != CONFIGURATION_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        return;
    }

    respLen = snprintf(resp, sizeof(resp),
                       "VREF=%.6f VOFF=%.6f VCOR=%.6f COFF=%.6f CCOR=%.6f",
                       vref, voff, vcor, coff, ccor);

    prvCONTROL_PrepareOkResponse(response, responseSize, resp, respLen);

    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO,
                  "Calibration parameters read: VREF=%.6f VOFF=%.6f VCOR=%.6f COFF=%.6f CCOR=%.6f\r\n",
                  vref, voff, vcor, coff, ccor);
}

static void prvCONTROL_ParamCalSet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    cmparse_value_t value;

    float vref, voff, vcor, coff, ccor;

    if(CMPARSE_GetArgValue(arguments, argumentsLength, "vref", &value) != CMPARSE_STATUS_OK ||
       sscanf(value.value, "%f", &vref) != 1)
        goto error;

    if(CMPARSE_GetArgValue(arguments, argumentsLength, "voff", &value) != CMPARSE_STATUS_OK ||
       sscanf(value.value, "%f", &voff) != 1)
        goto error;

    if(CMPARSE_GetArgValue(arguments, argumentsLength, "vcor", &value) != CMPARSE_STATUS_OK ||
       sscanf(value.value, "%f", &vcor) != 1)
        goto error;

    if(CMPARSE_GetArgValue(arguments, argumentsLength, "coff", &value) != CMPARSE_STATUS_OK ||
       sscanf(value.value, "%f", &coff) != 1)
        goto error;

    if(CMPARSE_GetArgValue(arguments, argumentsLength, "ccor", &value) != CMPARSE_STATUS_OK ||
       sscanf(value.value, "%f", &ccor) != 1)
        goto error;


    CONFIGURATION_SetParameter_Float("CAL_V_REF", vref, 1000);
    CONFIGURATION_SetParameter_Float("CAL_V_OFF", voff, 1000);
    CONFIGURATION_SetParameter_Float("CAL_V_COR", vcor, 1000);
    CONFIGURATION_SetParameter_Float("CAL_C_OFF", coff, 1000);
    CONFIGURATION_SetParameter_Float("CAL_C_COR", ccor, 1000);

    prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);


    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO,
                  "Calibration parameters updated: VREF=%.6f VOFF=%.6f VCOR=%.6f COFF=%.6f CCOR=%.6f\r\n",
                  vref, voff, vcor, coff, ccor);
    return;

error:
    prvCONTROL_PrepareErrorResponse(response, responseSize);

	LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR,
				  "Invalid calibration parameters (parsing failed)\r\n");
}

static void prvCONTROL_ParamStore(const char* arguments,
                                  uint16_t argumentsLength,
                                  char* response,
                                  uint16_t* responseSize)
{
    if(CONFIGURATION_StoreToFS(5000) != CONFIGURATION_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);

        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Configuration store to FS failed\r\n");
        return;
    }

    prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);

    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Configuration successfully stored to FS\r\n");
}

static void prvCONTROL_DeviceReset(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Device reset requested\r\n");

    prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);

    vTaskDelay(pdMS_TO_TICKS(500));

    SYSTEM_Restart();
}


static void prvCONTROL_ParamShuntGet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    float value;
    uint8_t def;

    if(CONFIGURATION_GetParameter_Float("SENS_SHUNT", &value, &def) != CONFIGURATION_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR,
                      "Failed to read shunt parameter\r\n");
        return;
    }

    char resp[32];
    int len = snprintf(resp, sizeof(resp), "SHUNT=%.6f", value);

    prvCONTROL_PrepareOkResponse(response, responseSize, resp, len);

    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO,
                  "Shunt parameter read: SHUNT=%.6f\r\n", value);

}
static void prvCONTROL_GetFWVersion(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    char buffer[CONF_CONFIGURATION_MAX_PARAM_VALUESIZE];
    uint32_t size = 0;

    if(SYSTEM_GetFWVersion(buffer, &size) != SYSTEM_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);

        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR,
                      "Unable to get FW version\r\n");
        return;
    }

    prvCONTROL_PrepareOkResponse(response, responseSize, buffer, size);

    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO,
                  "FW version read: %s\r\n", buffer);
}
static void prvCONTROL_GetHWSerial(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    char buffer[CONF_CONFIGURATION_MAX_PARAM_VALUESIZE];
    uint32_t size = 0;

    if(SYSTEM_GetDeviceSerial(buffer, &size) != SYSTEM_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);

        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR,
                      "Unable to get HW serial\r\n");
        return;
    }

    prvCONTROL_PrepareOkResponse(response, responseSize, buffer, size);

    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO,
                  "HW serial read: %s\r\n", buffer);
}

static void prvCONTROL_ParamGainGet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    float value;
    uint8_t def;

    if(CONFIGURATION_GetParameter_Float("SENS_GAIN", &value, &def) != CONFIGURATION_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR,
                      "Failed to read gain parameter\r\n");
        return;
    }

    char resp[32];
    int len = snprintf(resp, sizeof(resp), "GAIN=%.6f", value);

    prvCONTROL_PrepareOkResponse(response, responseSize, resp, len);


    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO,
                  "Gain parameter read: GAIN=%.6f\r\n", value);
}

static void prvCONTROL_GetMAC(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    uint8_t mac[6];
    char macStr[32];
    uint32_t len;

    if(NETWORK_GetMACAddr(mac) != NETWORK_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);

        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR,
                      "Unable to get MAC address\r\n");
        return;
    }

    len = snprintf(macStr, sizeof(macStr),
                   "%02X:%02X:%02X:%02X:%02X:%02X",
                   mac[0], mac[1], mac[2],
                   mac[3], mac[4], mac[5]);

    prvCONTROL_PrepareOkResponse(response, responseSize, macStr, len);

    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO,
                  "MAC address read: %s\r\n", macStr);
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
 * @param	responseSize: length of response message
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
/**
 * @brief	Get load state
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
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
 * @param	responseSize: length of response message
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
/**
 * @brief	Get battery state
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
/**
 * @brief	Get device power path by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
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
/**
 * @brief	Get device undercurrent by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
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
static void prvCONTROL_GetUVoltageValue(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    float value = 0;
    char valueString[16];
    uint32_t valueStringLength = 0;

    if(DPCONTROL_GetUVValue(&value, 1000) != DPCONTROL_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get UV value\r\n");
    }
    else
    {
        memset(valueString, 0, sizeof(valueString));
        valueStringLength = sprintf(valueString, "%.3f", value);
        prvCONTROL_PrepareOkResponse(response, responseSize, valueString, valueStringLength);
    }
}
static void prvCONTROL_SetUVoltageValue(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    cmparse_value_t valueArg;
    float value;

    memset(&valueArg, 0, sizeof(cmparse_value_t));

    if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &valueArg) != CMPARSE_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        return;
    }

    sscanf(valueArg.value, "%f", &value);

    if(DPCONTROL_SetUVValue(value, 1000) == DPCONTROL_STATUS_OK)
    {
        prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "UV value %.3f set\r\n", value);
    }
    else
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
    }
}
/**
 * @brief	Get device overvoltage by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
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
static void prvCONTROL_GetOVoltageValue(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    float value = 0;
    char valueString[16];
    uint32_t valueStringLength = 0;

    if(DPCONTROL_GetOVValue(&value, 1000) != DPCONTROL_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get OV value\r\n");
    }
    else
    {
        memset(valueString, 0, sizeof(valueString));
        valueStringLength = sprintf(valueString, "%.3f", value);
        prvCONTROL_PrepareOkResponse(response, responseSize, valueString, valueStringLength);
    }
}
static void prvCONTROL_SetOVoltageValue(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    cmparse_value_t valueArg;
    float value;

    memset(&valueArg, 0, sizeof(cmparse_value_t));

    if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &valueArg) != CMPARSE_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        return;
    }

    sscanf(valueArg.value, "%f", &value);

    if(DPCONTROL_SetOVValue(value, 1000) == DPCONTROL_STATUS_OK)
    {
        prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "OV value %.3f set\r\n", value);
    }
    else
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
    }
}
/**
 * @brief	Get device overcurrent by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
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
static void prvCONTROL_GetOCurrentValue(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    int32_t value = 0;
    char valueString[16];
    uint32_t valueStringLength = 0;

    if(DPCONTROL_GetOCValue(&value, 1000) != DPCONTROL_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get OC value\r\n");
    }
    else
    {
        memset(valueString, 0, sizeof(valueString));
        valueStringLength = sprintf(valueString, "%ld", value);
        prvCONTROL_PrepareOkResponse(response, responseSize, valueString, valueStringLength);
    }
}
static void prvCONTROL_SetOCurrentValue(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
    cmparse_value_t valueArg;
    int32_t value;

    memset(&valueArg, 0, sizeof(cmparse_value_t));

    if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &valueArg) != CMPARSE_STATUS_OK)
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
        return;
    }

    sscanf(valueArg.value, "%ld", &value);

    if(DPCONTROL_SetOCValue(value, 1000) == DPCONTROL_STATUS_OK)
    {
        prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
        LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "OC value %ld set\r\n", value);
    }
    else
    {
        prvCONTROL_PrepareErrorResponse(response, responseSize);
    }
}
/**
 * @brief	Trigger protection latch
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @brief	Enable or disable battery
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	argumentsLength: length of response message
 * @retval	void
 */
static void prvCONTROL_AddWaveChunk(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
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
	if(DPCONTROL_AddWaveChunk(value.value, value.size, 1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Wave chunk successfully added\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to add wave chunk\r\n");
		return;
	}
}

static void prvCONTROL_WaveCounterSet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	cmparse_value_t				value;
	int						counter;
	memset(&value, 0, sizeof(cmparse_value_t));
	if(CMPARSE_GetArgValue(arguments, argumentsLength, "value", &value) != CMPARSE_STATUS_OK)
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to obtain enable value\r\n");
		return;
	}
	sscanf(value.value, "%d", &counter);

	if(DPCONTROL_SetWaveCounter(counter, 1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Wave counter %d set\r\n", counter);
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set Wave counter\r\n");
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
static void prvCONTROL_WaveChunkSet(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
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

	if(DPCONTROL_SetWaveState(enableStatus, 1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Wave state set\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to set wave state\r\n");
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
static void prvCONTROL_WaveClear(const char* arguments, uint16_t argumentsLength, char* response, uint16_t* responseSize)
{
	if(DPCONTROL_ClearWave(1000) == DPCONTROL_STATUS_OK)
	{
		prvCONTROL_PrepareOkResponse(response, responseSize, "OK", 2);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "Wave cleared\r\n");
	}
	else
	{
		prvCONTROL_PrepareErrorResponse(response, responseSize);
		LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to clear wave\r\n");
		return;
	}
}

/**
 * @brief	Set DAC value
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
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
/**
 * @brief	Get device DAC value by utilizing system service
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
/**
 * @brief	Create EP link
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
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
/**
 * @brief	Create samples streaming
 * @param	arguments: arguments defined within control message
 * @param	argumentsLength: arguments message length
 * @param	response: response message content
 * @param	responseSize: length of response message
 * @retval	void
 */
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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
 * @param	responseSize: length of response message
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

	if(CONTROL_StatusLinkCreate(&statusLinkInstance, connectionInfo, 5000) != CONTROL_STATUS_OK)
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
 * @param	responseSize: length of response message
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
	uint8_t 		def;
	int32_t 		port;

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

			if(CONFIGURATION_GetParameter_Int("CONTROL_SER_PORT", &port, &def) != CONFIGURATION_STATUS_OK)
			{
			    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Unable to get control port\r\n");
			    prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
			    break;
			}

			/* Optional safety */
			if(port <= 0 || port > 65535)
			{
			    LOGGING_Write("Control Service", LOGGING_MSG_TYPE_ERROR, "Invalid control port\r\n");
			    prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
			    break;
			}

			prvCONTROL_DATA.serverPort = (uint16_t)port;


			address.sin_family = AF_INET;
			address.sin_port = htons(prvCONTROL_DATA.serverPort);
			address.sin_addr.s_addr = INADDR_ANY;

			err = bind(sock, (struct sockaddr *)&address, sizeof (address));
			if(err < 0){
				LOGGING_Write("Control Service",LOGGING_MSG_TYPE_ERROR,  "There is a problem to bind TCP socket\r\n");
				prvCONTROL_DATA.state = CONTROL_STATE_ERROR;
				break;
			}
			LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO, "TCP Connection successfully bound to port %d\r\n", prvCONTROL_DATA.serverPort);

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
			LOGGING_Write("Control Service", LOGGING_MSG_TYPE_INFO,"Wait for new connection on port %d\r\n", prvCONTROL_DATA.serverPort);

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
				LOGGING_Write("Control Service", LOGGING_MSG_TYPE_WARNING,  "Unable to set socket option\r\n");
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
					LOGGING_Write("Control Service", LOGGING_MSG_TYPE_WARNING, "There is error during control message parsing procedure\r\n");
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
					LOGGING_Write("Control Service", LOGGING_MSG_TYPE_WARNING, "There is a problem to send message\r\n");
				}
				/* Reinit buffers */
				memset(prvCONTROL_DATA.requestBuffer, 	0, CONTROL_BUFFER_SIZE);
				memset(prvCONTROL_DATA.responseBuffer, 	0, CONTROL_BUFFER_SIZE);
				prvCONTROL_DATA.responseBufferSize = 0;
			}
			LOGGING_Write("Control Service", LOGGING_MSG_TYPE_WARNING,  "Connection closed\r\n");
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
				prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].state = CONTROL_STATE_ERROR;
				break;
			}

			LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_INFO,  "Device is successfully connected to status link server\r\n");
			prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].linkState = CONTROL_LINK_STATE_UP;

			if(xSemaphoreGive(prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].initSig) != pdTRUE)
			{
				LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_ERROR,  "There is a problem with init semaphore\r\n");
				prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].state = CONTROL_STATE_ERROR;
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
				prvCONTROL_STATUS_LINK_DATA[linkInstance.linkInstanceNo].state = CONTROL_STATE_ERROR;
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
				LOGGING_Write("Control Service (Status)", LOGGING_MSG_TYPE_WARNING,  "Unable to send status message\r\n");
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
	CMPARSE_AddCommand("device reset",                  prvCONTROL_DeviceReset);
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

	CMPARSE_AddCommand("device wave chunk add", 		prvCONTROL_AddWaveChunk);
	CMPARSE_AddCommand("device wave counter set", 		prvCONTROL_WaveCounterSet);
	CMPARSE_AddCommand("device wave state set", 		prvCONTROL_WaveChunkSet);
	CMPARSE_AddCommand("device wave clear", 			prvCONTROL_WaveClear);


	CMPARSE_AddCommand("device uvoltage state get", 	prvCONTROL_GetUVoltage);
	CMPARSE_AddCommand("device ovoltage state get", 	prvCONTROL_GetOVoltage);
	CMPARSE_AddCommand("device ocurrent state get", 	prvCONTROL_GetOCurrent);
	CMPARSE_AddCommand("device uvoltage value get", 	prvCONTROL_GetUVoltageValue);
	CMPARSE_AddCommand("device uvoltage value set", 	prvCONTROL_SetUVoltageValue);

	CMPARSE_AddCommand("device ovoltage value get", 	prvCONTROL_GetOVoltageValue);
	CMPARSE_AddCommand("device ovoltage value set", 	prvCONTROL_SetOVoltageValue);

	CMPARSE_AddCommand("device ocurrent value get", 	prvCONTROL_GetOCurrentValue);
	CMPARSE_AddCommand("device ocurrent value set", 	prvCONTROL_SetOCurrentValue);

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

	CMPARSE_AddCommand("device fsystem bd format", 			prvCONTROL_FSystemFormat);
	CMPARSE_AddCommand("device fsystem bd read",   			prvCONTROL_FSystemRead);
	CMPARSE_AddCommand("device fsystem bd write",  			prvCONTROL_FSystemWrite);
	CMPARSE_AddCommand("device fsystem bd size get",  		prvCONTROL_FSystemBDSizeGet);


	CMPARSE_AddCommand("device param cal get",  			prvCONTROL_ParamCalGet);
	CMPARSE_AddCommand("device param cal set",  			prvCONTROL_ParamCalSet);
	CMPARSE_AddCommand("device param shunt get",  			prvCONTROL_ParamShuntGet);
	CMPARSE_AddCommand("device param gain get",  			prvCONTROL_ParamGainGet);

	CMPARSE_AddCommand("device ipinfo mac get", 			prvCONTROL_GetMAC);
	CMPARSE_AddCommand("device hwserial get",  				prvCONTROL_GetHWSerial);
	CMPARSE_AddCommand("device swserial get",  				prvCONTROL_GetFWVersion);

	CMPARSE_AddCommand("device param store",  				prvCONTROL_ParamStore);


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
/**
 * @}
 */
