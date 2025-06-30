/**
 ******************************************************************************
 * @file    logging.c
 *
 * @brief   Logging service is responsible for collecting, formatting, and
 *          transmitting log messages from various system components via UART.
 *          It runs as a FreeRTOS task and provides a thread-safe interface
 *          for writing formatted log messages with different severity levels.
 *
 * @author  Haris Turkmanovic
 * @email   haris.turkmanovic@gmail.com
 * @date    November 2022
 ******************************************************************************
 */
#include 	<stdarg.h>
#include 	<string.h>

#include 	"FreeRTOS.h"
#include 	"task.h"
#include 	"queue.h"
#include 	"semphr.h"


#include	"logging.h"
#include 	"system.h"
#include 	"drv_uart.h"
/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup LOGGING_SERVICE Logging service
 * @{
 */

/**
 * @defgroup LOGGING_PRIVATE_TYPES Logging private types
 * @{
 */

/**
 * @brief Function pointer type for sending log messages through different channels
 */
typedef	uint8_t (*sendLogMessageChannel)(uint8_t* message, uint32_t size);
/**
 * @}
 */

/**
 * @defgroup LOGGING_PRIVATE_STRUCTURES Logging private structures
 * @{
 */
/**
 * @brief Main logging service data structure
 */
typedef struct
{
	logging_state_t					state;				/**< Current state of the logging service */
	QueueHandle_t					txMsgQueue;			/**< Queue for pending log messages */
	SemaphoreHandle_t				initSig;			/**< Semaphore for signaling initialization completion */
	logging_initialization_status_t	channelInitStatus;	/**< Channel initialization status */
	TaskHandle_t					taskHandle;			/**< FreeRTOS task handle */
}logging_data_t;
/**
 * @brief Structure representing a log message in the internal queue
 */
typedef struct
{
	uint8_t							buffer[LOGGING_INTERNAL_BUFFER_SIZE];	/**< Message buffer */
	uint32_t						size;									/**< Message size in bytes */
}logging_message_t;
/**
 * @}
 */

/**
 * @defgroup LOGGING_PRIVATE_DATA Logging private data
 * @{
 */
/**
 * @brief Static instance of the logging service data
 */
static	logging_data_t				prvLOGGING_DATA;
/**
 * @}
 */

/**
 * @defgroup LOGGING_PRIVATE_FUNCTIONS Logging service private functions
 * @{
 */

/**
 * @brief Initialize logging communication channels.
 *
 * Configures the UART interface used for log message transmission.
 * Sets up UART3 with 115200 baud rate, no parity, and 1 stop bit.
 *
 * @return LOGGING_STATUS_OK on success, LOGGING_STATUS_ERROR on failure
 */
static logging_status_t	prvLOGGING_InitChannels()
{
	drv_uart_config_t channelConfig;

	channelConfig.baudRate = 115200;
	channelConfig.parityEnable = DRV_UART_PARITY_NONE;
	channelConfig.stopBitNo	= DRV_UART_STOPBIT_1;

	if(DRV_UART_Instance_Init(DRV_UART_INSTANCE_3, &channelConfig) != DRV_UART_STATUS_OK) return LOGGING_STATUS_ERROR;
	prvLOGGING_DATA.channelInitStatus = LOGGING_INITIALIZATION_STATUS_INIT;
	return LOGGING_STATUS_OK;
}
/**
 * @brief Send a log message through the configured communication channel.
 *
 * Handles both pre-scheduler and post-scheduler operation modes.
 * If the scheduler hasn't started yet, initializes channels on first use.
 * Transmits the message via UART3 with a configured timeout.
 *
 * @param[in] buffer Pointer to message buffer
 * @param[in] size Size of message in bytes
 * @return LOGGING_STATUS_OK on success, LOGGING_STATUS_ERROR on failure
 */
static logging_status_t prvLOGGING_SendLogMessage(uint8_t* buffer, uint32_t size)
{
	/* Initialize logging channels*/
	if(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED && prvLOGGING_DATA.channelInitStatus == LOGGING_INITIALIZATION_STATUS_NOINIT)
	{
		if(prvLOGGING_InitChannels() != LOGGING_STATUS_OK) return LOGGING_STATUS_ERROR;
	}

	/*Send data over all channels */
	if(DRV_UART_TransferData(DRV_UART_INSTANCE_3, buffer, size, LOGGING_TRANSMIT_TIMEOUT) != DRV_UART_STATUS_OK) return LOGGING_STATUS_ERROR;

	return LOGGING_STATUS_OK;
}
/**
 * @brief Logging service main task function.
 *
 * This is the core task responsible for managing the logging service state machine.
 * The task transitions through multiple states:
 *
 * - `LOGGING_STATE_INIT`: Initializes the UART communication channels
 * - `LOGGING_STATE_SERVICE`: Waits for log messages in the queue and transmits them
 * - `LOGGING_STATE_ERROR`: Reports system error and suspends indefinitely
 *
 * The task processes messages from the queue in FIFO order and handles transmission
 * through the configured UART interface.
 *
 * @param[in] pvParameters Unused task parameter
 */
static void prvLOGGING_TaskFunc(void* pvParameters){
	logging_message_t tmpBuffer;
	for(;;){
		switch(prvLOGGING_DATA.state)
		{
		case LOGGING_STATE_INIT:
			if(prvLOGGING_InitChannels() != LOGGING_STATUS_OK)
			{
				prvLOGGING_DATA.state	= LOGGING_STATE_ERROR;
				break;
			}
			prvLOGGING_DATA.state	= LOGGING_STATE_SERVICE;
			xSemaphoreGive(prvLOGGING_DATA.initSig);
			break;
		case LOGGING_STATE_SERVICE:
			if(xQueueReceive(prvLOGGING_DATA.txMsgQueue, &tmpBuffer, portMAX_DELAY) != pdPASS)
			{
				prvLOGGING_DATA.state	= LOGGING_STATE_ERROR;
				break;
			}
			if(prvLOGGING_SendLogMessage(tmpBuffer.buffer, tmpBuffer.size) != LOGGING_STATUS_OK)
			{
				prvLOGGING_DATA.state	= LOGGING_STATE_ERROR;
				break;
			}
			break;
		case LOGGING_STATE_ERROR:
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(portMAX_DELAY);
			break;
		}
	}
}

logging_status_t LOGGING_Init(uint32_t initTimeout){
	if(xTaskCreate(
			prvLOGGING_TaskFunc,
			LOGGING_TASK_NAME,
			LOGGING_TASK_STACK,
			NULL,
			LOGGING_TASK_PRIO,
			&prvLOGGING_DATA.taskHandle) != pdPASS) return LOGGING_STATUS_ERROR;

	prvLOGGING_DATA.initSig = xSemaphoreCreateBinary();

	if(prvLOGGING_DATA.initSig == NULL) return LOGGING_STATUS_ERROR;

	prvLOGGING_DATA.txMsgQueue = xQueueCreate(LOGGING_QUEUE_LENGTH, sizeof(logging_message_t));

	if(prvLOGGING_DATA.txMsgQueue == NULL) return LOGGING_STATUS_ERROR;

	prvLOGGING_DATA.state = SYSTEM_STATE_INIT;

	if(xSemaphoreTake(prvLOGGING_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return LOGGING_STATUS_ERROR;

	return LOGGING_STATUS_OK;
}

logging_status_t 	LOGGING_Write(char* serviceName, logging_msg_type_t msgType, char* message, ...)
{
    logging_message_t 	tmpMessageBuffer;
    tmpMessageBuffer.size = 0;

    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING && prvLOGGING_DATA.state != LOGGING_STATE_SERVICE) return LOGGING_STATUS_ERROR;

    memset(tmpMessageBuffer.buffer, 0, LOGGING_INTERNAL_BUFFER_SIZE);
    va_list args;
    va_start(args, message);

	memcpy(tmpMessageBuffer.buffer, serviceName, strlen(serviceName));
	tmpMessageBuffer.size += strlen(serviceName);

    switch (msgType)
    {
    case LOGGING_MSG_TYPE_INFO:
    	memcpy(tmpMessageBuffer.buffer + tmpMessageBuffer.size,": ", strlen(": "));
    	tmpMessageBuffer.size += strlen(": ");
        break;
    case LOGGING_MSG_TYPE_WARNING:
    	memcpy(tmpMessageBuffer.buffer + tmpMessageBuffer.size," [WARNNING]: ", strlen(" [WARNNING]: "));
    	tmpMessageBuffer.size += strlen(" [WARNNING]: ");
        break;
    case LOGGING_MSG_TYPE_ERROR:
    	memcpy(tmpMessageBuffer.buffer + tmpMessageBuffer.size," [>>! ERROR !<<]: ", strlen(" [>>! ERROR !<<]: "));
    	tmpMessageBuffer.size += strlen(" [>>! ERROR !<<]: ");
        break;
    }

    tmpMessageBuffer.size += vsprintf(tmpMessageBuffer.buffer + tmpMessageBuffer.size, message, args);
    va_end(args);

    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
    	if(prvLOGGING_SendLogMessage(tmpMessageBuffer.buffer, tmpMessageBuffer.size) != LOGGING_STATUS_OK) return LOGGING_STATUS_ERROR;
    }
    else
    {
        if (xQueueSend(prvLOGGING_DATA.txMsgQueue, &tmpMessageBuffer, 0) != pdPASS)  return LOGGING_STATUS_ERROR;
    }

    return LOGGING_STATUS_OK;
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

