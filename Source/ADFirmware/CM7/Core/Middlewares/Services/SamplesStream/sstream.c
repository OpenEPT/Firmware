/**
 ******************************************************************************
 * @file   	sstream.c
 *
 * @brief   Samples Stream service implementation provides high-speed ADC data 
 *          acquisition and streaming capabilities over network connections.
 *          This file implements the core functionality for configuring ADC 
 *          channels, managing acquisition parameters, and transmitting 
 *          sample data over UDP.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	February 2024
 ******************************************************************************
 */
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "lwip.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "logging.h"
#include "system.h"

#include "sstream.h"

#include "drv_ain.h"
#include "drv_gpio.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup SSTREAM_SERVICE Samples Stream service
 * @{
 */

/**
 * @defgroup SSTREAM_DEFINES Samples Stream task defines and default values
 * @{
 */
#define SSTREAM_TASK_START_BIT                       0x00000001  /**< Task notification bit: Start acquisition */
#define SSTREAM_TASK_STOP_BIT                        0x00000002  /**< Task notification bit: Stop acquisition */
#define SSTREAM_TASK_STREAM_BIT                      0x00000004  /**< Task notification bit: Start streaming */
#define SSTREAM_TASK_SET_ADC_RESOLUTION_BIT          0x00000008  /**< Task notification bit: Set ADC resolution */
#define SSTREAM_TASK_SET_ADC_STIME_BIT               0x00000010  /**< Task notification bit: Set ADC sampling time */
#define SSTREAM_TASK_SET_ADC_CLOCK_DIV_BIT           0x00000020  /**< Task notification bit: Set ADC clock divider */
#define SSTREAM_TASK_SET_ADC_CH1_STIME_BIT           0x00000040  /**< Task notification bit: Set channel 1 sampling time */
#define SSTREAM_TASK_SET_ADC_CH2_STIME_BIT           0x00000080  /**< Task notification bit: Set channel 2 sampling time */
#define SSTREAM_TASK_SET_ADC_CH1_OFFSET_BIT          0x00000100  /**< Task notification bit: Set channel 1 offset */
#define SSTREAM_TASK_SET_ADC_CH2_OFFSET_BIT          0x00000200  /**< Task notification bit: Set channel 2 offset */
#define SSTREAM_TASK_SET_ADC_CH1_AVERAGING_RATIO     0x00000400  /**< Task notification bit: Set channel 1 averaging ratio */
#define SSTREAM_TASK_SET_ADC_CH2_AVERAGING_RATIO     0x00000800  /**< Task notification bit: Set channel 2 averaging ratio */
#define SSTREAM_TASK_GET_ADC_CH1_VALUE               0x00001000  /**< Task notification bit: Get channel 1 value */
#define SSTREAM_TASK_GET_ADC_CH2_VALUE               0x00002000  /**< Task notification bit: Get channel 2 value */
#define SSTREAM_TASK_SET_SAMPLES_NO                  0x00004000  /**< Task notification bit: Set number of samples */
/**
 * @}
 */



/**
 * @defgroup SSTREAM_PRIVATE_STRUCTURES Samples Stream private structures
 * @{
 */
/**
 * @brief Stream control task data structure
 */
typedef struct
{
    TaskHandle_t                controlTaskHandle;   /**< Handle for the control task */
    SemaphoreHandle_t           initSig;             /**< Semaphore for initialization signaling */
    SemaphoreHandle_t           guard;               /**< Mutex for thread safety */
    sstream_state_t             state;               /**< Current task state */
    sstream_acquisition_state_t acquisitionState;    /**< Current acquisition state */
    drv_ain_adc_config_t        ainConfig;           /**< ADC configuration parameters */
    uint32_t                    id;                  /**< Connection identifier */
}sstream_control_data_t;

/**
 * @brief Stream data task data structure
 */
typedef struct
{
    TaskHandle_t                streamTaskHandle;    /**< Handle for the stream task */
    SemaphoreHandle_t           initSig;             /**< Semaphore for initialization signaling */
    SemaphoreHandle_t           guard;               /**< Mutex for thread safety */
    sstream_state_t             state;               /**< Current task state */
    sstream_connection_info     connectionInfo;      /**< Network connection information */
    uint32_t                    id;                  /**< Connection identifier */
    uint32_t                    ch1Value;            /**< Latest channel 1 value */
    uint32_t                    ch2Value;            /**< Latest channel 2 value */
    uint16_t                    lastSamples[2][4];   /**< Buffer for the last samples (2 channels, 4 samples each) */
}sstream_stream_data_t;

/**
 * @brief Main service data structure
 */
typedef struct
{
    sstream_control_data_t      controlInfo[CONF_SSTREAM_CONNECTIONS_MAX_NO];  /**< Array of control data for each connection */
    sstream_stream_data_t       streamInfo[CONF_SSTREAM_CONNECTIONS_MAX_NO];   /**< Array of stream data for each connection */
    uint32_t                    activeConnectionsNo;                           /**< Number of active connections */
    sstream_acquistion_state_changed_callback asccb;                           /**< Acquisition state change callback */
}sstream_data_t;

/**
 * @brief ADC packet information structure
 */
typedef struct
{
    uint32_t    address;    /**< Buffer address in memory */
    uint8_t     id;         /**< Packet identifier */
    uint32_t    size;       /**< Size of the packet in bytes */
}sstream_packet_t;
/**
 * @}
 */

/**
 * @defgroup SSTREAM_PRIVATE_DATA Samples Stream private data
 * @{
 */
static sstream_data_t			prvSSTREAM_DATA; /**< Main service data instance */

QueueHandle_t					prvSSTREAM_PACKET_QUEUE;
/**
 * @}
 */
/**
 * @brief ISR callback function for new ADC packet sampling events.
 *
 * This function is called from an interrupt context when a new ADC packet 
 * has been sampled and is ready for processing. It packages the packet 
 * information and sends it to the streaming task queue for UDP transmission.
 *
 * The function performs the following operations:
 * - Sends a debug character 'a' via ITM for tracing
 * - Creates a packet data structure with address, ID, and size information
 * - Queues the packet data using FreeRTOS ISR-safe queue operations
 * - Yields to higher priority tasks if necessary
 *
 * @param[in] packetAddress Memory address where the sampled packet data is stored
 * @param[in] packetID Unique identifier for the packet
 * @param[in] size Size of the packet data in bytes
 */
static void prvSSTREAM_NewPacketSampled(uint32_t packetAddress, uint8_t packetID, uint32_t size)
{
	ITM_SendChar('a');
	BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
	sstream_packet_t		packetData;
	packetData.address = packetAddress;
	packetData.id = packetID;
	packetData.size = size;

	xQueueSendFromISR(prvSSTREAM_PACKET_QUEUE, &packetData, &pxHigherPriorityTaskWoken);

    portYIELD_FROM_ISR( pxHigherPriorityTaskWoken );

}

/**
 * @brief Sample streaming task function for UDP data transmission.
 *
 * This is the core streaming task responsible for receiving sampled ADC packets
 * and transmitting them over UDP to a configured server. The task operates in
 * a state machine with the following states:
 *
 * - `SSTREAM_STATE_INIT`: Performs initialization steps including:
 *   - Creating UDP PCB and establishing connection to server
 *   - Setting up packet queue for inter-task communication
 *   - Initializing sample buffers and RGB status indicators
 *   - Signaling initialization completion via semaphore
 *
 * - `SSTREAM_STATE_SERVICE`: Main operational state that:
 *   - Waits for new packets from the ADC sampling ISR
 *   - Allocates PBUF for network transmission
 *   - Copies latest samples to shared buffer (thread-safe)
 *   - Transmits packet data via UDP
 *   - Returns buffer addresses to ADC driver for reuse
 *   - Provides debug tracing via ITM
 *
 * - `SSTREAM_STATE_ERROR`: Error handling state that reports system errors
 *   and blocks indefinitely
 *
 * @param[in] pvParam Pointer to sstream_stream_data_t connection configuration
 */
static void prvSSTREAM_StreamTaskFunc(void* pvParam)
{
	sstream_packet_t		packetData;
	sstream_stream_data_t   *connectionData;
	connectionData 			= (sstream_stream_data_t*)pvParam;


	struct udp_pcb			*pcb;
	struct ip4_addr 		dest;
	struct pbuf				*p;
	err_t					error;
	unsigned int			testPacketcounter = 0;

	system_rgb_value_t 		rgb;

	uint32_t				samplesToCopy;


	LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Samples stream task created\r\n");
	for(;;)
	{
		switch(connectionData->state)
		{
		case SSTREAM_STATE_INIT:
			pcb = udp_new();
			IP4_ADDR(&dest, connectionData->connectionInfo.serverIp[0],
					connectionData->connectionInfo.serverIp[1],
					connectionData->connectionInfo.serverIp[2],
					connectionData->connectionInfo.serverIp[3]);
			if(udp_connect(pcb, &dest, connectionData->connectionInfo.serverport) != ERR_OK)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to establish connection with stream server\r\n");
			}
			else
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Connection with stream server successfully established\r\n");
			}
			memset(connectionData->lastSamples, 0x00, 2*4);

			if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem with init semaphore\r\n");
				connectionData->state = SSTREAM_STATE_ERROR;
				break;
			}
			prvSSTREAM_PACKET_QUEUE = xQueueCreate(CONF_AIN_MAX_BUFFER_NO, sizeof(sstream_packet_t));
			if(prvSSTREAM_PACKET_QUEUE == NULL)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to create ADC packet queue\r\n");
				connectionData->state = SSTREAM_STATE_ERROR;
				break;
			}
			rgb.red = 0;
			rgb.green = 50;
			rgb.blue = 0;
			SYSTEM_SetRGB(rgb);
			connectionData->state = SSTREAM_STATE_SERVICE;
			break;
		case SSTREAM_STATE_SERVICE:
			xQueueReceive(prvSSTREAM_PACKET_QUEUE, &packetData, portMAX_DELAY);

			p = pbuf_alloc(PBUF_TRANSPORT, packetData.size + (2*DRV_AIN_ADC_BUFFER_OFFSET), PBUF_RAM);

			if(p == NULL)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release create pbuf\r\n");
			}

			memcpy(p->payload, (void*)packetData.address, packetData.size + (2*DRV_AIN_ADC_BUFFER_OFFSET));

			if(xSemaphoreTake(connectionData->guard, 0) == pdTRUE)
			{
				samplesToCopy = packetData.size/4 > 4 ? 4 : packetData.size/4;
				memcpy(connectionData->lastSamples[0], (void*)packetData.address + (2*DRV_AIN_ADC_BUFFER_OFFSET), samplesToCopy*2);
				memcpy(connectionData->lastSamples[1], (void*)packetData.address +
						(2*DRV_AIN_ADC_BUFFER_OFFSET) + packetData.size/2, samplesToCopy*2);
			}
			else
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_WARNING,  "Resource locked\r\n");
			}

			xSemaphoreGive(connectionData->guard);

			//if(testPacketcounter & 0x05){
			error = udp_send(pcb, p);
			//}
			testPacketcounter += 1;

			if(error != ERR_OK)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to send pbuf\r\n");
			}
			if(DRV_AIN_Stream_SubmitAddr(DRV_AIN_ADC_3, packetData.address, packetData.id) != DRV_AIN_STATUS_OK)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "Unable to submit buffer\r\n");
			}

			pbuf_free(p);
			ITM_SendChar('c');
			break;
		case SSTREAM_STATE_ERROR:
			LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Samples stream task is in error state\r\n");
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(portMAX_DELAY);
			break;
		}
	}
}

/**
 * @brief Sample streaming control task function for ADC configuration management.
 *
 * This is the control task responsible for initializing and managing ADC configuration
 * parameters based on task notifications. The task handles both initialization and
 * runtime configuration changes through a comprehensive state machine.
 *
 * - `SSTREAM_STATE_INIT`: Performs extensive initialization including:
 *   - GPIO configuration for acquisition status LED
 *   - ADC resolution, clock divider, and sampling time setup
 *   - Channel-specific configuration (sampling time, averaging ratios)
 *   - Callback registration for packet sampling events
 *   - Calculation of effective sampling parameters
 *
 * - `SSTREAM_STATE_SERVICE`: Main service state that responds to task notifications:
 *   - `SSTREAM_TASK_START_BIT`: Starts ADC acquisition and enables status LED
 *   - `SSTREAM_TASK_STOP_BIT`: Stops ADC acquisition and disables status LED
 *   - `SSTREAM_TASK_SET_ADC_*`: Various ADC parameter configuration commands
 *   - `SSTREAM_TASK_GET_ADC_CH*_VALUE`: Single-shot ADC value reading
 *   - `SSTREAM_TASK_SET_SAMPLES_NO`: Configures samples per channel
 *   - Recalculates sampling time after parameter changes
 *   - Provides acquisition state callbacks to upper layers
 *
 * - `SSTREAM_STATE_ERROR`: Error handling state that reports system errors
 *   and blocks indefinitely
 *
 * The task maintains thread-safe access to shared resources using semaphores
 * and provides comprehensive logging for all operations.
 *
 * @param[in] pvParam Pointer to sstream_control_data_t control configuration
 */
static void prvSSTREAM_ControlTaskFunc(void* pvParam)
{
	uint32_t				notifyValue = 0;
	TickType_t				blockingTime = portMAX_DELAY;

	drv_gpio_pin_init_conf_t 	acqStateLED;
	sstream_control_data_t			*connectionData;
	connectionData 			= (sstream_control_data_t*)pvParam;



	LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Samples stream control task created\r\n");
	for(;;)
	{
		switch(connectionData->state)
		{
		case SSTREAM_STATE_INIT:

			/* Init Acqusition State Diode */
			acqStateLED.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
			acqStateLED.pullState = DRV_GPIO_PIN_PULL_NOPULL;

			if(DRV_GPIO_Port_Init(SSTREAM_LED_PORT) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_WARNING,  "Unable to initialize stream LED port\r\n");
			}
			if(DRV_GPIO_Pin_Init(SSTREAM_LED_PORT, SSTREAM_LED_PIN, &acqStateLED) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_WARNING,  "Unable to initialize stream LED ppin\r\n");
			}

			/* Try to configure default resolution */
			if(DRV_AIN_SetResolution(DRV_AIN_ADC_3, SSTREAM_AIN_DEFAULT_RESOLUTION) == DRV_AIN_STATUS_OK)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "%d bit resolution set\r\n", SSTREAM_AIN_DEFAULT_RESOLUTION);
				connectionData->ainConfig.resolution = SSTREAM_AIN_DEFAULT_RESOLUTION;
			}
			else
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem with AIN resolution setting\r\n");
				connectionData->ainConfig.resolution = DRV_AIN_ADC_RESOLUTION_UNKNOWN;
			}

			/* Try to configure default clok div */
			if(DRV_AIN_SetClockDiv(DRV_AIN_ADC_3, SSTREAM_AIN_DEFAULT_CLOCK_DIV) == DRV_AIN_STATUS_OK)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Clock div resolution set\r\n", SSTREAM_AIN_DEFAULT_CLOCK_DIV);
				connectionData->ainConfig.clockDiv = SSTREAM_AIN_DEFAULT_CLOCK_DIV;
			}
			else
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem with AIN clock div setting\r\n");
				connectionData->ainConfig.resolution = DRV_AIN_ADC_CLOCK_DIV_UNKNOWN;
			}

			/* Try to configure channels default sampling time */
			if(DRV_AIN_SetChannelsSamplingTime(DRV_AIN_ADC_3, SSTREAM_AIN_DEFAULT_CH_SAMPLE_TIME) == DRV_AIN_STATUS_OK)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Channels sampling time %d set\r\n", SSTREAM_AIN_DEFAULT_CH_SAMPLE_TIME);
				connectionData->ainConfig.ch1.sampleTime = SSTREAM_AIN_DEFAULT_CH_SAMPLE_TIME;
				connectionData->ainConfig.ch2.sampleTime = SSTREAM_AIN_DEFAULT_CH_SAMPLE_TIME;
			}
			else
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem with AIN channels sampling time setting\r\n");
				connectionData->ainConfig.ch1.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_UNKNOWN;
				connectionData->ainConfig.ch2.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_UNKNOWN;
			}

			/* Try to configure default avg ratio */
			if(DRV_AIN_SetChannelAvgRatio(DRV_AIN_ADC_3, SSTREAM_AIN_DEFAULT_CH_AVG_RATIO) == DRV_AIN_STATUS_OK)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Channel 1 averaging ration %d set\r\n", SSTREAM_AIN_DEFAULT_CH_AVG_RATIO);
				connectionData->ainConfig.ch1.avgRatio = SSTREAM_AIN_DEFAULT_CH_AVG_RATIO;
			}
			else
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to set default channel 1 averaging ration\r\n");
				connectionData->ainConfig.ch1.avgRatio = 0;
			}

			/*Obtain ADC input clk*/
			if(DRV_AIN_GetADCClk(DRV_AIN_ADC_3, &connectionData->ainConfig.inputClk) == DRV_AIN_STATUS_OK)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "ADC Input clk %d\r\n", connectionData->ainConfig.inputClk);
			}
			else
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "Unable to obtain ADC clock\r\n");
			}

			/* Try to configure default avg ratio */
			if(DRV_AIN_SetSamplesNo(DRV_AIN_ADC_ADS9224R, CONF_SSTREAM_AIN_DEFAULT_SAMPLES_NO) == DRV_AIN_STATUS_OK)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "%d samples per channel set\r\n", CONF_SSTREAM_AIN_DEFAULT_SAMPLES_NO);
				connectionData->ainConfig.samplesNo = CONF_SSTREAM_AIN_DEFAULT_SAMPLES_NO;
			}
			else
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to set samples per channel\r\n");
				connectionData->ainConfig.samplesNo = 0;
			}

			/* Try to configure sampling time */
			if(DRV_AIN_SetSamplingPeriod(DRV_AIN_ADC_3, SSTREAM_AIN_DEFAULT_PERIOD, SSTREAM_AIN_DEFAULT_PRESCALER) == DRV_AIN_STATUS_OK)
			{
				connectionData->ainConfig.prescaler = SSTREAM_AIN_DEFAULT_PRESCALER;
				connectionData->ainConfig.period 	= SSTREAM_AIN_DEFAULT_PERIOD;
				connectionData->ainConfig.samplingTime = (double)1.0/(double)DRV_AIN_ADC_TIM_INPUT_CLK*
						((double)connectionData->ainConfig.prescaler + 1.0)*((double)connectionData->ainConfig.period + 1.0)*
						(double)connectionData->ainConfig.ch1.avgRatio*1000000;
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Sampling time %f set\r\n", connectionData->ainConfig.samplingTime);

			}
			else
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to set sampling time\r\n");
				connectionData->ainConfig.samplingTime = 0;
			}


			DRV_AIN_Stream_RegisterCallback(DRV_AIN_ADC_3, prvSSTREAM_NewPacketSampled);


			if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem with init semaphore\r\n");
				connectionData->state = SSTREAM_STATE_ERROR;
				break;
			}
			connectionData->state = SSTREAM_STATE_SERVICE;
			break;
		case SSTREAM_STATE_SERVICE:
			notifyValue = ulTaskNotifyTake(pdTRUE, blockingTime);
			if(notifyValue & SSTREAM_TASK_START_BIT)
			{
				connectionData->acquisitionState = SSTREAM_ACQUISITION_STATE_ACTIVE;

				if(DRV_AIN_Start(connectionData->ainConfig.adc) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Stream started \r\n");
					DRV_GPIO_Pin_SetState(SSTREAM_LED_PORT, SSTREAM_LED_PIN, DRV_GPIO_PIN_STATE_SET);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "Unable to start stream\r\n");
				}

				notifyValue |= SSTREAM_TASK_STREAM_BIT;


				prvSSTREAM_DATA.asccb(connectionData->id, SSTREAM_ACQUISITION_STATE_ACTIVE);

				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release init semaphore\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}
			if(notifyValue & SSTREAM_TASK_STREAM_BIT)
			{


//				vTaskDelay(pdMS_TO_TICKS(1000));
//
//				TaskToNotifyHandle = xTaskGetCurrentTaskHandle();
//
//				xTaskNotify(TaskToNotifyHandle, SSTREAM_TASK_STREAM_BIT, eSetBits);
			}
			if(notifyValue & SSTREAM_TASK_STOP_BIT)
			{
				connectionData->acquisitionState = SSTREAM_ACQUISITION_STATE_INACTIVE;

				if(DRV_AIN_Stop(DRV_AIN_ADC_3) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Stream stoped \r\n");
					DRV_GPIO_Pin_SetState(SSTREAM_LED_PORT, SSTREAM_LED_PIN, DRV_GPIO_PIN_STATE_RESET);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Unable to stop stream\r\n");
				}

				prvSSTREAM_DATA.asccb(connectionData->id, SSTREAM_ACQUISITION_STATE_INACTIVE);

				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release init semaphore\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}
			if(notifyValue & SSTREAM_TASK_SET_ADC_RESOLUTION_BIT)
			{
				/* Try to configure default resolution */
				if(DRV_AIN_SetResolution(DRV_AIN_ADC_3, connectionData->ainConfig.resolution) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "%d bit resolution set\r\n", connectionData->ainConfig.resolution);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem with AIN resolution setting\r\n");
				}

				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem with ADC configuration\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}
			if(notifyValue & SSTREAM_TASK_SET_ADC_CLOCK_DIV_BIT)
			{
				/* Try to configure ADC clock div */
				if(DRV_AIN_SetClockDiv(DRV_AIN_ADC_3, connectionData->ainConfig.clockDiv) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Clok div %d set\r\n", connectionData->ainConfig.clockDiv);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem with AIN clock div\r\n");
				}
				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release init semaphore\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}
			if(notifyValue & SSTREAM_TASK_SET_ADC_STIME_BIT)
			{
				/* Try to configure ADC clock div */
				if(DRV_AIN_SetSamplingPeriod(DRV_AIN_ADC_3, connectionData->ainConfig.prescaler, connectionData->ainConfig.period) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Sampling time %d set\r\n", connectionData->ainConfig.samplingTime);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to set sampling time\r\n");
				}
				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release init semaphore\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}
			if(notifyValue & SSTREAM_TASK_SET_ADC_CH1_STIME_BIT)
			{
				/* Try to configure ADC channel 0 sampling time */
				if(DRV_AIN_SetChannelsSamplingTime(DRV_AIN_ADC_3, connectionData->ainConfig.ch1.sampleTime) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Sampling time %d set on channel 0\r\n", connectionData->ainConfig.ch1.sampleTime);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to set channel sampling time\r\n");
				}
				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release init semaphore\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}
			if(notifyValue & SSTREAM_TASK_SET_ADC_CH2_STIME_BIT)
			{
				/* Try to configure ADC channel 1 sampling time */
				if(DRV_AIN_SetChannelsSamplingTime(DRV_AIN_ADC_3, connectionData->ainConfig.ch2.sampleTime) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Sampling time %d set on channel 2\r\n", connectionData->ainConfig.ch2.sampleTime);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to set channel sampling time\r\n");
				}
				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release init semaphore\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}
			if(notifyValue & SSTREAM_TASK_SET_ADC_CH1_OFFSET_BIT)
			{
				/* Try to configure ADC channel 1 sampling time */
				if(DRV_AIN_SetChannelOffset(DRV_AIN_ADC_3, 1, connectionData->ainConfig.ch1.offset) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Channel 1 offset %d successfully set\r\n", connectionData->ainConfig.ch1.offset);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "Unable to set channel 1 offset\r\n");
				}
				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release init semaphore\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}
			if(notifyValue & SSTREAM_TASK_SET_ADC_CH2_OFFSET_BIT)
			{
				/* Try to configure ADC channel 1 sampling time */
				if(DRV_AIN_SetChannelOffset(DRV_AIN_ADC_3, 2, connectionData->ainConfig.ch2.offset) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Channel 2 offset %d successfully set\r\n", connectionData->ainConfig.ch2.offset);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "Unable to set channel 1 offset\r\n");
				}
				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release init semaphore\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}
			if(notifyValue & SSTREAM_TASK_SET_ADC_CH1_AVERAGING_RATIO)
			{
				/* Try to configure ADC channel 1 sampling time */
				if(DRV_AIN_SetChannelAvgRatio(DRV_AIN_ADC_3, connectionData->ainConfig.ch1.avgRatio) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Channel 1 averaging ration %d successfully set\r\n", connectionData->ainConfig.ch1.avgRatio);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "Unable to set channel 1 averaging ratio\r\n");
				}
				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release init semaphore\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}
			if(notifyValue & SSTREAM_TASK_GET_ADC_CH1_VALUE)
			{
				/* Try to read ADC channel 1 value */
				uint32_t value;
				if(DRV_AIN_GetADCValue(DRV_AIN_ADC_3, 1, &value) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Channel 1 successfully read value %d\r\n", value);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "Unable read channel 1 \r\n");
				}

				xSemaphoreTake(prvSSTREAM_DATA.streamInfo[connectionData->id].guard, portMAX_DELAY);
				prvSSTREAM_DATA.streamInfo[connectionData->id].ch1Value = value;
				xSemaphoreGive(prvSSTREAM_DATA.streamInfo[connectionData->id].guard);

				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release init semaphore\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}
			if(notifyValue & SSTREAM_TASK_GET_ADC_CH2_VALUE)
			{
				/* Try to read ADC channel 2 value */
				uint32_t value;
				if(DRV_AIN_GetADCValue(DRV_AIN_ADC_3, 2, &value) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Channel 2 successfully read value %d\r\n", value);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "Unable read channel 2 \r\n");
				}

				xSemaphoreTake(prvSSTREAM_DATA.streamInfo[connectionData->id].guard, portMAX_DELAY);
				prvSSTREAM_DATA.streamInfo[connectionData->id].ch2Value = value;
				xSemaphoreGive(prvSSTREAM_DATA.streamInfo[connectionData->id].guard);

				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release init semaphore\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}

			if(notifyValue & SSTREAM_TASK_SET_SAMPLES_NO)
			{
				/* Try to configure default resolution */
				if(DRV_AIN_SetSamplesNo(DRV_AIN_ADC_ADS9224R, connectionData->ainConfig.samplesNo) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "%d samples per channel set\r\n", connectionData->ainConfig.samplesNo);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "Unable to set samples per channel\r\n");
				}

				if(xSemaphoreGive(connectionData->initSig) != pdTRUE)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem with ADC configuration\r\n");
					connectionData->state = SSTREAM_STATE_ERROR;
					break;
				}
			}
			connectionData->ainConfig.samplingTime = (double)1.0/(double)DRV_AIN_ADC_TIM_INPUT_CLK*
									((double)connectionData->ainConfig.prescaler + 1.0)*((double)connectionData->ainConfig.period + 1.0)*
									(double)connectionData->ainConfig.ch1.avgRatio*1000000;
			break;

		case SSTREAM_STATE_ERROR:
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(portMAX_DELAY);
			break;
		}
	}
}

sstream_status_t			SSTREAM_Init(void)
{
	memset(&prvSSTREAM_DATA, 0, sizeof(sstream_data_t));
	return SSTREAM_STATUS_OK;
}
sstream_status_t			SSTREAM_CreateChannel(sstream_connection_info* connectionHandler, uint32_t timeout)
{
	if(prvSSTREAM_DATA.activeConnectionsNo > SSTREAM_CONNECTIONS_MAX_NO) return SSTREAM_STATUS_ERROR;

	uint32_t currentId = prvSSTREAM_DATA.activeConnectionsNo;
	prvSSTREAM_DATA.controlInfo[currentId].id = currentId;
	prvSSTREAM_DATA.streamInfo[currentId].id = currentId;
	memcpy(&prvSSTREAM_DATA.streamInfo[currentId].connectionInfo, connectionHandler,  sizeof(sstream_connection_info));

	prvSSTREAM_DATA.controlInfo[currentId].initSig = xSemaphoreCreateBinary();
	if(prvSSTREAM_DATA.controlInfo[currentId].initSig == NULL) return SSTREAM_STATUS_ERROR;

	prvSSTREAM_DATA.streamInfo[currentId].initSig = xSemaphoreCreateBinary();
	if(prvSSTREAM_DATA.streamInfo[currentId].initSig == NULL) return SSTREAM_STATUS_ERROR;

	prvSSTREAM_DATA.controlInfo[currentId].guard = xSemaphoreCreateMutex();
	if(prvSSTREAM_DATA.controlInfo[currentId].guard == NULL) return SSTREAM_STATUS_ERROR;

	prvSSTREAM_DATA.streamInfo[currentId].guard = xSemaphoreCreateMutex();
	if(prvSSTREAM_DATA.streamInfo[currentId].guard == NULL) return SSTREAM_STATUS_ERROR;


	if(xTaskCreate(
			prvSSTREAM_ControlTaskFunc,
			SSTREAM_CONTROL_TASK_NAME,
			SSTREAM_CONTROL_TASK_STACK_SIZE,
			&prvSSTREAM_DATA.controlInfo[currentId],
			SSTREAM_CONTROL_TASK_PRIO,
			&prvSSTREAM_DATA.controlInfo[currentId].controlTaskHandle) != pdPASS) return SSTREAM_STATUS_ERROR;

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[currentId].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;

	/* Create stream task */
	if(xTaskCreate(
			prvSSTREAM_StreamTaskFunc,
			SSTREAM_STREAM_TASK_NAME,
			SSTREAM_STREAM_TASK_STACK_SIZE,
			&prvSSTREAM_DATA.streamInfo[currentId],
			SSTREAM_STREAM_TASK_PRIO,
			&prvSSTREAM_DATA.streamInfo[currentId].streamTaskHandle) != pdPASS) return SSTREAM_STATUS_ERROR;



	if(xSemaphoreTake(prvSSTREAM_DATA.streamInfo[currentId].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;


	prvSSTREAM_DATA.streamInfo[currentId].connectionInfo.id = currentId;
	prvSSTREAM_DATA.activeConnectionsNo += 1;
	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_GetConnectionByIP(sstream_connection_info* connectionHandler, uint8_t ip[4], uint16_t port)
{
	uint32_t 	connectionIterator = 0;
	uint8_t 	ipIterator = 0;
	while(connectionIterator < prvSSTREAM_DATA.activeConnectionsNo)
	{
		for(ipIterator = 0; ipIterator < 4; ipIterator++){
			if(prvSSTREAM_DATA.streamInfo[connectionIterator].connectionInfo.serverIp[ipIterator] == ip[ipIterator]) continue;
			break;
		}
		if(ipIterator == 4 && prvSSTREAM_DATA.streamInfo[connectionIterator].connectionInfo.serverport == port)
		{
			connectionHandler = &prvSSTREAM_DATA.streamInfo[connectionIterator].connectionInfo;
			return SSTREAM_STATUS_OK;
		}
		connectionIterator += 1;
	}
	return SSTREAM_STATUS_ERROR;

}
sstream_status_t				SSTREAM_GetConnectionByID(sstream_connection_info** connectionHandler, uint32_t id)
{
	uint32_t 	connectionIterator = 0;
	while(connectionIterator < prvSSTREAM_DATA.activeConnectionsNo)
	{
		if(prvSSTREAM_DATA.streamInfo[connectionIterator].connectionInfo.id == id)
		{
			*connectionHandler = &prvSSTREAM_DATA.streamInfo[connectionIterator].connectionInfo;
			return SSTREAM_STATUS_OK;
		}
		connectionIterator += 1;
	}

	return SSTREAM_STATUS_ERROR;
}
sstream_acquisition_state_t		SSTREAM_GetAcquisitionState(sstream_connection_info* connectionHandler, uint32_t timeout)
{
	sstream_acquisition_state_t acqState;
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	acqState = prvSSTREAM_DATA.controlInfo[connectionHandler->id].acquisitionState;
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;
	return acqState;
}
sstream_status_t				SSTREAM_Start(sstream_connection_info* connectionHandler, sstream_adc_t adc, uint32_t timeout)
{

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	switch(adc)
	{
	case SSTREAM_ADC_INTERNAL:
		prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.adc = DRV_AIN_ADC_3;
		break;
	case SSTREAM_ADC_EXTERNAL:
		prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.adc = DRV_AIN_ADC_ADS9224R;
		break;
	}
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;


	if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
			SSTREAM_TASK_START_BIT,
			eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;

	/* Wait until configuration is applied*/
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_StartStream(sstream_connection_info* connectionHandler, uint32_t timeout)
{
	if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
			SSTREAM_TASK_STREAM_BIT,
			eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;

	/* Wait until configuration is applied*/
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_Stop(sstream_connection_info* connectionHandler, uint32_t timeout)
{
	if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
			SSTREAM_TASK_STOP_BIT,
			eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;

	/* Wait until configuration is applied*/
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_SetResolution(sstream_connection_info* connectionHandler, sstream_adc_resolution_t resolution, uint32_t timeout)
{

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.resolution = resolution;
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	/* Send request to configure AIN*/
	if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
			SSTREAM_TASK_SET_ADC_RESOLUTION_BIT,
			eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;

	/* Wait until configuration is applied*/
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_SetSamplesNo(sstream_connection_info* connectionHandler, uint32_t samplesNo, uint32_t timeout)
{

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.samplesNo = samplesNo;
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	/* Send request to configure AIN*/
	if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
			SSTREAM_TASK_SET_SAMPLES_NO,
			eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;

	/* Wait until configuration is applied*/
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_SetSamplingPeriod(sstream_connection_info* connectionHandler, uint32_t prescaller, uint32_t period, uint32_t timeout)
{
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;

	prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.prescaler 		= prescaller;
	prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.period 		= period;
	prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.samplingTime 	= (double)1.0/(double)DRV_AIN_ADC_TIM_INPUT_CLK*
							((double)prescaller + 1.0)*((double)period + 1.0)*
							(double)prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch1.avgRatio*1000000;

	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	/* Send request to configure AIN*/
	if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
			SSTREAM_TASK_SET_ADC_STIME_BIT,
			eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;

	/* Wait until configuration is applied*/
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_SetClkDiv(sstream_connection_info* connectionHandler, sstream_adc_clk_div_t adcClkDiv, uint32_t timeout)
{
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.clockDiv = adcClkDiv;
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	/* Send request to configure AIN*/
	if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
			SSTREAM_TASK_SET_ADC_CLOCK_DIV_BIT,
			eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;

	/* Wait until configuration is applied*/
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_SetChannelSamplingTime(sstream_connection_info* connectionHandler, uint32_t channel, sstream_adc_sampling_time_t stime, uint32_t timeout)
{
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	if(channel == 1)
	{
		prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch1.sampleTime = stime;
	}
	if(channel == 2)
	{
		prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch2.sampleTime = stime;
	}
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	/* Send request to configure AIN*/
	if(channel == 1)
	{
		if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
				SSTREAM_TASK_SET_ADC_CH1_STIME_BIT,
				eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;
	}
	if(channel == 2)
	{
		if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
				SSTREAM_TASK_SET_ADC_CH2_STIME_BIT,
				eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;
	}

	/* Wait until configuration is applied*/
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return SSTREAM_STATUS_OK;
}
sstream_status_t				SSTREAM_SetChannelOffset(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t offset, uint32_t timeout)
{
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	if(channel == 1)
	{
		prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch1.offset = offset;
	}
	if(channel == 2)
	{
		prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch2.offset = offset;
	}
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	/* Send request to configure AIN*/
	if(channel == 1)
	{
		if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
				SSTREAM_TASK_SET_ADC_CH1_OFFSET_BIT,
				eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;
	}
	if(channel == 2)
	{
		if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
				SSTREAM_TASK_SET_ADC_CH2_OFFSET_BIT,
				eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;
	}

	/* Wait until configuration is applied*/
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return SSTREAM_STATUS_OK;
}

sstream_status_t				SSTREAM_SetChannelAvgRatio(sstream_connection_info* connectionHandler, uint32_t channel, sstream_adc_ch_avg_ratio_t avgRatio, uint32_t timeout)
{
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	if(channel == 1)
	{
		prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch1.avgRatio = avgRatio;
	}
	if(channel == 2)
	{
		prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch2.avgRatio = avgRatio;
	}
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	/* Send request to configure AIN*/
	if(channel == 1)
	{
		if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
				SSTREAM_TASK_SET_ADC_CH1_AVERAGING_RATIO,
				eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;
	}
	if(channel == 2)
	{
		if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
				SSTREAM_TASK_SET_ADC_CH2_AVERAGING_RATIO,
				eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;
	}

	/* Wait until configuration is applied*/
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return SSTREAM_STATUS_OK;
}
sstream_adc_resolution_t		SSTREAM_GetResolution(sstream_connection_info* connectionHandler, uint32_t timeout)
{
	sstream_adc_resolution_t resolution;

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	resolution = prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.resolution;
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return resolution;
}
sstream_adc_clk_div_t			SSTREAM_GetClkDiv(sstream_connection_info* connectionHandler, uint32_t timeout)
{
	sstream_adc_clk_div_t adcClockDiv;

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	adcClockDiv = prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.clockDiv;
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return adcClockDiv;
}
uint32_t						SSTREAM_GetSamplingPeriod(sstream_connection_info* connectionHandler, uint32_t timeout)
{
	uint32_t stime;

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	stime = prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.samplingTime;
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return stime;
}
sstream_adc_sampling_time_t		SSTREAM_GetChannelSamplingTime(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t timeout)
{
	sstream_adc_sampling_time_t chstime;

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	if(channel == 1)
	{
		chstime = prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch1.sampleTime;
	}
	if(channel == 2)
	{
		chstime = prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch2.sampleTime;
	}
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return chstime;
}
uint32_t						SSTREAM_GetChannelOffset(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t timeout)
{
	uint32_t choffset;

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	if(channel == 1)
	{
		choffset = prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch1.offset;
	}
	if(channel == 2)
	{
		choffset = prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch2.offset;
	}
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return choffset;
}

sstream_adc_ch_avg_ratio_t		SSTREAM_GetChannelAvgRatio(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t timeout)
{
	sstream_adc_ch_avg_ratio_t avgRatio;

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	if(channel == 1)
	{
		avgRatio = prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch1.avgRatio;
	}
	if(channel == 2)
	{
		avgRatio = prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.ch2.avgRatio;
	}
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return avgRatio;
}

uint32_t						SSTREAM_GetAdcInputClk(sstream_connection_info* connectionHandler, uint32_t timeout)
{
	uint32_t inputClk;

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;
	inputClk = prvSSTREAM_DATA.controlInfo[connectionHandler->id].ainConfig.inputClk;
	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return inputClk;
}

sstream_status_t				SSTREAM_GetAdcValue(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t* value, uint32_t timeout)
{
	/* Send request to read AIN*/
	if(channel == 1)
	{
		if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
				SSTREAM_TASK_GET_ADC_CH1_VALUE,
				eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;
	}
	if(channel == 2)
	{
		if(xTaskNotify(prvSSTREAM_DATA.controlInfo[connectionHandler->id].controlTaskHandle,
				SSTREAM_TASK_GET_ADC_CH2_VALUE,
				eSetBits) != pdPASS) return SSTREAM_STATUS_ERROR;
	}

	/* Wait until channels are read*/
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;

	if(channel == 1)
	{
		*value = prvSSTREAM_DATA.streamInfo[connectionHandler->id].ch1Value;
	}
	if(channel == 2)
	{
		*value = prvSSTREAM_DATA.streamInfo[connectionHandler->id].ch2Value;
	}

	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return SSTREAM_STATUS_OK;
}

sstream_status_t				SSTREAM_GetLastSamples(sstream_connection_info* connectionHandler, uint8_t* buffer, uint32_t size, uint32_t timeout)
{
	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;

	memcpy(buffer, prvSSTREAM_DATA.streamInfo[connectionHandler->id].lastSamples, size*4);

	if(xSemaphoreGive(prvSSTREAM_DATA.controlInfo[connectionHandler->id].guard) != pdTRUE) return SSTREAM_STATUS_ERROR;

	return SSTREAM_STATUS_OK;
}

sstream_status_t				SSTREAM_RegisterAcquisitionStateChangeCB(sstream_acquistion_state_changed_callback cb)
{

	prvSSTREAM_DATA.asccb = cb;

	return SSTREAM_STATUS_OK;
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
