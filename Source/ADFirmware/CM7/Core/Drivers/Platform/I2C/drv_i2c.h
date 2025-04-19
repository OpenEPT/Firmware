/*
 * drv_i2c.h
 *
 *  Created on: Apr 10, 2025
 *      Author: elektronika
 */

#ifndef CORE_DRIVERS_PLATFORM_I2C_DRV_I2C_H_
#define CORE_DRIVERS_PLATFORM_I2C_DRV_I2C_H_

typedef enum
{
	DRV_I2C_INITIALIZATION_STATUS_NOINIT	=	0,
	DRV_I2C_INITIALIZATION_STATUS_INIT		=	1
}drv_i2c_initialization_status_t;

typedef enum
{
	DRV_I2C_STATUS_OK,
	DRV_I2C_STATUS_ERROR
}drv_i2c_status_t;

typedef enum
{
	DRV_I2C_INSTANCE_1 = 0
}drv_i2c_instance_t;

typedef struct
{
	uint32_t clkFreq;
}drv_i2c_config_t;


drv_i2c_status_t 	DRV_I2C_Init();
drv_i2c_status_t	DRV_I2C_Instance_Init(drv_i2c_instance_t instance, drv_i2c_config_t* config);
drv_i2c_status_t	DRV_I2C_Transmit(drv_i2c_instance_t instance, uint8_t addr,uint8_t* data, uint32_t size, uint32_t timeout);
drv_i2c_status_t	DRV_I2C_Receive(drv_i2c_instance_t instance, uint8_t addr,uint8_t* data, uint32_t size, uint32_t timeout);


#endif /* CORE_DRIVERS_PLATFORM_I2C_DRV_I2C_H_ */
