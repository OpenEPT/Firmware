/*
 * globalConfig.h
 *
 *  Created on: Nov 5, 2023
 *      Author: Haris
 */

#ifndef CORE_CONFIGURATION_GLOBALCONFIG_H_
#define CORE_CONFIGURATION_GLOBALCONFIG_H_


/*Driver layer connfiguration*/
/* AnalogIN configuration*/
#define CONF_AIN_MAX_BUFFER_SIZE			500
#define CONF_AIN_ADC_BUFFER_OFFSET			4
#define CONF_AIN_ADC_BUFFER_MARKER			0xA5A5
#define CONF_AIN_MAX_BUFFER_NO				2
#define CONF_DRV_AIN_ADC_TIM_INPUT_CLK		200000000 //HZ

/* Timer configuration*/
#define CONF_DRV_TIMER_MAX_NUMBER_OF_TIMERS 	3
#define CONF_DRV_TIMER_MAX_NUMBER_OF_CHANNELS 	3

/* GPIO configuration*/
#define CONF_GPIO_PORT_MAX_NUMBER			10
#define CONF_GPIO_PIN_MAX_NUMBER			16
#define	CONF_GPIO_INTERRUPTS_MAX_NUMBER		15


#define	CONF_UART_INSTANCES_MAX_NUMBER		4



/*Middleware layer configuration*/
/**/
#define CONF_SSTREAM_CONTROL_TASK_NAME			"Stream control"
#define CONF_SSTREAM_CONTROL_TASK_PRIO			4
#define CONF_SSTREAM_CONTROL_TASK_STACK_SIZE	1024
#define CONF_SSTREAM_STREAM_TASK_NAME			"Stream"
#define CONF_SSTREAM_STREAM_TASK_PRIO			4
#define CONF_SSTREAM_STREAM_TASK_STACK_SIZE		1024
#define CONF_SSTREAM_CONNECTIONS_MAX_NO			2
#define	CONF_SSTREAM_AIN_DEFAULT_RESOLUTION		16
#define	CONF_SSTREAM_AIN_DEFAULT_CLOCK_DIV		1
#define	CONF_SSTREAM_AIN_DEFAULT_CH_SAMPLE_TIME	2
#define	CONF_SSTREAM_AIN_DEFAULT_CH_AVG_RATIO	1
#define	CONF_SSTREAM_AIN_DEFAULT_SAMPLE_TIME	1000
#define CONF_SSTREAM_AIN_VOLTAGE_CHANNEL		1
#define	CONF_SSTREAM_AIN_CURRENT_CHANNEL		2
#define CONF_SSTREAM_AIN_DEFAULT_PRESCALER		49999
#define CONF_SSTREAM_AIN_DEFAULT_PERIOD			3

/**/
#define CONF_SYSTEM_TASK_NAME					"System task"
#define CONF_SYSTEM_TASK_PRIO					5
#define CONF_SYSTEM_TASK_STACK_SIZE				1024
#define CONF_SYSTEM_DEFAULT_DEVICE_NAME			"ACQ Device"
#define CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX		50

#define	CONF_SYSTEM_ERROR_STATUS_DIODE_PORT		1	//Port B
#define	CONF_SYSTEM_ERROR_STATUS_DIODE_PIN		14

#define	CONF_SYSTEM_LINK_STATUS_DIODE_PORT		4	//Port E
#define	CONF_SYSTEM_LINK_STATUS_DIODE_PIN		1

/**/
#define CONF_NETWORK_TASK_NAME					"Network Task"
#define CONF_NETWORK_TASK_PRIO					3
#define CONF_NETWORK_TASK_STACK_SIZE			1024

#define CONFIG_LOGGING_TASK_NAME				"LOG Task"
#define	CONFIG_LOGGING_STACK_SIZE				1024
#define	CONFIG_LOGGING_PRIO						3

#define CONFIG_CONTROL_TASK_NAME				"Control Task"
#define CONFIG_CONTROL_PRIO						4
#define CONFIG_CONTROL_STACK_SIZE				2048
#define CONFIG_CONTROL_BUFFER_SIZE				1024
#define CONFIG_CONTROL_SERVER_PORT				5000
#define CONF_CONTROL_RESPONSE_OK_STATUS_MSG		"OK"
#define CONF_CONTROL_RESPONSE_ERROR_STATUS_MSG	"ERROR"
#define CONFIG_CONTROL_STATUS_LINK_MAX_NO		3
#define CONFIG_CONTROL_STATUS_LINK_TASK_NAME	"Status Link Task"
#define CONFIG_CONTROL_STATUS_LINK_PRIO			7
#define CONFIG_CONTROL_STATUS_LINK_STACK_SIZE	1024
#define CONFIG_CONTROL_STATUS_MESSAGES_MAX_NO	20

#define CONF_NETWORK_DEVICE_IP_ADDRESS			"192.168.2.110"
#define CONF_NETWORK_DEVICE_IP_MASK				"255.255.255.0"
#define CONF_NETWORK_DEVICE_IP_GW				"192.168.2.1"

#define CONF_ENERGY_DEBUGGER_TASK_NAME				"Energy Debugger Task"
#define CONF_ENERGY_DEBUGGER_TASK_PRIO				5
#define CONF_ENERGY_DEBUGGER_STACK_SIZE				1024
#define CONF_ENERGY_DEBUGGER_BUTTON_PIN				13
#define CONF_ENERGY_DEBUGGER_BUTTON_PIN				13
#define CONF_ENERGY_DEBUGGER_BUTTON_ISR_PRIO		5 //configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
#define CONF_ENERGY_DEBUGGER_ID_QUEUE_LENGTH		100
#define CONF_ENERGY_DEBUGGER_MESSAGE_BUFFER_LENTH 		100
#define CONF_ENERGY_DEBUGGER_TCP_MESSAGE_BUFFER_LENTH 	200
#define CONF_ENERGY_DEBUGGER_EBP_NAMES_QUEUE_LENGTH	10
#define CONF_ENERGY_DEBUGGER_EBP_QUEUE_LENGTH		10
#define CONF_ENERGY_DEBUGGER_MAX_CONNECTIONS        3

#endif /* CORE_CONFIGURATION_GLOBALCONFIG_H_ */
