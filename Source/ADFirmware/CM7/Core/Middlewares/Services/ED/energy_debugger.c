/*
 * energy_debugger.c
 *
 *  Created on: Jun 23, 2024
 *      Author: Haris Turkmanovic, Pavle Lakic & Dimitrije Lilic
 */

#include "energy_debugger.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lwip.h"
#include "lwip/api.h"
#include "lwip/sockets.h"

#include "drv_ain.h"
#include "drv_gpio.h"
#include "drv_uart.h"

#include "system.h"
#include "logging.h"


//
/**
 * @brief Link state
 */
typedef enum{
	EP_LINK_STATE_UP,		/*!< Link is up */
	EP_LINK_STATE_DOWN		/*!< Link is down */
}energy_debugger_link_state_t;

/**
 * @brief Link state
 */
typedef enum{
	EP_COMM_STATUS_ESTABLISHED,		/*!< Link is up */
	EP_COMM_STATUS_DISCONNECTED		/*!< Link is down */
}energy_debugger_comm_status_t;

typedef struct
{

	TaskHandle_t 						clientTaskHandle;
	energy_debugger_connection_info		connectionInfo;
	energy_debugger_state_t 			taskState;
	energy_debugger_link_state_t        linkState;
	QueueHandle_t						ebp;
	uint32_t					        id;
} energy_debugger_links_t;

typedef struct
{
	energy_debugger_links_t				activeLink[ENERGY_DEBUGGER_MAX_CONNECTIONS];
	uint32_t							activeConnectionsNo;
	uint8_t 						    buttonClickCounter;
	energy_debugger_state_t 			mainTaskState;

} energy_debugger_data_t;


typedef struct
{
	uint32_t 	packetID;
	uint8_t     dmaID;
}energy_debugger_id_t;

typedef struct
{
	uint8_t 	name[ENERGY_DEBUGGER_MESSAGE_BUFFER_LENTH];
	uint8_t     nameLength;
}energy_debugger_ebp_name_t;

typedef struct
{
	energy_debugger_ebp_name_t 	name;
	energy_debugger_id_t 	    id;
}energy_debugger_breakpoint_info_t;

static 	TaskHandle_t 						prvENERGY_DEBUGGER_TASK_HANDLE;
static  QueueHandle_t						prvENERGY_DEBUGGER_QUEUE_VALUES;
static  QueueHandle_t						prvENERGY_DEBUGGER_QUEUE_EBP_NAME;
static  volatile energy_debugger_ebp_name_t prvENERGY_DEBUGGER_EBP_LAST_NAME;

static energy_debugger_data_t				prvENERGY_DEBUGGER_DATA;

static uint8_t								prvENERGY_DEBUGGER_ENABLED;






static void prvEDEBUGGING_SerialISR(uint8_t data)
{
	BaseType_t *pxHigherPriorityTaskWoken = pdFALSE;
	prvENERGY_DEBUGGER_EBP_LAST_NAME.name[prvENERGY_DEBUGGER_EBP_LAST_NAME.nameLength] = data;
	prvENERGY_DEBUGGER_EBP_LAST_NAME.nameLength += 1;
	if(data == '\r')
	{
		//Todo: Potential long execution
	    xQueueSendToBackFromISR(prvENERGY_DEBUGGER_QUEUE_EBP_NAME, &prvENERGY_DEBUGGER_EBP_LAST_NAME, pxHigherPriorityTaskWoken);
		memset(&prvENERGY_DEBUGGER_EBP_LAST_NAME, 0, sizeof(energy_debugger_ebp_name_t));
	    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
	}
	if((prvENERGY_DEBUGGER_EBP_LAST_NAME.nameLength + 1) == ENERGY_DEBUGGER_MESSAGE_BUFFER_LENTH)
	{
		//Prevent buffer overflow
		memset(&prvENERGY_DEBUGGER_EBP_LAST_NAME, 0, sizeof(energy_debugger_ebp_name_t));
		prvENERGY_DEBUGGER_EBP_LAST_NAME.nameLength = 0;
	}

}


static void prvEDEBUGGING_SyncISR(uint16_t GPIO_Pin)
{
	BaseType_t *pxHigherPriorityTaskWoken = pdFALSE;
	//DRV_GPIO_ClearInterruptFlag(GPIO_Pin);
	uint32_t tmpPacketCounter = 0;
	energy_debugger_id_t id;

	//Set energy breakpoint marker
	DRV_AIN_Stream_SetCapture(&id.packetID, &id.dmaID);

    // Increment the button click counter
    prvENERGY_DEBUGGER_DATA.buttonClickCounter++;

    // Log the button press
    //LOGGING_Write("Energy Debugger", LOGGING_MSG_TYPE_INFO, "Button pressed! Count: %d\r\n", prvENERGY_DEBUGGER_DATA.button_click_counter);

    // Change state if needed
    if (prvENERGY_DEBUGGER_DATA.mainTaskState == ENERGY_DEBUGGER_STATE_INIT)
    {
        prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_SERVICE;
    }
    //xTaskNotifyFromISR(prvENERGY_DEBUGGER_TASK_HANDLE, 0x01, eSetBits, pxHigherPriorityTaskWoken);

    xQueueSendToBackFromISR(prvENERGY_DEBUGGER_QUEUE_VALUES, &id, pxHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}
static void prvENERGY_DEBUGGER_Client_Task(void* pvParam)
{

	uint8_t		   tcpBuffer[ENERGY_DEBUGGER_TCP_MESSAGE_BUFFER_LENTH];
	memset(tcpBuffer, 0, ENERGY_DEBUGGER_TCP_MESSAGE_BUFFER_LENTH);
    struct netconn *conn;
    err_t 			connect_err;
	ip_addr_t 		remote_ip;
	energy_debugger_links_t			linkData;
	memcpy(&linkData, pvParam, sizeof(energy_debugger_links_t));

	energy_debugger_breakpoint_info_t ebp;
	memset(&ebp, 0, sizeof(energy_debugger_breakpoint_info_t));
	for(;;)
	{
		switch(linkData.taskState)
		{
			case ENERGY_DEBUGGER_STATE_INIT:

				conn = netconn_new(NETCONN_TCP);
				IP_ADDR4(&remote_ip, linkData.connectionInfo.serverIp[0],
						linkData.connectionInfo.serverIp[1],
						linkData.connectionInfo.serverIp[2],
						linkData.connectionInfo.serverIp[3]);
				LOGGING_Write("Energy point client service", LOGGING_MSG_TYPE_INFO,  "Try to create eplink connection with server:\r\n");
				LOGGING_Write("Energy point client service", LOGGING_MSG_TYPE_INFO,  "Server IP: %d.%d.%d.%d\r\n",linkData.connectionInfo.serverIp[0],
						linkData.connectionInfo.serverIp[1],
						linkData.connectionInfo.serverIp[2],
						linkData.connectionInfo.serverIp[3]);
				LOGGING_Write("Energy point client service", LOGGING_MSG_TYPE_INFO,  "Server Port: %d\r\n",linkData.connectionInfo.serverport);


				connect_err = netconn_connect(conn, &remote_ip, linkData.connectionInfo.serverport);

				if(connect_err != ERR_OK)
				{
					LOGGING_Write("Energy point client service", LOGGING_MSG_TYPE_ERROR,  "There is a problem to connect to eplink server\r\n");
					linkData.taskState = ENERGY_DEBUGGER_STATE_ERROR;
					break;
				}

				LOGGING_Write("Energy point client service", LOGGING_MSG_TYPE_INFO,  "Device is successfully connected to eplink server\r\n");

				linkData.linkState = EP_LINK_STATE_UP;
				linkData.taskState = ENERGY_DEBUGGER_STATE_SERVICE;
				LOGGING_Write("Energy point client service",LOGGING_MSG_TYPE_INFO,  "Energy point client service successfully initialized\r\n");
				break;
			case ENERGY_DEBUGGER_STATE_SERVICE:
				//Get id
				memset(tcpBuffer, 0, ENERGY_DEBUGGER_TCP_MESSAGE_BUFFER_LENTH);
				if(xQueueReceive(linkData.ebp, &ebp, portMAX_DELAY) != pdTRUE)
				{
					linkData.taskState = ENERGY_DEBUGGER_STATE_ERROR;
					break;
				}
				*((uint32_t*)&tcpBuffer) = ebp.id.packetID;
				*((((uint32_t*)&tcpBuffer) + 1)) = ebp.id.dmaID;
				memcpy(&tcpBuffer[8], ebp.name.name, ebp.name.nameLength);
				LOGGING_Write("Energy point client service",LOGGING_MSG_TYPE_INFO,  "New EP info received\r\n");
				if(netconn_write(conn, tcpBuffer, ebp.name.nameLength + 8, NETCONN_COPY) != ERR_OK)
				{
					LOGGING_Write("Energy point client service", LOGGING_MSG_TYPE_WARNNING,  "Unable to send EP message\r\n");
				}
				else
				{
					LOGGING_Write("Energy point client service", LOGGING_MSG_TYPE_INFO,  "EP message successfully sent\r\n");
				}

				memset(&ebp, 0, sizeof(energy_debugger_breakpoint_info_t));
				break;
			case ENERGY_DEBUGGER_STATE_ERROR:
				SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
				vTaskDelay(portMAX_DELAY);
				break;
		}
	}
}

static void prvENERGY_DEBUGGER_EPP_Task()
{
		LOGGING_Write("Energy Debugger", LOGGING_MSG_TYPE_INFO, "Energy Debugger service started\r\n");
		uint8_t  data;
		drv_uart_config_t channelConfig;
        energy_debugger_id_t 		      id;
		energy_debugger_ebp_name_t		  ebpName;
		energy_debugger_breakpoint_info_t ebp;
		energy_debugger_comm_status_t	  comStatus = EP_COMM_STATUS_DISCONNECTED;
		memset(&ebp, 0, sizeof(energy_debugger_breakpoint_info_t));
		memset(&ebpName, 0, sizeof(energy_debugger_ebp_name_t));
		for(;;)
		{
			switch(prvENERGY_DEBUGGER_DATA.mainTaskState)
			{
			case ENERGY_DEBUGGER_STATE_INIT:


				channelConfig.baudRate = 115200;
				channelConfig.parityEnable = DRV_UART_PARITY_NONE;
				channelConfig.stopBitNo	= DRV_UART_STOPBIT_1;

				// Initialize UART4
				if(DRV_UART_Instance_Init(DRV_UART_INSTANCE_4, &channelConfig) != DRV_UART_STATUS_OK)
				{
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_ERROR,  "Unable to initialize serial port\r\n");
					prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;
					break;
				}

				if(DRV_UART_Instance_RegisterRxCallback(DRV_UART_INSTANCE_4, prvEDEBUGGING_SerialISR)!= DRV_UART_STATUS_OK)
				{
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_ERROR,  "Unable to initialize serial port\r\n");
					prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;
					break;
				}

			    // Initialize the GPIO port
			    if (DRV_GPIO_Port_Init(ENERGY_DEBUGGER_BUTTON_PORT) != DRV_GPIO_STATUS_OK)
			    	prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;

			    // Configure the pin for the button
			    drv_gpio_pin_init_conf_t button_pin_conf;
			    button_pin_conf.mode = DRV_GPIO_PIN_MODE_IT_RISING_FALLING;
			    button_pin_conf.pullState = DRV_GPIO_PIN_PULL_NOPULL;
			    //Definisati pin preko makroa
			    if (DRV_GPIO_Pin_Init(ENERGY_DEBUGGER_BUTTON_PORT, ENERGY_DEBUGGER_BUTTON_PIN, &button_pin_conf) != DRV_GPIO_STATUS_OK)
			    {
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_ERROR,  "Unable to register sync GPIO\r\n");
					prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;
			    	break;
			    }

			    // Register the button press callback
			    if (DRV_GPIO_RegisterCallback(ENERGY_DEBUGGER_BUTTON_PORT, ENERGY_DEBUGGER_BUTTON_PIN, prvEDEBUGGING_SyncISR, ENERGY_DEBUGGER_BUTTON_ISR_PRIO, &button_pin_conf) != DRV_GPIO_STATUS_OK)
			    {
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_ERROR,  "Unable to register sync callback\r\n");
					prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;
			    	break;
			    }
			    prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_SERVICE;
				LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_INFO,  "Energy point service successfully initialized\r\n");

				ENERGY_DEBUGGER_SetEnable(0);

				break;
			case ENERGY_DEBUGGER_STATE_SERVICE:
				//Wait for new debugger name
				memset(&ebp, 0, sizeof(energy_debugger_breakpoint_info_t));
				memset(&ebpName, 0, sizeof(energy_debugger_ebp_name_t));
				if(xQueueReceive(prvENERGY_DEBUGGER_QUEUE_EBP_NAME, &ebpName, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_ERROR,  "Unable to get EP info from callback\r\n");
					prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;
			    	break;
				}

				LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_INFO,  "Energy debugger message received\r\n");
				if(ebpName.name[0] == '0')
				{
					//config message
					if(strncmp((char*)ebpName.name, "0:START\r", 6) == 0)
					{
						DRV_UART_TransferData(DRV_UART_INSTANCE_4, "OK\r", 3, 1000);
						ENERGY_DEBUGGER_SetEnable(1);
						comStatus = EP_COMM_STATUS_ESTABLISHED;
						LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_INFO,  "EP Link with DUT is established\r\n");
						break;;
					}
					if(strncmp((char*)ebpName.name, "0:STOP\r", 6) == 0)
					{
						DRV_UART_TransferData(DRV_UART_INSTANCE_4, "OK\r", 3, 1000);
						ENERGY_DEBUGGER_SetEnable(0);
						LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_INFO,  "EP Link with DUT is disconnected\r\n");
						comStatus = EP_COMM_STATUS_DISCONNECTED;
						break;;
					}
					DRV_UART_TransferData(DRV_UART_INSTANCE_4, "ERROR\r", 6, 1000);
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_WARNNING,  "Undefined command received from DUT\r\n");
					break;
				}
				if(ebpName.name[0] == '1')
				{
					//ebp message
					if(comStatus != EP_COMM_STATUS_ESTABLISHED)
					{
						LOGGING_Write("Energy point service", LOGGING_MSG_TYPE_WARNNING,  "Start EP from DUT\r\n");
						break;
					}

					//Get id
					if(xQueueReceive(prvENERGY_DEBUGGER_QUEUE_VALUES, &id, 0) != pdTRUE)
					{
						LOGGING_Write("Energy point service", LOGGING_MSG_TYPE_WARNNING,  "EP ID mismatch\r\n");
						break;
					}

					LOGGING_Write("Energy point service", LOGGING_MSG_TYPE_INFO,  "Energy point successfully received\r\n");
					ebp.id.packetID = id.packetID;
					ebp.id.dmaID = id.dmaID;
					memcpy(&ebp.name, &ebpName, sizeof(energy_debugger_ebp_name_t));
					ebp.name.name[ebp.name.nameLength] = '\n';
					ebp.name.nameLength += 1;
					LOGGING_Write("Energy point service", LOGGING_MSG_TYPE_INFO,  "Energy point name: %s\n", ebpName.name);
					LOGGING_Write("Energy point service", LOGGING_MSG_TYPE_INFO,  "Energy point Packet ID: %d\r\n", id.packetID);
					LOGGING_Write("Energy point service", LOGGING_MSG_TYPE_INFO,  "Energy point DMA ID: %d\r\n", id.dmaID);

					//send info
					for(int i = 0; i < prvENERGY_DEBUGGER_DATA.activeConnectionsNo; i++)
					{
						if(xQueueSendToBack(prvENERGY_DEBUGGER_DATA.activeLink[i].ebp, &ebp, 0) != pdTRUE)
						{
							LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_ERROR,  "Unable to send EP info\r\n");
							prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;
							break;
						}
					}
					break;
				}
				if(ebpName.name[0] == '2')
				{
					LOGGING_Write("Energy point service", LOGGING_MSG_TYPE_INFO,  "Info messages received from DUT:\r\n");
					LOGGING_Write("Energy point service", LOGGING_MSG_TYPE_INFO,  "%s\n", ebpName.name);
					break;
				}
				LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_WARNNING,  "Undefined message received from DUT\r\n");
				break;
			case ENERGY_DEBUGGER_STATE_ERROR:
				SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
				vTaskDelay(portMAX_DELAY);
				break;
			}

		}
}


energy_debugger_status_t ENERGY_DEBUGGER_CreateLink(energy_debugger_connection_info* serverInfo, uint32_t timeout)
{

	if(prvENERGY_DEBUGGER_DATA.activeConnectionsNo > ENERGY_DEBUGGER_MAX_CONNECTIONS) return ENERGY_DEBUGGER_STATE_ERROR;
	uint32_t currentID = prvENERGY_DEBUGGER_DATA.activeConnectionsNo;
	memcpy(&prvENERGY_DEBUGGER_DATA.activeLink[currentID].connectionInfo, serverInfo, sizeof(energy_debugger_connection_info));
	prvENERGY_DEBUGGER_DATA.activeLink[currentID].id = 0;
	prvENERGY_DEBUGGER_DATA.activeLink[currentID].taskState = ENERGY_DEBUGGER_STATE_INIT;

	// Create the client task
	prvENERGY_DEBUGGER_DATA.activeLink[currentID].ebp =  xQueueCreate(
    		CONF_ENERGY_DEBUGGER_EBP_QUEUE_LENGTH,
			sizeof(energy_debugger_breakpoint_info_t));

    if(prvENERGY_DEBUGGER_DATA.activeLink[currentID].ebp == NULL ) return ENERGY_DEBUGGER_STATE_ERROR;

	// Create the client task
	if(xTaskCreate(prvENERGY_DEBUGGER_Client_Task,
			ENERGY_DEBUGGER_TASK_NAME,
			ENERGY_DEBUGGER_STACK_SIZE,
			&prvENERGY_DEBUGGER_DATA.activeLink[currentID],
			ENERGY_DEBUGGER_TASK_PRIO,
			&prvENERGY_DEBUGGER_DATA.activeLink[currentID].clientTaskHandle) != pdTRUE) return ENERGY_DEBUGGER_STATUS_ERROR;

	prvENERGY_DEBUGGER_DATA.activeConnectionsNo += 1;


    return ENERGY_DEBUGGER_STATUS_OK;
}
energy_debugger_status_t ENERGY_DEBUGGER_SetEnable(uint8_t enableStatus)
{
	if(enableStatus == 1)
	{
	    drv_gpio_pin_init_conf_t button_pin_conf;
	    button_pin_conf.mode = DRV_GPIO_PIN_MODE_IT_RISING_FALLING;
	    button_pin_conf.pullState = DRV_GPIO_PIN_PULL_NOPULL;
		xQueueReset(prvENERGY_DEBUGGER_QUEUE_VALUES);
		for(int i = 0; i < prvENERGY_DEBUGGER_DATA.activeConnectionsNo; i++)
		{
			xQueueReset(prvENERGY_DEBUGGER_DATA.activeLink[i].ebp);
		}
		//DRV_UART_Instance_EnableISR(DRV_UART_INSTANCE_4);
	    DRV_GPIO_RegisterCallback(ENERGY_DEBUGGER_BUTTON_PORT, ENERGY_DEBUGGER_BUTTON_PIN, prvEDEBUGGING_SyncISR, ENERGY_DEBUGGER_BUTTON_ISR_PRIO, &button_pin_conf);
	}
	else
	{
		DRV_GPIO_Pin_DisableInt(ENERGY_DEBUGGER_BUTTON_PORT, ENERGY_DEBUGGER_BUTTON_PIN);
		//DRV_UART_Instance_DisableISR(DRV_UART_INSTANCE_4);

	}
    return ENERGY_DEBUGGER_STATUS_OK;
}

energy_debugger_status_t ENERGY_DEBUGGER_Init(uint32_t timeout)
{
    // Create the main task
	memset(&prvENERGY_DEBUGGER_DATA, 0, sizeof(energy_debugger_data_t));


    if(xTaskCreate(prvENERGY_DEBUGGER_EPP_Task,
            ENERGY_DEBUGGER_TASK_NAME,
            ENERGY_DEBUGGER_STACK_SIZE,
            NULL,
            ENERGY_DEBUGGER_TASK_PRIO,
            &prvENERGY_DEBUGGER_TASK_HANDLE) != pdTRUE) return ENERGY_DEBUGGER_STATUS_ERROR;

    prvENERGY_DEBUGGER_QUEUE_VALUES =  xQueueCreate(
    		ENERGY_DEBUGGER_ID_QUEUE_LENTH,
			sizeof(energy_debugger_id_t));

    if(prvENERGY_DEBUGGER_QUEUE_VALUES == NULL ) return ENERGY_DEBUGGER_STATE_ERROR;

    prvENERGY_DEBUGGER_QUEUE_EBP_NAME =  xQueueCreate(
    		CONF_ENERGY_DEBUGGER_EBP_NAMES_QUEUE_LENGTH,
			sizeof(energy_debugger_ebp_name_t));

    if(prvENERGY_DEBUGGER_QUEUE_EBP_NAME == NULL ) return ENERGY_DEBUGGER_STATE_ERROR;

    prvENERGY_DEBUGGER_ENABLED = 0;

    return ENERGY_DEBUGGER_STATUS_OK;
}

