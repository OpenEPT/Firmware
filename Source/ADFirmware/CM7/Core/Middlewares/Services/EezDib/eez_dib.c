/**
 ******************************************************************************
 * @file    eez_dib.c
 *
 * @brief   EEZ DIB service is responsible for communication with the main
 *          EEZ DIB microcontroller via SPI. It handles incoming messages,
 *          manages acquisition state, and transmits sampled data using a
 *          buffered protocol.
 *          It runs as a FreeRTOS task and processes messages received through
 *          SPI interrupts.
 *
 * @author  Haris Turkmanovic & Dimitrije Lilic
 * @date    May 2025
 ******************************************************************************
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

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup EEZ_DIB_SERVICE EEZ DIB Service
 * @{
 */

/**
 * @defgroup EEZ_DIB_PRIVATE_DEFINES EEZ DIB private defines
 * @{
 */
#define MSG_LEN                         10
#define EEZ_DIB_STATUS_ACQ_START        0x01
/**
 * @}
 */

/**
 * @defgroup EEZ_DIB_PRIVATE_STRUCTURES EEZ DIB private structures
 * @{
 */
typedef struct {
    uint8_t data[MSG_LEN];      /**< Data content of the received message */
    uint8_t size;               /**< Actual message size */
} eez_dib_msg_t;

/**
 * @brief Structure holding internal EEZ DIB state
 */
typedef struct {
    eez_dib_state_t mainTaskState;               /**< Current task state */
    uint8_t statusData;                          /**< Status flags */
    eez_dib_acq_state_t acqState;                /**< Acquisition state */
    sstream_connection_info* streamInfo;         /**< Pointer to stream info */
    SemaphoreHandle_t guard;                     /**< Mutex for thread safety */
    uint8_t buffer[16];                          /**< Temporary data buffer */
} eez_dib_data_t;
/**
 * @}
 */

/**
 * @defgroup EEZ_DIB_PRIVATE_DATA EEZ DIB private data
 * @{
 */
static TaskHandle_t prvEEZ_DIB_TASK_HANDLE;
static QueueHandle_t prvEEZ_DIB_QUEUE_ID;
static eez_dib_data_t prvEEZ_DIB_DATA;

static uint8_t prvEEZ_DIB_DATAIN[MSG_LEN] __attribute__((section(".ADCSamplesBufferSPI")));
static uint8_t prvEEZ_DIB_DATAOUT[MSG_LEN] __attribute__((section(".ADCSamplesBufferSPI")));
/**
 * @}
 */

/**
 * @defgroup EEZ_DIB_PRIVATE_FUNCTIONS EEZ DIB private functions
 * @{
 */

/**
 * @brief SPI RX callback for processing incoming EEZ DIB messages.
 *
 * This function is triggered by an SPI interrupt when data is received.
 * It checks if the termination character is present, and if so, forwards
 * the received buffer to the main EEZ DIB task via queue.
 *
 * @param[in] data Unused input pointer.
 */
static void prvEEZDIB_Communication(uint8_t* data)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (prvEEZ_DIB_DATAIN[5] == '\r') {
        if (xQueueSendFromISR(prvEEZ_DIB_QUEUE_ID, prvEEZ_DIB_DATAIN, &xHigherPriorityTaskWoken) != pdTRUE) {
            while(1); // Fatal error handling
        }
        ITM_SendChar('h');
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

    memset(prvEEZ_DIB_DATAIN, 0, MSG_LEN);
}

/**
 * @brief Main EEZ DIB task responsible for SPI message processing.
 *
 * The task initializes SPI, registers callbacks, and continuously waits for
 * messages from the ISR. Based on the acquisition state, it formats outgoing
 * data accordingly.
 */
static void prvEEZ_DIB_Task()
{
    LOGGING_Write("Eez Dib", LOGGING_MSG_TYPE_INFO, "Eez Dib service started\r\n");

    drv_spi_config_t channelConfig;
    eez_dib_msg_t msgTmp;
    eez_dib_acq_state_t lastAcqState;

    for (;;)
    {
        switch (prvEEZ_DIB_DATA.mainTaskState)
        {
        case EEZ_DIB_STATE_INIT:
            channelConfig.mode = DRV_SPI_MODE_SLAVE;
            channelConfig.polarity = DRV_SPI_POLARITY_LOW;
            channelConfig.phase = DRV_SPI_PHASE_1EDGE;

            if (DRV_SPI_Instance_Init(DRV_SPI_INSTANCE2, &channelConfig) != DRV_SPI_STATUS_OK ||
                DRV_SPI_Instance_RegisterRxCallback(DRV_SPI_INSTANCE2, prvEEZDIB_Communication) != DRV_SPI_STATUS_OK)
            {
                LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_ERROR, "Unable to initialize serial port\r\n");
                prvEEZ_DIB_DATA.mainTaskState = EEZ_DIB_STATE_ERROR;
                break;
            }

            if (DRV_SPI_EnableITData(DRV_SPI_INSTANCE2, prvEEZ_DIB_DATAIN, prvEEZ_DIB_DATAOUT, MSG_LEN) != DRV_SPI_STATUS_OK) {
                LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_ERROR, "Slave RX failed\r\n");
                break;
            }

            LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_INFO, "Eez Dib service successfully initialized\r\n");
            prvEEZ_DIB_DATA.mainTaskState = EEZ_DIB_STATE_SERVICE;
            break;

        case EEZ_DIB_STATE_SERVICE:
            if (xQueueReceive(prvEEZ_DIB_QUEUE_ID, &msgTmp, portMAX_DELAY) != pdTRUE)
            {
                LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_ERROR, "Message Queue is full\r\n");
                prvEEZ_DIB_DATA.mainTaskState = EEZ_DIB_STATE_ERROR;
                break;
            }

            if (xSemaphoreTake(prvEEZ_DIB_DATA.guard, 0) != pdTRUE)
            {
                LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_ERROR, "Unable to get resource\r\n");
                prvEEZ_DIB_DATA.mainTaskState = EEZ_DIB_STATE_ERROR;
                break;
            }

            lastAcqState = prvEEZ_DIB_DATA.acqState;

            if (xSemaphoreGive(prvEEZ_DIB_DATA.guard) != pdTRUE)
            {
                LOGGING_Write("Eez Dib service", LOGGING_MSG_TYPE_ERROR, "Unable to release resource\r\n");
                prvEEZ_DIB_DATA.mainTaskState = EEZ_DIB_STATE_ERROR;
                break;
            }

            if (lastAcqState == EEZ_DIB_ACQUISIIION_STATE_ACTIVE)
            {
                prvEEZ_DIB_DATAOUT[4] |= EEZ_DIB_STATUS_ACQ_START;
                if (SSTREAM_GetLastSamples(prvEEZ_DIB_DATA.streamInfo, prvEEZ_DIB_DATA.buffer, 4, 0) == SSTREAM_STATUS_OK)
                {
                    prvEEZ_DIB_DATAOUT[0] = prvEEZ_DIB_DATA.buffer[9];
                    prvEEZ_DIB_DATAOUT[1] = prvEEZ_DIB_DATA.buffer[8];
                    prvEEZ_DIB_DATAOUT[2] = prvEEZ_DIB_DATA.buffer[1];
                    prvEEZ_DIB_DATAOUT[3] = prvEEZ_DIB_DATA.buffer[0];
                }
                else
                {
                    memset(prvEEZ_DIB_DATAOUT, 0, 4);
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

            prvEEZ_DIB_DATAOUT[5] = 0xA5;
            prvEEZ_DIB_DATAOUT[6] = 0xA5;
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
/**
 * @}
 */
/**
 * @}
 */
/**
 * @}
 */

