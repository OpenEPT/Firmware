/**
 ******************************************************************************
 * @file    bq25180.c
 *
 * @brief   BQ25180 battery charger IC driver implementation providing hardware
 *          abstraction layer for controlling and monitoring the TI BQ25180
 *          linear battery charger. This driver supports charging control, 
 *          current limit settings, interrupt handling, ADC monitoring, watchdog
 *          timer configuration, and register access for comprehensive charger
 *          management.
 *
 * @author  Dimitrije Lilic, Haris Turkmanovic
 * @date    April 2026
 ******************************************************************************
 */
#include <stdint.h>

#include "bq25180.h"

#include "drv_i2c.h"
#include "drv_gpio.h"

/**
 * @defgroup HAL Hardware Abstraction Layer
 * @{
 */

/**
 * @defgroup BQ25180_DRIVER BQ25180 Battery Charger Driver
 * @{
 */

/**
 * @defgroup BQ25180_PRIVATE_DEFINES BQ25180 driver defines and default values
 * @{
 */
#define BQ25180_CHARGE_INT_PORT        2     	/*!< GPIO port used for BQ25180 interrupt signal */
#define BQ25180_CHARGE_INT_PIN         2      	/*!< GPIO pin used for BQ25180 interrupt signal */
#define BQ25180_CHARGE_INT_PRIO        5       /*!< Interrupt priority for BQ25180 interrupt line */

#define BQ25180_DEV_ADDR               0x6A    /*!< BQ25180 I2C slave device address */
#define BQ25180_DEV_ID                 0xC0    /*!< Expected BQ25180 device identifier value */

#define BQ25180_REG_STAT0              0x00    /*!< Status register 0 */
#define BQ25180_REG_STAT1              0x01    /*!< Status register 1 */
#define BQ25180_REG_IFLAG0             0x02    /*!< Interrupt flag register 0 */

#define BQ25180_REG_VBAT_CTRL          0x03    /*!< Battery regulation voltage control register */
#define BQ25180_REG_ICHG_CTRL          0x04    /*!< Charge current control register */

#define BQ25180_REG_CHARGERCTRL0       0x05    /*!< Charger control register 0 */
#define BQ25180_REG_CHARGERCTRL1       0x06    /*!< Charger control register 1 */

#define BQ25180_REG_IC_CTRL            0x07    /*!< Input current limit control register */
#define BQ25180_REG_TMR_ILIM           0x08    /*!< Safety timer and input current limit register */
#define BQ25180_REG_SHIP_RST           0x09    /*!< Ship mode and reset control register */
#define BQ25180_REG_SYS_REG            0x0A    /*!< System configuration register */
#define BQ25180_REG_TS_CONTROL         0x0B    /*!< Temperature sensing control register */

#define BQ25180_REG_ID                 0x0C    /*!< Device identification register */
/**
 * @}
 */

bq25180_intcb prvBQ25180_CB;

/**
 * @defgroup BQ25180_PRIVATE_FUNCTIONS BQ25180 driver private functions
 * @{
 */

/**
 * @brief Interrupt callback handler for BQ25180 interrupts
 * 
 * This function is called when the BQ25180 interrupt pin changes state.
 * It forwards the interrupt to the registered user callback function.
 *
 * @param GPIO_Pin The GPIO pin that triggered the interrupt
 * @retval None
 */
static void prvBQ25180_IntCallback(uint16_t GPIO_Pin)
{
	if(prvBQ25180_CB != 0)
	{
		prvBQ25180_CB();
	}
}

/**
 * @brief Read data from a BQ25180 register
 * 
 * This function reads the content of a specified register from the BQ25180 device
 * using I2C communication.
 *
 * @param reg Register address to read
 * @param data Pointer to store the read data
 * @param timeout Communication timeout in milliseconds
 * @retval ::bq25180_status_t BQ25180_STATUS_OK if successful, BQ25180_STATUS_ERROR otherwise
 */
static bq25180_status_t prvBQ25180_ReadReg(uint8_t reg, uint8_t* data, uint32_t timeout)
{
	uint8_t addr;
	uint8_t dataTx[1];//[2];
	uint8_t dataRx[1];//[2];

	/*Send register */
	addr = BQ25180_DEV_ADDR;
	addr = (addr << 1) | 0x00;
	dataTx[0] = reg;
	if(DRV_I2C_Transmit(DRV_I2C_INSTANCE_1, addr, dataTx, 1, timeout) != DRV_I2C_STATUS_OK) return BQ25180_STATUS_ERROR;

	/*Read register data */
	addr = BQ25180_DEV_ADDR;
	dataRx[0] = 0;
	addr = (addr << 1) | 0x01;
	if(DRV_I2C_Receive(DRV_I2C_INSTANCE_1, addr, dataRx, 1, timeout)!= DRV_I2C_STATUS_OK) return BQ25180_STATUS_ERROR;

	*data = dataRx[0];

    return BQ25180_STATUS_OK;
}

/**
 * @brief Write data to a BQ25180 register
 * 
 * This function writes data to a specified register of the BQ25180 device
 * using I2C communication. Optionally verifies the write by reading back.
 *
 * @param reg Register address to write to
 * @param data Data byte to write
 * @param writeCheck If 1, verify write by reading back
 * @param timeout Communication timeout in milliseconds
 * @retval ::bq25180_status_t BQ25180_STATUS_OK if successful, BQ25180_STATUS_ERROR otherwise
 */
static bq25180_status_t prvBQ25180_WriteReg(uint8_t reg, uint8_t data, uint8_t writeCheck, uint32_t timeout)
{
	uint8_t addr;
	uint8_t dataTx[2];
	uint8_t readData;

	addr = BQ25180_DEV_ADDR;
	addr = (addr << 1) | 0x00;
	dataTx[0] = reg;
	dataTx[1] = data;

	if(DRV_I2C_Transmit(DRV_I2C_INSTANCE_1, addr, dataTx, 2, timeout) != DRV_I2C_STATUS_OK) return BQ25180_STATUS_ERROR;

	if(writeCheck == 1)
	{
		if(prvBQ25180_ReadReg(reg, &readData, timeout) != BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;

		if(readData != data) return BQ25180_STATUS_ERROR;
	}

    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_Init()
{

	drv_gpio_pin_init_conf_t 	chargeEnPin;
	drv_i2c_config_t config;

	config.clkFreq = 100000;

	if(DRV_I2C_Instance_Init(DRV_I2C_INSTANCE_1, &config) != DRV_I2C_STATUS_OK)
		return BQ25180_STATUS_ERROR;

	// Configure the pin for the button
	drv_gpio_pin_init_conf_t button_pin_conf;
	button_pin_conf.mode = DRV_GPIO_PIN_MODE_IT_FALLING;
	button_pin_conf.pullState = DRV_GPIO_PIN_PULL_NOPULL;


	if(DRV_GPIO_Port_Init(BQ25180_CHARGE_INT_PORT) != DRV_GPIO_STATUS_OK)
		return BQ25180_STATUS_ERROR;

	if (DRV_GPIO_Pin_Init(BQ25180_CHARGE_INT_PORT, BQ25180_CHARGE_INT_PIN, &button_pin_conf) != DRV_GPIO_STATUS_OK)
		return BQ25180_STATUS_ERROR;

	if (DRV_GPIO_RegisterCallback(BQ25180_CHARGE_INT_PORT, BQ25180_CHARGE_INT_PIN, prvBQ25180_IntCallback, BQ25180_CHARGE_INT_PRIO) != DRV_GPIO_STATUS_OK)
		return BQ25180_STATUS_ERROR;

	prvBQ25180_CB = 0;

	return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_Ping(uint32_t timeout)
{
	uint8_t data = 0;
	if(prvBQ25180_ReadReg(BQ25180_REG_ID, &data, timeout) != BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;

	if((data & 0xFF) != (BQ25180_DEV_ID)) return BQ25180_STATUS_ERROR;

    return BQ25180_STATUS_OK;
}







bq25180_status_t BQ25180_GetChargerIntMask(uint8_t* mask, uint32_t timeout)
{
	uint8_t data = 0;

	if(prvBQ25180_ReadReg(BQ25180_REG_CHARGERCTRL0, &data, timeout) != BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;

	*mask =  data;

    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_SetChargerIntMask(uint8_t mask, uint32_t timeout)
{
	uint8_t data = 0;

	/* Preserve CHG_DIS bit [7] */
	if(prvBQ25180_ReadReg(BQ25180_REG_CHARGERCTRL1, &data, timeout) != BQ25180_STATUS_OK)
	{
		return BQ25180_STATUS_ERROR;
	}

	data &= ~(0x07);
	data |= (mask & 0x07);

	if(prvBQ25180_WriteReg(BQ25180_REG_CHARGERCTRL1, data, 1, timeout) != BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;

    return BQ25180_STATUS_OK;
}
bq25180_status_t BQ25180_Charge_Enable(uint32_t timeout)
{


    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_Charge_Current_Set(uint32_t current, uint32_t timeout)
{
	uint8_t regData = 0;
	uint8_t ichgCode = 0;

	if(current <= 35)
	{
		ichgCode = (uint8_t)(current - 5);
	}
	else
	{
		ichgCode = (uint8_t)(((current - 40) / 10) + 31);
	}

	/* Preserve CHG_DIS bit [7] */
	if(prvBQ25180_ReadReg(BQ25180_REG_ICHG_CTRL, &regData, timeout) != BQ25180_STATUS_OK)
	{
		return BQ25180_STATUS_ERROR;
	}

	regData &= ~(0x7F);
	regData |= (ichgCode & 0x7F);

	if(prvBQ25180_WriteReg(BQ25180_REG_ICHG_CTRL, regData, 1, timeout) != BQ25180_STATUS_OK)
	{
		return BQ25180_STATUS_ERROR;
	}

	return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_Charge_Current_Get(uint32_t* current, uint32_t timeout)
{
	uint8_t regData = 0;
	if(prvBQ25180_ReadReg(BQ25180_REG_ICHG_CTRL, &regData, 1000) != BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;

	//*current = (uint32_t)((float)regData*1.25);
	*current = (uint32_t)(regData & 0x7F); /* 1 mA/LSB */
    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_Charge_PreCurrent_Set(uint32_t current, uint32_t timeout)
{
    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_Charge_PreCurrent_Get(uint32_t* current, uint32_t timeout)
{
    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_Charge_TermCurrent_Set(bq25180_tcurrent_value_t value, uint32_t timeout)
{
    uint8_t regData = 0;

    /* Validate ILIM value */
    if(value > BQ25180_TCURRENT_VALUE_20) return BQ25180_STATUS_ERROR;

    /* Read current register value */
    if(prvBQ25180_ReadReg(BQ25180_REG_CHARGERCTRL0, &regData, timeout) != BQ25180_STATUS_OK)
    {
        return BQ25180_STATUS_ERROR;
    }

    /* Update only ILIM[2:0] bits */
    regData &= ~(0x30);
    regData |= ((uint8_t)(value  << 4)  & 0x30);

    /* Write updated register value */
    if(prvBQ25180_WriteReg(BQ25180_REG_CHARGERCTRL0, regData, 1, timeout) != BQ25180_STATUS_OK)
    {
        return BQ25180_STATUS_ERROR;
    }

    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_Charge_TermCurrent_Get(uint32_t* current, uint32_t timeout)
{
	uint8_t regData = 0;

	if(prvBQ25180_ReadReg(BQ25180_REG_CHARGERCTRL1, &regData, 1000) != BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;

	*current = (uint32_t)((regData >> 3) & 0x0F);

    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_Charge_RegVoltage_Set(float voltage, uint32_t timeout)
{
	if (voltage > 4.65) return BQ25180_STATUS_OK;
	float volDiff = voltage - 3.5;
	float volReg = volDiff/0.01;
	int regData = volReg;

	if(prvBQ25180_WriteReg(BQ25180_REG_VBAT_CTRL, regData, 1, 1000)!= BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;

    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_Charge_RegVoltage_Get(float* voltage, uint32_t timeout)
{
	uint8_t regData = 0;

	if(prvBQ25180_ReadReg(BQ25180_REG_VBAT_CTRL, &regData, 1000) != BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;

	*voltage = (float)(((float)(regData)*0.01) + 3.6);

    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_WDG_SetStatus(bq25180_wdg_status status, uint32_t timeout)
{
	uint8_t regData = 0;
	uint8_t regData1 = 0;

	if(prvBQ25180_ReadReg(BQ25180_REG_SYS_REG, &regData, 1000) != BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;
	if(prvBQ25180_ReadReg(BQ25180_REG_IC_CTRL, &regData1, timeout) != BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;

	if(status == BQ25180_WDG_STATUS_ENABLE)
	{
		regData |= 1 << 2;
	}
	else
	{
		regData &= ~(1 << 2);
	    regData1 &= ~(0x03);
	    regData1 |= (0x03);
	}

	if(prvBQ25180_WriteReg(BQ25180_REG_SYS_REG, regData, 1, 1000)!= BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;
	if(prvBQ25180_WriteReg(BQ25180_REG_IC_CTRL, regData1, 1, 1000)!= BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;
    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_ILim_Set(bq25180_ilim_value_t value, uint32_t timeout)
{
    uint8_t regData = 0;

    /* Validate ILIM value */
    if(value > BQ25180_ILIM_VALUE_1050) return BQ25180_STATUS_ERROR;

    /* Read current register value */
    if(prvBQ25180_ReadReg(BQ25180_REG_TMR_ILIM, &regData, timeout) != BQ25180_STATUS_OK)
    {
        return BQ25180_STATUS_ERROR;
    }

    /* Update only ILIM[2:0] bits */
    regData &= ~(0x07);
    regData |= ((uint8_t)value & 0x07);

    /* Write updated register value */
    if(prvBQ25180_WriteReg(BQ25180_REG_TMR_ILIM, regData, 1, timeout) != BQ25180_STATUS_OK)
    {
        return BQ25180_STATUS_ERROR;
    }

    return BQ25180_STATUS_OK;
}


bq25180_status_t BQ25180_GetChargerIntFlags(uint8_t* intFlags, uint32_t timeout)
{
    uint8_t regData = 0;

    /* Read current register value */
    if(prvBQ25180_ReadReg(BQ25180_REG_STAT0, &regData, timeout) != BQ25180_STATUS_OK)
    {
        return BQ25180_STATUS_ERROR;
    }

    *intFlags = regData;

    return BQ25180_STATUS_OK;

}


bq25180_status_t BQ25180_SetChargerTimerState(bq25180_stimer_value_t value, uint32_t timeout)
{
    uint8_t regData = 0;

    /* Validate ILIM value */
    if(value > BQ25180_STIMER_VALUE_DISABLED) return BQ25180_STATUS_ERROR;

    /* Read current register value */
    if(prvBQ25180_ReadReg(BQ25180_REG_IC_CTRL, &regData, timeout) != BQ25180_STATUS_OK)
    {
        return BQ25180_STATUS_ERROR;
    }

    /* Update only ILIM[2:0] bits */
    regData &= ~(0x0C);
    regData |= ((uint8_t)(value << 2) & 0x0C);

    /* Write updated register value */
    if(prvBQ25180_WriteReg(BQ25180_REG_IC_CTRL, regData, 1, timeout) != BQ25180_STATUS_OK)
    {
        return BQ25180_STATUS_ERROR;
    }

    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_Charge_ChargeStatus_Set(bq25180_charge_status status, uint32_t timeout)
{
	/**/
	uint8_t regData = 0;
	if(prvBQ25180_ReadReg(BQ25180_REG_ICHG_CTRL, &regData, 1000) != BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;
	/**/
	if(status == BQ25180_CHARGE_STATUS_ENABLE)
	{
		regData &= ~0x80;

		if(prvBQ25180_WriteReg(BQ25180_REG_ICHG_CTRL, regData, 1, 1000)!= BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;

	}
	else
	{
		regData |= 0x80;

		if(prvBQ25180_WriteReg(BQ25180_REG_ICHG_CTRL, regData, 1, 1000)!= BQ25180_STATUS_OK) return BQ25180_STATUS_ERROR;
	}

    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_RegCallback(bq25180_intcb cb)
{
	prvBQ25180_CB = cb;

    return BQ25180_STATUS_OK;
}

bq25180_status_t BQ25180_ReadReg(uint8_t regAddr, uint8_t* data, uint32_t timeout)
{
    return prvBQ25180_ReadReg(regAddr, data, timeout);
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
