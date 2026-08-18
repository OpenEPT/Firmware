/**
 ******************************************************************************
 * @file    dac6578.c
 *
 * @brief   DAC6578 DAC driver implementation providing hardware abstraction
 *          layer for controlling the TI DAC6578 10-bit, 8-channel I2C DAC.
 *          This driver uses I2C instance 2 and supports setting output code
 *          values for selected DAC channels.
 *
 * @author  elektronika
 * @date    April 2026
 ******************************************************************************
 */

#include "dac6578.h"

#include "main.h"
#include "drv_i2c.h"

/**
 * @defgroup HAL Hardware Abstraction Layer
 * @{
 */

/**
 * @defgroup DAC6578_DRIVER DAC6578 DAC Driver
 * @{
 */

/**
 * @defgroup DAC6578_PRIVATE_DEFINES DAC6578 driver private defines
 * @{
 */
#define DAC6578_CHANNELS_NUMBER                                  8U
#define DAC6578_COMMAND_WRITE_INPUT_REG      					 0x00U
#define DAC6578_COMMAND_SELECT_REG      					 	 0x10U
#define DAC6578_COMMAND_WRITE_INPUT_AND_UPDATE_ALL_DAC_REGS      0x20U
#define DAC6578_COMMAND_WRITE_INPUT_AND_UPDATE_DAC_REG           0x30U
#define DAC6578_COMMAND_POWER_ON_OFF_DAC          				 0x40U
#define DAC6578_COMMAND_WRITE_TO_CLEAR_DAC        				 0x50U
#define DAC6578_COMMAND_LDAC_REGISTER        				 	 0x60U
#define DAC6578_COMMAND_SOFTWARE        				 	 	 0x70U

#define DAC6578_COMMAND_READ_INPUT_REG      					 0x00U
#define DAC6578_COMMAND_READ_REG      					 		 0x10U
#define DAC6578_COMMAND_READ_POWER_DOWN_REG      				 0x40U
#define DAC6578_COMMAND_READ_CLEAR_DAC      					 0x50U
#define DAC6578_COMMAND_READ_LDAC_REGS          				 0x60U
/**
 * @}
 */

/**
 * @defgroup DAC6578_PRIVATE_FUNCTIONS DAC6578 driver private functions
 * @{
 */

/**
 * @brief   Validate DAC6578 channel
 * @param   channel: DAC channel
 * @retval  1 if channel is valid, 0 otherwise
 */
static uint8_t prvDAC6578_IsChannelValid(dac6578_channel_t channel)
{
    return ((uint32_t)channel < DAC6578_CHANNELS_NUMBER) ? 1U : 0U;
}

/**
 * @brief   Write raw DAC6578 command frame
 * @param   command: Command and access byte
 * @param   value: 10-bit DAC value
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::dac6578_status_t
 */
static dac6578_status_t prvDAC6578_WriteCommand(uint8_t command, uint16_t value, uint32_t timeout)
{
    uint8_t devAddr;
    uint8_t txBuffer[3];

    if(value > DAC6578_MAX_VALUE)
    {
        return DAC6578_STATUS_ERROR;
    }

    txBuffer[0] = command;
    txBuffer[1] = (uint8_t)((value >> 2U) & 0xFFU);
    txBuffer[2] = (uint8_t)((value & 0x03U) << 6U);

    devAddr = (uint8_t)(DAC6578_DEV_ADDR << 1U);

    if(DRV_I2C_Transmit(DRV_I2C_INSTANCE_2, devAddr, txBuffer, sizeof(txBuffer), timeout) != DRV_I2C_STATUS_OK)
    {
        return DAC6578_STATUS_ERROR;
    }

    return DAC6578_STATUS_OK;
}

/**
 * @}
 */

dac6578_status_t DAC6578_Init(void)
{
    drv_i2c_config_t config;

    config.clkFreq = 100;

    if(DRV_I2C_Instance_Init(DRV_I2C_INSTANCE_2, &config) != DRV_I2C_STATUS_OK)
    {
        return DAC6578_STATUS_ERROR;
    }

    return DAC6578_STATUS_OK;
}

dac6578_status_t DAC6578_SetAndUpdateChannelValue(dac6578_channel_t channel, uint16_t value, uint32_t timeout)
{
    uint8_t command;

    if((prvDAC6578_IsChannelValid(channel) == 0U) || (value > DAC6578_MAX_VALUE))
    {
        return DAC6578_STATUS_ERROR;
    }

    command = (uint8_t)(DAC6578_COMMAND_WRITE_INPUT_AND_UPDATE_DAC_REG | (uint8_t)channel);

    return prvDAC6578_WriteCommand(command, value, timeout);
}

dac6578_status_t DAC6578_SetChannelValue(dac6578_channel_t channel,
                                         uint16_t value,
                                         uint32_t timeout)
{
    uint8_t command;

    if((prvDAC6578_IsChannelValid(channel) == 0U) ||
       (value > DAC6578_MAX_VALUE))
    {
        return DAC6578_STATUS_ERROR;
    }

    command = (uint8_t)(DAC6578_COMMAND_WRITE_INPUT_REG |
                        (uint8_t)channel);

    return prvDAC6578_WriteCommand(command, value, timeout);
}

dac6578_status_t DAC6578_SetChannelState(dac6578_channel_t channel,
                                         dac6578_channel_state_t state,
                                         uint32_t timeout)
{
    uint8_t devAddr;
    uint8_t txBuffer[3];
    uint8_t channelMask;

    if((prvDAC6578_IsChannelValid(channel) == 0U) ||
       ((state != DAC6578_CHANNEL_DISABLED) &&
        (state != DAC6578_CHANNEL_ENABLED)))
    {
        return DAC6578_STATUS_ERROR;
    }

    channelMask = (uint8_t)(1U << (uint8_t)channel);

    txBuffer[0] = DAC6578_COMMAND_POWER_ON_OFF_DAC;

    if(state == DAC6578_CHANNEL_ENABLED)
    {
        txBuffer[1] = 0x00U;
    }
    else
    {
        txBuffer[1] = 0x60U;
    }

    txBuffer[1] |= (uint8_t)(channelMask >> 3U);
    txBuffer[2]  = (uint8_t)(channelMask << 5U);

    devAddr = (uint8_t)(DAC6578_DEV_ADDR << 1U);

    if(DRV_I2C_Transmit(DRV_I2C_INSTANCE_2,
                        devAddr,
                        txBuffer,
                        sizeof(txBuffer),
                        timeout) != DRV_I2C_STATUS_OK)
    {
        return DAC6578_STATUS_ERROR;
    }

    return DAC6578_STATUS_OK;
}

dac6578_status_t DAC6578_Reset(uint32_t timeout)
{
    uint8_t devAddr;
    uint8_t txBuffer[3];

    txBuffer[0] = DAC6578_COMMAND_SOFTWARE;
    txBuffer[1] = 0x00U;
    txBuffer[2] = 0x00U;

    devAddr = (uint8_t)(DAC6578_DEV_ADDR << 1U);

    if(DRV_I2C_Transmit(DRV_I2C_INSTANCE_2, devAddr, txBuffer, 1, timeout) != DRV_I2C_STATUS_OK)
    {
        return DAC6578_STATUS_ERROR;
    }

    return DAC6578_STATUS_OK;
}

/**
 * @}
 */

/**
 * @}
 */
