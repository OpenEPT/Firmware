/**
 ******************************************************************************
 * @file   	sstream.c
 *
 * @brief
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	Februar 2024
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


#define  	SSTREAM_TASK_START_BIT						0x00000001
#define  	SSTREAM_TASK_STOP_BIT						0x00000002
#define  	SSTREAM_TASK_STREAM_BIT						0x00000004
#define  	SSTREAM_TASK_SET_ADC_RESOLUTION_BIT			0x00000008
#define  	SSTREAM_TASK_SET_ADC_STIME_BIT				0x00000010
#define  	SSTREAM_TASK_SET_ADC_CLOCK_DIV_BIT			0x00000020
#define  	SSTREAM_TASK_SET_ADC_CH1_STIME_BIT			0x00000040
#define  	SSTREAM_TASK_SET_ADC_CH2_STIME_BIT			0x00000080
#define  	SSTREAM_TASK_SET_ADC_CH1_OFFSET_BIT			0x00000100
#define  	SSTREAM_TASK_SET_ADC_CH2_OFFSET_BIT			0x00000200
#define  	SSTREAM_TASK_SET_ADC_CH1_AVERAGING_RATIO	0x00000400
#define  	SSTREAM_TASK_SET_ADC_CH2_AVERAGING_RATIO	0x00000800
#define  	SSTREAM_TASK_GET_ADC_CH1_VALUE				0x00001000
#define  	SSTREAM_TASK_GET_ADC_CH2_VALUE				0x00002000



typedef struct
{
	TaskHandle_t				controlTaskHandle;
	SemaphoreHandle_t			initSig;
	SemaphoreHandle_t			guard;
	sstream_state_t				state;
	sstream_acquisition_state_t	acquisitionState;
	drv_ain_adc_config_t		ainConfig;
	uint32_t					id;
}sstream_control_data_t;

typedef struct
{
	TaskHandle_t				streamTaskHandle;
	SemaphoreHandle_t			initSig;
	SemaphoreHandle_t			guard;
	sstream_state_t				state;
	sstream_connection_info		connectionInfo;
	uint32_t					id;
	uint32_t					ch1Value;
	uint32_t					ch2Value;
}sstream_stream_data_t;

typedef struct
{
	sstream_control_data_t		controlInfo[CONF_SSTREAM_CONNECTIONS_MAX_NO];
	sstream_stream_data_t		streamInfo[CONF_SSTREAM_CONNECTIONS_MAX_NO];
	uint32_t					activeConnectionsNo;
}sstream_data_t;

typedef struct
{
	uint32_t 	address;
	uint8_t		id;
}sstream_packet_t;


static sstream_data_t			prvSSTREAM_DATA;

static uint16_t					prvSSTREAM_TEST_DATA[CONF_AIN_MAX_BUFFER_SIZE];
QueueHandle_t					prvSSTREAM_PACKET_QUEUE;


static void prvSSTREAM_NewPacketSampled(uint32_t packetAddress, uint8_t packetID)
{
	ITM_SendChar('a');
	BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
	sstream_packet_t		packetData;
	packetData.address = packetAddress;
	packetData.id = packetID;

	xQueueSendFromISR(prvSSTREAM_PACKET_QUEUE, &packetData, &pxHigherPriorityTaskWoken);

    portYIELD_FROM_ISR( pxHigherPriorityTaskWoken );

}


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
			memset(prvSSTREAM_TEST_DATA, 0xA5, 2*CONF_AIN_MAX_BUFFER_SIZE);

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

			p = pbuf_alloc(PBUF_TRANSPORT, 2*(CONF_AIN_MAX_BUFFER_SIZE + DRV_AIN_ADC_BUFFER_OFFSET), PBUF_RAM);

			if(p == NULL)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to release create pbuf\r\n");
			}

			memcpy(p->payload, (void*)packetData.address, 2*(CONF_AIN_MAX_BUFFER_SIZE + DRV_AIN_ADC_BUFFER_OFFSET));

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
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize stream LED port\r\n");
			}
			if(DRV_GPIO_Pin_Init(SSTREAM_LED_PORT, SSTREAM_LED_PIN, &acqStateLED) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("SStream service", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize stream LED ppin\r\n");
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
				connectionData->ainConfig.resolution = DRV_AIN_ADC_RESOLUTION_UKNOWN;
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
				connectionData->ainConfig.resolution = DRV_AIN_ADC_CLOCK_DIV_UKNOWN;
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
				connectionData->ainConfig.ch1.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_UKNOWN;
				connectionData->ainConfig.ch2.sampleTime = DRV_AIN_ADC_SAMPLE_TIME_UKNOWN;
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
				connectionData->acquisitionState = SSTREAM_ACQUISITION_STATE_START;

				if(DRV_AIN_Start(connectionData->ainConfig.adc) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Stream started \r\n");
					DRV_GPIO_Pin_SetState(SSTREAM_LED_PORT, SSTREAM_LED_PIN, DRV_GPIO_PIN_STATE_SET);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Unable to start stream\r\n");
				}

				notifyValue |= SSTREAM_TASK_STREAM_BIT;

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
				connectionData->acquisitionState = SSTREAM_ACQUISITION_STATE_STOP;

				if(DRV_AIN_Stop(DRV_AIN_ADC_3) == DRV_AIN_STATUS_OK)
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Stream stoped \r\n");
					DRV_GPIO_Pin_SetState(SSTREAM_LED_PORT, SSTREAM_LED_PIN, DRV_GPIO_PIN_STATE_RESET);
				}
				else
				{
					LOGGING_Write("SStream service", LOGGING_MSG_TYPE_INFO,  "Unable to stop stream\r\n");
				}

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

	if(xTaskCreate(
			prvSSTREAM_ControlTaskFunc,
			SSTREAM_CONTROL_TASK_NAME,
			SSTREAM_CONTROL_TASK_STACK_SIZE,
			&prvSSTREAM_DATA.controlInfo[currentId],
			SSTREAM_CONTROL_TASK_PRIO,
			&prvSSTREAM_DATA.controlInfo[currentId].controlTaskHandle) != pdPASS) return SSTREAM_STATUS_ERROR;

	/* Create stream task */
	if(xTaskCreate(
			prvSSTREAM_StreamTaskFunc,
			SSTREAM_STREAM_TASK_NAME,
			SSTREAM_STREAM_TASK_STACK_SIZE,
			&prvSSTREAM_DATA.streamInfo[currentId],
			SSTREAM_STREAM_TASK_PRIO,
			&prvSSTREAM_DATA.streamInfo[currentId].streamTaskHandle) != pdPASS) return SSTREAM_STATUS_ERROR;

	prvSSTREAM_DATA.controlInfo[currentId].initSig = xSemaphoreCreateBinary();
	if(prvSSTREAM_DATA.controlInfo[currentId].initSig == NULL) return SSTREAM_STATUS_ERROR;

	prvSSTREAM_DATA.streamInfo[currentId].initSig = xSemaphoreCreateBinary();
	if(prvSSTREAM_DATA.streamInfo[currentId].initSig == NULL) return SSTREAM_STATUS_ERROR;

	prvSSTREAM_DATA.controlInfo[currentId].guard = xSemaphoreCreateMutex();
	if(prvSSTREAM_DATA.controlInfo[currentId].guard == NULL) return SSTREAM_STATUS_ERROR;

	prvSSTREAM_DATA.streamInfo[currentId].guard = xSemaphoreCreateMutex();
	if(prvSSTREAM_DATA.streamInfo[currentId].guard == NULL) return SSTREAM_STATUS_ERROR;

	if(xSemaphoreTake(prvSSTREAM_DATA.controlInfo[currentId].initSig,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return SSTREAM_STATUS_ERROR;

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


