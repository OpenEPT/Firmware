/**
 ******************************************************************************
 * @file    energy_debugger.c
 *
 * @brief   Energy debugger service provides TCP client functionality for
 *          energy profiling and debugging. It monitors GPIO button presses,
 *          captures energy breakpoints via UART, and transmits debugging
 *          information to remote servers over TCP connections.
 *
 * @author  Pavle Lakic & Dimitrije Lilic
 * @date    June 2024
 ******************************************************************************
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


/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup ENERGY_DEBUGGER_SERVICE Energy Debugger service
 * @{
 */

/**
 * @defgroup ENERGY_DEBUGGER_PRIVATE_ENUMS Energy Debugger private enumerations
 * @{
 */

/**
 * @brief Link connection state enumeration
 */
typedef enum{
	EP_LINK_STATE_UP,		/**< Link is established and operational */
	EP_LINK_STATE_DOWN		/**< Link is disconnected or failed */
}energy_debugger_link_state_t;
/**
 * @}
 */

/**
 * @defgroup ENERGY_DEBUGGER_PRIVATE_STRUCTURES Energy Debugger private structures
 * @{
 */
/**
 * @brief Structure representing an active TCP link to energy debugger server
 */
typedef struct
{
	TaskHandle_t 						clientTaskHandle;	/**< FreeRTOS task handle for this link */
	energy_debugger_connection_info		connectionInfo;		/**< TCP connection parameters */
	energy_debugger_state_t 			taskState;			/**< Current task state */
	energy_debugger_link_state_t        linkState;			/**< Current link connection state */
	QueueHandle_t						ebp;				/**< Queue for energy breakpoint data */
	uint32_t					        id;					/**< Link identifier */
} energy_debugger_links_t;

/**
 * @brief Main energy debugger service data structure
 */
typedef struct
{
	energy_debugger_links_t				activeLink[ENERGY_DEBUGGER_MAX_CONNECTIONS];	/**< Array of active TCP connections */
	uint32_t							activeConnectionsNo;							/**< Number of currently active connections */
	uint8_t 						    buttonClickCounter;								/**< Counter for button press events */
	energy_debugger_state_t 			mainTaskState;									/**< Main task state */
} energy_debugger_data_t;


/**
 * @brief Structure representing energy debugger breakpoint name
 */
typedef struct
{
	uint8_t 	name[ENERGY_DEBUGGER_MESSAGE_BUFFER_LENGTH];	/**< Breakpoint name string */
	uint8_t     nameLength;									/**< Length of the name string */
}energy_debugger_ebp_name_t;

/**
 * @brief Structure containing breakpoint information
 */
typedef struct
{
	energy_debugger_ebp_name_t 	name;	/**< Breakpoint name structure */
	uint32_t 					id;		/**< Unique breakpoint identifier */
}energy_debugger_breakpoint_info_t;

/**
 * @}
 */

/**
 * @defgroup ENERGY_DEBUGGER_PRIVATE_DATA Energy Debugger private data
 * @{
 */

/**
 * @brief Main task handle for energy debugger service
 */
static 	TaskHandle_t 						prvENERGY_DEBUGGER_TASK_HANDLE;

/**
 * @brief Queue for energy breakpoint IDs
 */
static  QueueHandle_t						prvENERGY_DEBUGGER_QUEUE_ID;

/**
 * @brief Queue for energy breakpoint names received via UART
 */
static  QueueHandle_t						prvENERGY_DEBUGGER_QUEUE_EBP_NAME;

/**
 * @brief Last received energy breakpoint name (volatile for ISR access)
 */
static  volatile energy_debugger_ebp_name_t prvENERGY_DEBUGGER_EBP_LAST_NAME;

/**
 * @brief Static instance of the energy debugger service data
 */
static energy_debugger_data_t				prvENERGY_DEBUGGER_DATA;

/**
 * @}
 */

/**
 * @defgroup ENERGY_DEBUGGER_PRIVATE_FUNCTIONS Energy Debugger service private functions
 * @{
 */

/**
 * @brief UART character received callback function.
 *
 * This function is called from interrupt context when a character is received
 * on the UART interface. It builds the energy breakpoint name string character
 * by character until a carriage return ('\r') is detected, then queues the
 * complete name for processing by the main task.
 *
 * @param[in] data Received character from UART
 * 
 * @note This function executes in interrupt context and must be kept minimal.
 * @note The function handles potential long execution warning in the TODO comment.
 */
static void prvEDEBUGGING_SerialCharReceived(uint8_t data)
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

}

/**
 * @brief GPIO button press interrupt callback function.
 *
 * This function is triggered when the energy debugger button is pressed. It performs
 * the following operations:
 * - Clears the GPIO interrupt flag
 * - Sets energy breakpoint capture marker via ADC stream
 * - Increments button click counter
 * - Queues the packet counter for transmission
 * - Transitions main task from INIT to SERVICE state if needed
 *
 * @param[in] GPIO_Pin GPIO pin number that triggered the interrupt
 * 
 * @note This function executes in interrupt context.
 * @note The commented logging code should remain disabled in ISR context.
 */
static void prvEDEBUGGING_ButtonPressedCallback(uint16_t GPIO_Pin)
{
	BaseType_t *pxHigherPriorityTaskWoken = pdFALSE;
	DRV_GPIO_ClearInterruptFlag(GPIO_Pin);
	uint32_t tmpPacketCounter = 0;

	//Set energy breakpoint marker
	DRV_AIN_Stream_SetCapture(&tmpPacketCounter);

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

    xQueueSendToBackFromISR(prvENERGY_DEBUGGER_QUEUE_ID, &tmpPacketCounter, pxHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}
/**
 * @brief TCP client task function for individual energy debugger connections.
 *
 * This task manages a single TCP connection to an energy debugger server. It operates
 * in three main states:
 *
 * - `ENERGY_DEBUGGER_STATE_INIT`: Establishes TCP connection to the specified server
 *   using the connection parameters provided during link creation.
 *
 * - `ENERGY_DEBUGGER_STATE_SERVICE`: Waits for energy breakpoint information from
 *   the main task queue, formats the data (ID + name), and transmits it to the
 *   connected server via TCP.
 *
 * - `ENERGY_DEBUGGER_STATE_ERROR`: Reports errors to the system and suspends the task.
 *
 * @param[in] pvParam Pointer to energy_debugger_links_t structure containing
 *                    connection parameters and task data
 * 
 * @note Each active connection runs as a separate instance of this task.
 * @note TCP buffer size is limited to ENERGY_DEBUGGER_TCP_MESSAGE_BUFFER_LENGTH bytes.
 */
static void prvENERGY_DEBUGGER_Client_Task(void* pvParam)
{

	uint8_t		   tcpBuffer[ENERGY_DEBUGGER_TCP_MESSAGE_BUFFER_LENGTH];
	memset(tcpBuffer, 0, ENERGY_DEBUGGER_TCP_MESSAGE_BUFFER_LENGTH);
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
				memset(tcpBuffer, 0, ENERGY_DEBUGGER_TCP_MESSAGE_BUFFER_LENGTH);
				if(xQueueReceive(linkData.ebp, &ebp, portMAX_DELAY) != pdTRUE)
				{
					linkData.taskState = ENERGY_DEBUGGER_STATE_ERROR;
					break;
				}
				*((uint32_t*)&tcpBuffer) = ebp.id;
				memcpy(&tcpBuffer[4], ebp.name.name, ebp.name.nameLength);
				LOGGING_Write("Energy point client service",LOGGING_MSG_TYPE_INFO,  "New EP info received\r\n");
				if(netconn_write(conn, tcpBuffer, ebp.name.nameLength + 4, NETCONN_COPY) != ERR_OK)
				{
					LOGGING_Write("Energy point client service", LOGGING_MSG_TYPE_WARNING,  "Unable to send EP message\r\n");
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
/**
 * @brief Main energy debugger service task function.
 *
 * This is the core task responsible for initializing the energy debugger service
 * and coordinating energy breakpoint collection and distribution. The task operates
 * in three main states:
 *
 * - `ENERGY_DEBUGGER_STATE_INIT`: Performs initialization including:
 *    - UART configuration for breakpoint name reception
 *    - GPIO configuration for button press detection
 *    - Callback registration for interrupt handling
 *
 * - `ENERGY_DEBUGGER_STATE_SERVICE`: Main operation loop that:
 *    - Receives energy breakpoint names from UART queue
 *    - Receives corresponding IDs from button press queue
 *    - Distributes complete breakpoint information to all active TCP connections
 *
 * - `ENERGY_DEBUGGER_STATE_ERROR`: Reports errors and suspends the task
 *
 * @note This function runs as a FreeRTOS task and never returns.
 * @note Contains commented legacy UART6 code that should be removed.
 */
static void prvENERGY_DEBUGGER_Task()
{
		LOGGING_Write("Energy Debugger", LOGGING_MSG_TYPE_INFO, "Energy Debugger service started\r\n");
		drv_uart_config_t channelConfig;
		uint32_t id;
		energy_debugger_ebp_name_t		  ebpName;
		energy_debugger_breakpoint_info_t ebp;
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

				if(DRV_UART_Instance_RegisterRxCallback(DRV_UART_INSTANCE_4, prvEDEBUGGING_SerialCharReceived)!= DRV_UART_STATUS_OK)
				{
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_ERROR,  "Unable to initialize serial port\r\n");
					prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;
					break;
				}
				//Todo Ovo treba ukloniti iz koda, zamenili smo USART6 -> UART4
				/*
				if(DRV_UART_Instance_Init(DRV_UART_INSTANCE_6, &channelConfig) != DRV_UART_STATUS_OK)
			    {
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_ERROR,  "Unable to initialize serial port\r\n");
					prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;
			    	break;
			    }

				if(DRV_UART_Instance_RegisterRxCallback(DRV_UART_INSTANCE_6, prvEDEBUGGING_SerialCharReceived)!= DRV_UART_STATUS_OK)
			    {
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_ERROR,  "Unable to initialize serial port\r\n");
					prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;
			    	break;
			    }
			    */

			    // Initialize the GPIO port
			    if (DRV_GPIO_Port_Init(ENERGY_DEBUGGER_BUTTON_PORT) != DRV_GPIO_STATUS_OK)
			    	prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;

			    // Configure the pin for the button
			    drv_gpio_pin_init_conf_t button_pin_conf;
			    button_pin_conf.mode = DRV_GPIO_PIN_MODE_IT_RISING;
			    button_pin_conf.pullState = DRV_GPIO_PIN_PULL_NOPULL;
			    //Definisati pin preko makroa
			    if (DRV_GPIO_Pin_Init(ENERGY_DEBUGGER_BUTTON_PORT, ENERGY_DEBUGGER_BUTTON_PIN, &button_pin_conf) != DRV_GPIO_STATUS_OK)
			    {
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_ERROR,  "Unable to register sync GPIO\r\n");
					prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;
			    	break;
			    }

			    // Register the button press callback
			    if (DRV_GPIO_RegisterCallback(ENERGY_DEBUGGER_BUTTON_PORT, ENERGY_DEBUGGER_BUTTON_PIN, prvEDEBUGGING_ButtonPressedCallback, ENERGY_DEBUGGER_BUTTON_ISR_PRIO) != DRV_GPIO_STATUS_OK)
			    {
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_ERROR,  "Unable to register sync callback\r\n");
					prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;
			    	break;
			    }
			    prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_SERVICE;
				LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_INFO,  "Energy point service successfully initialized\r\n");

				break;
			case ENERGY_DEBUGGER_STATE_SERVICE:
				//Wait for new debugger name
				if(xQueueReceive(prvENERGY_DEBUGGER_QUEUE_EBP_NAME, &ebpName, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_ERROR,  "Unable to get EP info from callback\r\n");
					prvENERGY_DEBUGGER_DATA.mainTaskState = ENERGY_DEBUGGER_STATE_ERROR;
			    	break;
				}

				//Get id
				if(xQueueReceive(prvENERGY_DEBUGGER_QUEUE_ID, &id, 0) != pdTRUE)
				{
					LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_WARNING,  "EP ID mismatch\r\n");
					continue;
				}


				LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_INFO,  "Energy point successfully received\r\n");
				ebp.id = id;
				memcpy(&ebp.name, &ebpName, sizeof(energy_debugger_ebp_name_t));
				LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_INFO,  "Energy point name: %s\r\n", ebpName.name);
				LOGGING_Write("Energy point service",LOGGING_MSG_TYPE_INFO,  "Energy point ID: %d\r\n", id);

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

				//clear buffer
				memset(&ebp, 0, sizeof(energy_debugger_breakpoint_info_t));
				memset(&ebpName, 0, sizeof(energy_debugger_ebp_name_t));

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

energy_debugger_status_t ENERGY_DEBUGGER_Init(uint32_t timeout)
{
    // Create the main task
	memset(&prvENERGY_DEBUGGER_DATA, 0, sizeof(energy_debugger_data_t));


    if(xTaskCreate(prvENERGY_DEBUGGER_Task,
            ENERGY_DEBUGGER_TASK_NAME,
            ENERGY_DEBUGGER_STACK_SIZE,
            NULL,
            ENERGY_DEBUGGER_TASK_PRIO,
            &prvENERGY_DEBUGGER_TASK_HANDLE) != pdTRUE) return ENERGY_DEBUGGER_STATUS_ERROR;



    prvENERGY_DEBUGGER_QUEUE_ID =  xQueueCreate(
    		ENERGY_DEBUGGER_ID_QUEUE_LENGTH,
			sizeof(uint32_t));

    if(prvENERGY_DEBUGGER_QUEUE_ID == NULL ) return ENERGY_DEBUGGER_STATE_ERROR;

    prvENERGY_DEBUGGER_QUEUE_EBP_NAME =  xQueueCreate(
    		CONF_ENERGY_DEBUGGER_EBP_NAMES_QUEUE_LENGTH,
			sizeof(energy_debugger_ebp_name_t));

    if(prvENERGY_DEBUGGER_QUEUE_EBP_NAME == NULL ) return ENERGY_DEBUGGER_STATE_ERROR;



    return ENERGY_DEBUGGER_STATUS_OK;
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
