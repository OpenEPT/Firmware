/**
 ******************************************************************************
 * @file   	control.h
 *
 * @brief  	Control service is in charge to receive and process control messages.
 * 			This service communicates with others services defined within this
 * 			firmware. Communications is mostly related to the configuration of
 * 			corresponding service or obtaining status messages from certain service
 * 			defined within control message content.
 * 			All control service interface functions, as well as public defines and
 * 			data types,are defined within this header file
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	December 2023
 ******************************************************************************
 */
#include <stdint.h>
#include "globalConfig.h"

#ifndef CORE_MIDDLEWARES_SERVICES_CONTROL_CONTROL_H_
#define CORE_MIDDLEWARES_SERVICES_CONTROL_CONTROL_H_
/**
 * @defgroup CONTROL_PUBLIC_DATA Control service public data and defines
 * @{
 */
/**
 * @defgroup CONTROL_PUBLIC_DEFINES Control service public defines
 * @{
 */
#define CONTROL_BUFFER_SIZE						CONFIG_CONTROL_BUFFER_SIZE				/*!< Request and response buffer sizes */
#define CONTROL_SERVER_PORT						CONFIG_CONTROL_SERVER_PORT				/*!< Control service TCP PORT number value */

#define CONTROL_TASK_NAME						CONFIG_CONTROL_TASK_NAME				/*!< Control service task name */
#define CONTROL_TASK_PRIO						CONFIG_CONTROL_PRIO						/*!< Control service task prio */
#define CONTROL_TASK_STACK						CONFIG_CONTROL_STACK_SIZE				/*!< Control service task stack size */

#define CONTROL_RESPONSE_OK_STATUS_MSG			CONF_CONTROL_RESPONSE_OK_STATUS_MSG		/*!< Default OK response message */
#define CONTROL_RESPONSE_ERROR_STATUS_MSG		CONF_CONTROL_RESPONSE_ERROR_STATUS_MSG	/*!< Default ERROR response message */

#define CONTROL_STATUS_LINK_MAX_NO				CONFIG_CONTROL_STATUS_LINK_MAX_NO		/*!< MAX Number of status links */
#define CONTROL_STATUS_LINK_TASK_NAME			CONFIG_CONTROL_STATUS_LINK_TASK_NAME	/*!< Status link task name */
#define CONTROL_STATUS_LINK_TASK_PRIO			CONFIG_CONTROL_STATUS_LINK_PRIO			/*!< Status link task prio */
#define CONTROL_STATUS_LINK_TASK_STACK			CONFIG_CONTROL_STATUS_LINK_STACK_SIZE	/*!< Status link task stack size */
#define CONTROL_STATUS_LINK_MESSAGES_MAX_NO		CONFIG_CONTROL_STATUS_MESSAGES_MAX_NO	/*!< Status messages queue buffer size */
/**
 * @}
 */
/**
 * @defgroup CONTROL_PUBLIC_STRUCTURES Control service public structures
 * @{
 */
/**
 * @brief Control service task state
 */
typedef enum
{
	CONTROL_STATE_INIT,			/*!< Control service initialization state */
	CONTROL_STATE_SERVICE,		/*!< Control service is in service state */
	CONTROL_STATE_ERROR			/*!< Control service is in error state */
}control_state_t;
/**
 * @brief Control service return status
 */
typedef enum{
	CONTROL_STATUS_OK,			/*!< Control service status OK */
	CONTROL_STATUS_ERROR		/*!< Control service status Error */
}control_status_t;
/**
 * @brief Link state
 */
typedef enum{
	CONTROL_LINK_STATE_UP,		/*!< Link is up */
	CONTROL_LINK_STATE_DOWN		/*!< Link is down */
}control_link_state_t;
/**
 * @brief Status link IP info
 */
typedef struct
{
	uint8_t		ip[4];			/*!< IP Address buffer */
	uint16_t	portNo;			/*!< Port number */
}control_status_link_ip_info_t;
/**
 * @brief Status link instance handler
 */
typedef struct{
	uint32_t						linkInstanceNo;
	control_status_link_ip_info_t 	ipInfo;
}control_status_link_instance_t;
/**
 * @}
 */
/**
 * @}
 */
/**
 * @defgroup CONTROL_PUBLIC_FUNCTIONS Control service interface functions
 * @{
 */
/**
 * @brief	Initialize control service
 * @param	initTimeout: timeout initialization
 * @retval	::control_status_t
 */
control_status_t 	CONTROL_Init(uint32_t initTimeout);
/**
 * @brief	Report control service that link is closed
 *
 * 			This function is usually used by Network service to report Link
 * 			status. Based on the number of this function calls, Control service
 * 			close opened links
 *
 * @retval	::control_status_t
 */
control_status_t 	CONTROL_LinkClosed();
/**
 * @brief	Create status link
 *
 * 			Create status link thread and try to establish communication with control
 * 			link server.
 *
 * @param	control_status_link_instance_t: control link instance. If control link is successfully
 * 			created this argument should be used to access control link with all functions related
 * 			to control link.  See ::control_status_link_instance_t structure
 * @param	statusServerIp: server IP information. See ::control_status_link_ip_info_t
 * @param	timeout: timeout value to wait for status link to be created
 * @retval	::control_status_t
 */
control_status_t 	CONTROL_StatusLinkCreate(control_status_link_instance_t* statusLinkInstance, control_status_link_ip_info_t statusServerIp, uint32_t timeout);
/**
 * @brief	Send status message over control link
 *
 * @param	control_status_link_instance_t: pointer to previously created status link
 * 			to control link.  See ::control_status_link_instance_t structure
 * @param	message: message to send
 * @param	messageSize: message size
 * @param	timeout: timeout interval to wait for message to be sent over status link
 * @retval	::control_status_t
 */
control_status_t 	CONTROL_StatusLinkSendMessage(control_status_link_instance_t* statusLinkInstance, const char* message, uint32_t messageSize, uint32_t timeout);
/**
 * @}
 */
#endif /* CORE_MIDDLEWARES_SERVICES_CONTROL_CONTROL_H_ */
