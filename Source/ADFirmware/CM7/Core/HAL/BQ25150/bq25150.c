/*
 * bq25150.c
 *
 *  Created on: Apr 12, 2025
 *      Author: elektronika
 */
#include <stdint.h>

#include "bq25150.h"

#include "drv_i2c.h"
#include "drv_gpio.h"

#define		BQ250150_CHARGE_EN_PORT		4
#define		BQ250150_CHARGE_EN_PIN		8

#define 	BQ25150_DEV_ADDR			0x6B
#define 	BQ25150_DEV_ID				0x20

#define 	BQ25150_REG_VBAT_CTRL		0x12
#define 	BQ25150_REG_ICHG_CTRL		0x13
#define 	BQ25150_REG_TERMCTRL		0x15
#define 	BQ25150_REG_CHARGERCTRL0	0x17
#define 	BQ25150_REG_ILIMCTRL		0x19
#define 	BQ25150_REG_ICCTRL2			0x37
#define 	BQ25150_REG_ID				0x6F


static bq25150_status_t prvBQ25150_ReadReg(uint8_t reg, uint8_t* data, uint32_t timeout)
{
	uint8_t addr;
	uint8_t dataTx[2];
	uint8_t dataRx[2];

	/*Send register */
	addr = BQ25150_DEV_ADDR;
	addr = (addr << 1) | 0x00;
	dataTx[0] = reg;
	if(DRV_I2C_Transmit(DRV_I2C_INSTANCE_1, addr, dataTx, 1, timeout) != DRV_I2C_STATUS_OK) return BQ25150_STATUS_ERROR;

	/*Read register data */
	addr = BQ25150_DEV_ADDR;
	dataRx[0] = 0;
	addr = (addr << 1) | 0x01;
	if(DRV_I2C_Receive(DRV_I2C_INSTANCE_1, addr, dataRx, 1, timeout)!= DRV_I2C_STATUS_OK) return BQ25150_STATUS_ERROR;

	*data = dataRx[0];

    return BQ25150_STATUS_OK;
}


static bq25150_status_t prvBQ25150_WriteReg(uint8_t reg, uint8_t data, uint8_t writeCheck, uint32_t timeout)
{
	uint8_t addr;
	uint8_t dataTx[2];
	uint8_t readData;

	addr = BQ25150_DEV_ADDR;
	addr = (addr << 1) | 0x00;
	dataTx[0] = reg;
	dataTx[1] = data;

	if(DRV_I2C_Transmit(DRV_I2C_INSTANCE_1, addr, dataTx, 2, timeout) != DRV_I2C_STATUS_OK) return BQ25150_STATUS_ERROR;

	if(writeCheck == 1)
	{
		if(prvBQ25150_ReadReg(reg, &readData, timeout) != BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

		if(readData != data) return BQ25150_STATUS_ERROR;
	}

    return BQ25150_STATUS_OK;
}



bq25150_status_t BQ25150_Init()
{

	drv_gpio_pin_init_conf_t 	chargeEnPin;

	if(DRV_I2C_Instance_Init(DRV_I2C_INSTANCE_1, 0) != DRV_I2C_STATUS_OK) return BQ25150_STATUS_ERROR;

	/* Init Acqusition State Diode */
	chargeEnPin.mode		 = DRV_GPIO_PIN_MODE_OUTPUT_PP;
	chargeEnPin.pullState	 = DRV_GPIO_PIN_PULL_NOPULL;

	if(DRV_GPIO_Port_Init(BQ250150_CHARGE_EN_PORT) != DRV_GPIO_STATUS_OK) return BQ25150_STATUS_ERROR;
	if(DRV_GPIO_Pin_Init(BQ250150_CHARGE_EN_PORT, BQ250150_CHARGE_EN_PIN, &chargeEnPin) != DRV_GPIO_STATUS_OK) return BQ25150_STATUS_ERROR;
    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_Ping(uint32_t timeout)
{
	uint8_t data = 0;
	if(prvBQ25150_ReadReg(BQ25150_REG_ID, &data, timeout) != BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

	if(data != BQ25150_DEV_ID) return BQ25150_STATUS_ERROR;

    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_Charge_Enable(uint32_t timeout)
{


    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_Charge_Current_Set(uint32_t current, uint32_t timeout)
{
	float regValue = (((float)current)/1.25);
	int regData = (int)regValue;

	if(prvBQ25150_WriteReg(BQ25150_REG_ICHG_CTRL, regData, 1, 1000)!= BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_Charge_Current_Get(uint32_t* current, uint32_t timeout)
{
	uint8_t regData = 0;
	if(prvBQ25150_ReadReg(BQ25150_REG_ICHG_CTRL, &regData, 1000) != BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

	*current = (uint32_t)((float)regData*1.25);

    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_Charge_PreCurrent_Set(uint32_t current, uint32_t timeout)
{
    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_Charge_PreCurrent_Get(uint32_t* current, uint32_t timeout)
{
    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_Charge_TermCurrent_Set(uint32_t current, uint32_t timeout)
{
	if(current > 32) return BQ25150_STATUS_ERROR;
	uint8_t regData = 0;

	if(prvBQ25150_ReadReg(BQ25150_REG_TERMCTRL, &regData, 1000) != BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

	/*Clear previous value*/
	regData &= 0b11000001;

	regData |= ((uint8_t)current) << 1;

	if(prvBQ25150_WriteReg(BQ25150_REG_TERMCTRL, regData, 1, 1000)!= BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_Charge_TermCurrent_Get(uint32_t* current, uint32_t timeout)
{
	uint8_t regData = 0;

	if(prvBQ25150_ReadReg(BQ25150_REG_TERMCTRL, &regData, 1000) != BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

	*current = regData;

    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_Charge_RegVoltage_Set(float voltage, uint32_t timeout)
{
	float volDiff = voltage - 3.6;
	float volReg = volDiff/0.01;
	int regData = volReg;

	if(prvBQ25150_WriteReg(BQ25150_REG_VBAT_CTRL, regData, 1, 1000)!= BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_Charge_RegVoltage_Get(float* voltage, uint32_t timeout)
{
	uint8_t regData = 0;

	if(prvBQ25150_ReadReg(BQ25150_REG_VBAT_CTRL, &regData, 1000) != BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

	*voltage = (float)(((float)(regData)*0.01) + 3.6);

    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_WDG_SetStatus(bq25150_wdg_status status, uint32_t timeout)
{
	uint8_t regData = 0;

	if(prvBQ25150_ReadReg(BQ25150_REG_CHARGERCTRL0, &regData, 1000) != BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

	if(status == BQ25150_WDG_STATUS_DISABLE)
	{
		regData |= 1 << 4;
	}
	else
	{
		regData &= ~(1 << 4);
	}

	if(prvBQ25150_WriteReg(BQ25150_REG_CHARGERCTRL0, regData, 1, 1000)!= BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;
    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_ILim_Set(bq25250_ilim_value_t value, uint32_t timeout)
{
	uint8_t regData = 0;

	if(prvBQ25150_ReadReg(BQ25150_REG_ILIMCTRL, &regData, 1000) != BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

	regData |= (uint8_t)value;

	if(prvBQ25150_WriteReg(BQ25150_REG_ILIMCTRL, regData, 1, 1000)!= BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_Charge_SetStatus(bq25150_charge_status status, uint32_t timeout)
{
	/**/
	uint8_t regData = 0;
	if(prvBQ25150_ReadReg(BQ25150_REG_ICCTRL2, &regData, 1000) != BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;
	/**/
	if(status == BQ25150_CHARGE_STATUS_ENABLE)
	{
		regData &= ~0x01;

		if(prvBQ25150_WriteReg(BQ25150_REG_ICCTRL2, regData, 1, 1000)!= BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

		if(DRV_GPIO_Pin_SetState(BQ250150_CHARGE_EN_PORT, BQ250150_CHARGE_EN_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK) return BQ25150_STATUS_ERROR;

	}
	else
	{
		regData |= 0x01;

		if(prvBQ25150_WriteReg(BQ25150_REG_ICCTRL2, regData, 1, 1000)!= BQ25150_STATUS_OK) return BQ25150_STATUS_ERROR;

		if(DRV_GPIO_Pin_SetState(BQ250150_CHARGE_EN_PORT, BQ250150_CHARGE_EN_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK) return BQ25150_STATUS_ERROR;
	}

    return BQ25150_STATUS_OK;
}

bq25150_status_t BQ25150_ReadReg(uint8_t regAddr, uint8_t* data, uint32_t timeout)
{
    return prvBQ25150_ReadReg(regAddr, data, timeout);
}
