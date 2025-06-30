/**
 ******************************************************************************
 * @file   	drv_uart.h
 *
 * @brief  	UART driver provides hardware abstraction layer for STM32 UART
 * 			peripherals including UART1, UART3, UART4, UART6, and UART7.
 * 			This driver supports UART initialization, configuration of baud
 * 			rate, parity, and stop bits, data transmission with timeout control,
 * 			and interrupt-driven data reception with callback mechanisms.
 * 			The driver enables reliable serial communication for various
 * 			applications including debugging, data logging, and communication
 * 			with external devices.
 * 			All UART driver interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_UART_UART_H_
#define CORE_DRIVERS_PLATFORM_UART_UART_H_

#include <stdint.h>
#include "globalConfig.h"

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup UART_DRIVER UART Driver
 * @{
 */

/**
 * @defgroup UART_PUBLIC_DEFINES UART driver public defines
 * @{
 */
#define DRV_UART_INSTANCES_MAX_NUMBER	CONF_UART_INSTANCES_MAX_NUMBER		/*!< Maximum number of UART instances */
/**
 * @}
 */

/**
 * @defgroup UART_PUBLIC_TYPES UART driver public data types
 * @{
 */

/**
 * @brief UART driver return status
 */
typedef enum
{
	DRV_UART_STATUS_OK,				/*!< UART operation successful */
	DRV_UART_STATUS_ERROR			/*!< UART operation failed */
}drv_uart_status_t;

/**
 * @brief Available UART instances
 */
typedef enum
{
	DRV_UART_INSTANCE_1	= 0,		/*!< UART1 instance */
	DRV_UART_INSTANCE_3	= 1,		/*!< UART3 instance */
	DRV_UART_INSTANCE_7	= 2,		/*!< UART7 instance */
	DRV_UART_INSTANCE_6	= 3,		/*!< UART6 instance */
	DRV_UART_INSTANCE_4 = 4			/*!< UART4 instance */
}drv_uart_instance_t;

/**
 * @brief UART stop bit configuration
 */
typedef enum
{
	DRV_UART_STOPBIT_1,				/*!< 1 stop bit */
	DRV_UART_STOPBIT_2				/*!< 2 stop bits */
}drv_uart_stopbit_t;

/**
 * @brief UART parity configuration
 */
typedef enum
{
	DRV_UART_PARITY_NONE,			/*!< No parity */
	DRV_UART_PARITY_ODD,			/*!< Odd parity */
	DRV_UART_PARITY_EVEN			/*!< Even parity */
}drv_uart_parity_t;

/**
 * @brief UART initialization status
 */
typedef enum
{
	DRV_UART_INITIALIZATION_STATUS_NOINIT	=	0,		/*!< UART not initialized */
	DRV_UART_INITIALIZATION_STATUS_INIT		=	1		/*!< UART initialized */
}drv_uart_initialization_status_t;

/**
 * @brief UART configuration structure
 */
typedef struct
{
	uint32_t			baudRate;		/*!< UART baud rate in bps */
	drv_uart_stopbit_t	stopBitNo;		/*!< Number of stop bits. See ::drv_uart_stopbit_t */
	drv_uart_parity_t	parityEnable;	/*!< Parity configuration. See ::drv_uart_parity_t */
}drv_uart_config_t;

/**
 * @brief UART receive interrupt callback function pointer type
 * @param data: Received data byte
 */
typedef void (*drv_uart_rx_isr_callback)(uint8_t data);

/**
 * @}
 */

/**
 * @defgroup UART_PUBLIC_FUNCTIONS UART driver interface functions
 * @{
 */

/**
 * @brief	Initialize UART driver subsystem
 * @details	This function initializes the UART driver subsystem, sets up
 * 			internal data structures, and prepares the driver for UART
 * 			instance initialization. Must be called before any UART
 * 			instance operations.
 * @retval	::drv_uart_status_t
 */
drv_uart_status_t	DRV_UART_Init();

/**
 * @brief	Initialize specific UART instance
 * @details	This function initializes a specific UART instance with the
 * 			provided configuration including baud rate, stop bits, and
 * 			parity settings. The UART peripheral is configured and ready
 * 			for data transmission and reception.
 * @param	instance: UART instance to initialize. See ::drv_uart_instance_t
 * @param	config: Pointer to UART configuration structure. See ::drv_uart_config_t
 * @retval	::drv_uart_status_t
 */
drv_uart_status_t	DRV_UART_Instance_Init(drv_uart_instance_t instance, drv_uart_config_t* config);

/**
 * @brief	Transmit data through UART instance
 * @details	This function transmits data through the specified UART instance
 * 			with timeout control. The function blocks until all data is
 * 			transmitted or timeout occurs.
 * @param	instance: UART instance for transmission. See ::drv_uart_instance_t
 * @param	buffer: Pointer to data buffer to transmit
 * @param	size: Number of bytes to transmit
 * @param	timeout: Transmission timeout in milliseconds
 * @retval	::drv_uart_status_t
 */
drv_uart_status_t	DRV_UART_Instance_TransferData(drv_uart_instance_t instance, uint8_t* buffer, uint8_t size, uint32_t timeout);

/**
 * @brief	Register receive interrupt callback function
 * @details	This function registers a callback function that will be called
 * 			when data is received on the specified UART instance. The callback
 * 			is executed in interrupt context and should be kept short.
 * @param	instance: UART instance. See ::drv_uart_instance_t
 * @param	rxcb: Callback function pointer. See ::drv_uart_rx_isr_callback
 * @retval	::drv_uart_status_t
 */
drv_uart_status_t	DRV_UART_Instance_RegisterRxCallback(drv_uart_instance_t instance, drv_uart_rx_isr_callback rxcb);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_DRIVERS_PLATFORM_UART_UART_H_ */