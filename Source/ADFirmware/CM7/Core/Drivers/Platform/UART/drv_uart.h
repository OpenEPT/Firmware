/**
 ******************************************************************************
 * @file   	drv_uart.h
 * @brief  	...
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_UART_UART_H_
#define CORE_DRIVERS_PLATFORM_UART_UART_H_

#include <stdint.h>
#include "globalConfig.h"

#define DRV_UART_INSTANCES_MAX_NUMBER	CONF_UART_INSTANCES_MAX_NUMBER

typedef enum
{
	DRV_UART_STATUS_OK,
	DRV_UART_STATUS_ERROR
}drv_uart_status_t;

typedef enum
{
	DRV_UART_INSTANCE_1	= 0,
	DRV_UART_INSTANCE_3	= 1,
	DRV_UART_INSTANCE_7	= 2,
	DRV_UART_INSTANCE_6	= 3,
}drv_uart_instance_t;

typedef enum
{
	DRV_UART_STOPBIT_1,
	DRV_UART_STOPBIT_2
}drv_uart_stopbit_t;

typedef enum
{
	DRV_UART_PARITY_NONE,
	DRV_UART_PARITY_ODD,
	DRV_UART_PARITY_EVEN
}drv_uart_parity_t;

typedef enum
{
	DRV_UART_INITIALIZATION_STATUS_NOINIT	=	0,
	DRV_UART_INITIALIZATION_STATUS_INIT		=	1
}drv_uart_initialization_status_t;


typedef struct
{
	uint32_t			baudRate;
	drv_uart_stopbit_t	stopBitNo;
	drv_uart_stopbit_t	parityEnable;
}drv_uart_config_t;



typedef void (*drv_uart_rx_isr_callback)(uint8_t data);


drv_uart_status_t	DRV_UART_Init();
drv_uart_status_t	DRV_UART_Instance_Init(drv_uart_instance_t instance, drv_uart_config_t* config);
drv_uart_status_t	DRV_UART_Instance_TransferData(drv_uart_instance_t instance, uint8_t* buffer, uint8_t size, uint32_t timeout);
drv_uart_status_t	DRV_UART_Instance_RegisterRxCallback(drv_uart_instance_t instance, drv_uart_rx_isr_callback rxcb);


#endif /* CORE_DRIVERS_PLATFORM_UART_UART_H_ */
