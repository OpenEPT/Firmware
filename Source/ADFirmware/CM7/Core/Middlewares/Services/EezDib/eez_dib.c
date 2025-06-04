/*
 * eez_dib.c
 *
 *  Created on: May 15, 2025
 *      Author: Haris Turkmanovic & Dimitrije Lilic
 */

#include <stdlib.h>
#include "eez_dib.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "drv_spi.h"
#include "drv_gpio.h"

#include "system.h"
#include "logging.h"
#include "sstream.h"

#define MSG_LEN 10

#define EEZ_DIB_STATUS_ACQ_START		0x01

typedef struct
{
    uint8_t         data[MSG_LEN];
    uint8_t         size;
}eez_dib_msg_t;

typedef struct
{
	eez_dib_state_t 			mainTaskState;
	uint8_t						statusData;
	eez_dib_acq_state_t			acqState;
	sstream_connection_info* 	streamInfo;
	SemaphoreHandle_t			guard;
	uint8_t					    buffer[16];
} eez_dib_data_t;

static 	TaskHandle_t 			prvEEZ_DIB_TASK_HANDLE;
static  QueueHandle_t			prvEEZ_DIB_QUEUE_ID;

static eez_dib_data_t			prvEEZ_DIB_DATA;

static uint8_t					prvEEZ_DIB_DATAIN[MSG_LEN] __attribute__((section(".ADCSamplesBufferSPI")));
static uint8_t					prvEEZ_DIB_DATAOUT[MSG_LEN] __attribute__((section(".ADCSamplesBufferSPI")));


static void prvEEZDIB_Communication(uint8_t* data)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;


	/* If the termination character is received, process the message */
	if (prvEEZ_DIB_DATAIN[5] == '\r')
	{
		/* Send the pointer to the message into the queue */
		if(xQueueSendFromISR(prvEEZ_DIB_QUEUE_ID, prvEEZ_DIB_DATAIN, &xHigherPriorityTaskWoken) != pdTRUE)
		{
			while(1);
		}

		ITM_SendChar('h');

		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}

	/* Reset index to start a new message */
	//memset(prvEEZ_DIB_DATA.input.data, 0, MSG_LEN);
	memset(prvEEZ_DIB_DATAIN, 0, MSG_LEN);
}

static void prvEEZ_DIB_Task()
{
		LOGGING_Write("Eez Dib", LOGGING_MSG_TYPE_INFO, "Eez Dib service started\r\n");
		drv_spi_config_t channelConfig;
		eez_dib_msg_t msgTmp;

		uint32_t counter = 0;
		eez_dib_acq_state_t lastAcqState;

		uint8_t streamID;

		for(;;)
		{
			switch(prvEEZ_DIB_DATA.mainTaskState)
			{
			case EEZ_DIB_STATE_INIT:

				channelConfig.mode = DRV_SPI_MODE_SLAVE;
				channelConfig.polarity = DRV_SPI_POLARITY_LOW;
				channelConfig.phase	= DRV_SPI_PHASE_1EDGE;

				// Initialize SPI2
				if(DRV_SPI_Instance_Init(DRV_SPI_INSTANCE2, &channelConfig) != DRV_SPI_STATUS_OK)
				{
					LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_ERROR, "Unable to initialize serial port\r\n");
					prvEEZ_DIB_DATA.mainTaskState = EEZ_DIB_STATE_ERROR;
					break;
				}

				if(DRV_SPI_Instance_RegisterRxCallback(DRV_SPI_INSTANCE2, prvEEZDIB_Communication)!= DRV_SPI_STATUS_OK)
				{
					LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_ERROR, "Unable to initialize serial port\r\n");
					prvEEZ_DIB_DATA.mainTaskState = EEZ_DIB_STATE_ERROR;
					break;
				}

				LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_INFO, "Eez Dib service successfully initialized\r\n");


				// Start slave receive FIRST with proper buffer
				if(DRV_SPI_EnableITData(DRV_SPI_INSTANCE2, prvEEZ_DIB_DATAIN, prvEEZ_DIB_DATAOUT, MSG_LEN) != DRV_SPI_STATUS_OK) {
					LOGGING_Write("Test", LOGGING_MSG_TYPE_ERROR, "Slave RX failed\r\n");
					break;
				}

				prvEEZ_DIB_DATA.mainTaskState = EEZ_DIB_STATE_SERVICE;


				break;
			case EEZ_DIB_STATE_SERVICE:

				ITM_SendChar('s');
				if(xQueueReceive(prvEEZ_DIB_QUEUE_ID, &msgTmp, portMAX_DELAY) != pdTRUE)
				{
					while(1);
				}

				ITM_SendChar('a');
//				LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_INFO, "New MSG Received over SPI\r\n");
//				LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_INFO, msgTmp.data);
				if(xSemaphoreTake(prvEEZ_DIB_DATA.guard, 0) != pdTRUE)
				{
					LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_ERROR, "Unable to get resource\r\n");
					prvEEZ_DIB_DATA.mainTaskState = EEZ_DIB_STATE_ERROR;
					break;
				}

				lastAcqState = prvEEZ_DIB_DATA.acqState;

				if(xSemaphoreGive(prvEEZ_DIB_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_ERROR, "Unable to release resource\r\n");
					prvEEZ_DIB_DATA.mainTaskState = EEZ_DIB_STATE_ERROR;
					break;
				}
				if(lastAcqState == EEZ_DIB_ACQUISIIION_STATE_ACTIVE)
				{
					prvEEZ_DIB_DATAOUT[4] |= EEZ_DIB_STATUS_ACQ_START;
					if(SSTREAM_GetLastSamples(prvEEZ_DIB_DATA.streamInfo, prvEEZ_DIB_DATA.buffer, 4, 0) == SSTREAM_STATUS_OK)
					{
						prvEEZ_DIB_DATAOUT[0] = prvEEZ_DIB_DATA.buffer[9];
						prvEEZ_DIB_DATAOUT[1] = prvEEZ_DIB_DATA.buffer[8];
						prvEEZ_DIB_DATAOUT[2] = prvEEZ_DIB_DATA.buffer[1];
						prvEEZ_DIB_DATAOUT[3] = prvEEZ_DIB_DATA.buffer[0];
					}
					else
					{
						prvEEZ_DIB_DATAOUT[0] = 0;
						prvEEZ_DIB_DATAOUT[1] = 0;
						prvEEZ_DIB_DATAOUT[2] = 0;
						prvEEZ_DIB_DATAOUT[3] = 0;
					}
				}
				else
				{
					prvEEZ_DIB_DATAOUT[4] &= ~EEZ_DIB_STATUS_ACQ_START;
					prvEEZ_DIB_DATAOUT[0] = 0xAA;
					prvEEZ_DIB_DATAOUT[1] = 0xBB;
					prvEEZ_DIB_DATAOUT[2] = 0xCC;
					prvEEZ_DIB_DATAOUT[3] = 0xDD;
				}

				ITM_SendChar('r');

				counter += 4;

//				if(DRV_SPI_EnableITData(DRV_SPI_INSTANCE2,
//						prvEEZ_DIB_DATA.input.data,
//						prvEEZ_DIB_DATA.output.data, MSG_LEN) != DRV_SPI_STATUS_OK)
//				{
//					while(1);
//				}

				ITM_SendChar('i');
				break;

			case EEZ_DIB_STATE_ERROR:
				SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
				vTaskDelay(portMAX_DELAY);
				break;
			}

		}
}

eez_dib_status_t EEZ_DIB_Init(uint32_t timeout)
{
    // Create the main task
	memset(&prvEEZ_DIB_DATA, 0, sizeof(eez_dib_data_t));
	memset(prvEEZ_DIB_DATAIN, 0, MSG_LEN);
	memset(prvEEZ_DIB_DATAOUT, 0, MSG_LEN);

    prvEEZ_DIB_QUEUE_ID =  xQueueCreate(
    		EEZ_DIB_ID_QUEUE_LENTH,
			sizeof(eez_dib_msg_t));

    prvEEZ_DIB_DATA.guard = xSemaphoreCreateMutex();

    if(prvEEZ_DIB_DATA.guard == NULL)  return EEZ_DIB_STATE_ERROR;

    if(prvEEZ_DIB_QUEUE_ID == NULL ) return EEZ_DIB_STATE_ERROR;

    if(xTaskCreate(prvEEZ_DIB_Task,
            EEZ_DIB_TASK_NAME,
            EEZ_DIB_STACK_SIZE,
            NULL,
            EEZ_DIB_TASK_PRIO,
            &prvEEZ_DIB_TASK_HANDLE) != pdTRUE) return EEZ_DIB_STATUS_ERROR;

    return EEZ_DIB_STATUS_OK;
}

eez_dib_status_t EEZ_DIB_SetAcquisitionState(eez_dib_acq_state_t acqState, uint8_t streamID, uint32_t timeout)
{
	if(xSemaphoreTake(prvEEZ_DIB_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return EEZ_DIB_STATUS_ERROR;

	prvEEZ_DIB_DATA.acqState = acqState;
	SSTREAM_GetConnectionByID(&prvEEZ_DIB_DATA.streamInfo, streamID);

	if(xSemaphoreGive(prvEEZ_DIB_DATA.guard) != pdTRUE) return EEZ_DIB_STATUS_ERROR;

	return EEZ_DIB_STATUS_ERROR;
}

