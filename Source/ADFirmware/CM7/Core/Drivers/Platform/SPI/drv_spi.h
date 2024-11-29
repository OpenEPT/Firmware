/*
 * drv_spi.h
 *
 *  Created on: Jun 30, 2024
 *      Author: Pavle Lakic & Dimitrije Lilic
 */

#ifndef CORE_DRIVERS_PLATFORM_SPI_DRV_SPI_H_
#define CORE_DRIVERS_PLATFORM_SPI_DRV_SPI_H_

#include <stdint.h>
#include "globalConfig.h"

typedef enum drv_spi_initialization_status_t
{
	DRV_SPI_INITIALIZATION_STATUS_NOINIT	=	0,
	DRV_SPI_INITIALIZATION_STATUS_INIT		=	1
}drv_spi_initialization_status_t;

typedef enum drv_spi_status_t
{
	DRV_SPI_STATUS_OK,
	DRV_SPI_STATUS_ERROR
}drv_spi_status_t;

typedef enum drv_spi_instance_t
{
	DRV_SPI_INSTANCE3 = 0
}drv_spi_instance_t;

typedef enum drv_spi_mode_t
{
	DRV_SPI_MODE_SLAVE = 0,
	DRV_SPI_MODE_MASTER = 1
}drv_spi_mode_t;

typedef enum drv_spi_direction_t
{
	DRV_SPI_DIRECTION_2LINES = 0,
	DRV_SPI_DIRECTION_2LINES_TXONLY = 1,
	DRV_SPI_DIRECTION_2LINES_RXONLY = 2,
	DRV_SPI_DIRECTION_1LINE = 3
}drv_spi_direction_t;

typedef enum drv_spi_size_t
{
	DRV_SPI_DATA_SIZE_8BITS = 0,
	DRV_SPI_DATA_SIZE_16BITS = 1
}drv_spi_size_t;

typedef enum drv_spi_polarity_t
{
	DRV_SPI_POLARITY_LOW = 0,
	DRV_SPI_POLARITY_HIGH = 1
}drv_spi_polarity_t;

typedef enum drv_spi_phase_t
{
	DRV_SPI_PHASE_1EDGE = 0,
	DRV_SPI_PHASE_2EDGE = 1
}drv_spi_phase_t;

typedef enum drv_spi_clock_prescaler_t
{
	DRV_SPI_BAUDRATEPRESCALER_2 = 0,
	DRV_SPI_BAUDRATEPRESCALER_4 = 1,
	DRV_SPI_BAUDRATEPRESCALER_8 = 2,
	DRV_SPI_BAUDRATEPRESCALER_16 = 3,
	DRV_SPI_BAUDRATEPRESCALER_32 = 4,
	DRV_SPI_BAUDRATEPRESCALER_64 = 5,
	DRV_SPI_BAUDRATEPRESCALER_128 = 6,
	DRV_SPI_BAUDRATEPRESCALER_256 = 7
}drv_spi_clock_prescaler_t;

typedef enum drv_spi_first_bit_t
{
	DRV_SPI_FIRSTBIT_MSB = 0,
	DRV_SPI_FIRSTBIT_LSB = 1
}drv_spi_first_bit_t;

typedef enum drv_spi_fifo_t
{
	DRV_SPI_FIFO_THRESHOLD_01DATA = 0
}drv_spi_fifo_t;

typedef struct drv_spi_config_t
{
	drv_spi_mode_t		mode;
	drv_spi_polarity_t	polarity;
	drv_spi_phase_t		phase;
}drv_spi_config_t;

drv_spi_status_t	DRV_SPI_Init();
drv_spi_status_t	DRV_SPI_Instance_Init(drv_spi_instance_t instance, drv_spi_config_t* config);
drv_spi_status_t	DRV_SPI_Instance_DeInit(drv_spi_instance_t instance);
drv_spi_status_t	DRV_SPI_TransmitData(uint8_t* buffer, uint8_t size, uint32_t timeout);
drv_spi_status_t	DRV_SPI_ReceiveData(uint8_t* buffer, uint8_t size, uint32_t timeout);

/**/
void				MX_SPI4_Init();
void				MX_SPI5_Init();

#endif /* CORE_DRIVERS_PLATFORM_SPI_DRV_SPI_H_ */
