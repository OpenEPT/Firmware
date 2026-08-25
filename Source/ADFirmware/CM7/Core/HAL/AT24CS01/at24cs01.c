/**
 ******************************************************************************
 * @file    m24c32.c
 *
 * @brief   M24C32 EEPROM driver implementation providing hardware abstraction
 *          layer for controlling and accessing the ST M24C32 I2C EEPROM
 *          memory. This driver supports device presence check, single-byte and
 *          multi-byte read/write operations, page-aligned writes, and address
 *          range validation for reliable non-volatile data storage.
 *
 * @author  elektronika
 * @date    April 2026
 ******************************************************************************
 */

#include "../AT24CS01/at24cs01.h"

#include "main.h"
#include "drv_i2c.h"

#include <string.h>

/**
 * @defgroup HAL Hardware Abstraction Layer
 * @{
 */

/**
 * @defgroup AT24CS01_DRIVER AT24CS01 EEPROM Driver
 * @{
 */

/**
 * @defgroup AT24CS01_PRIVATE_FUNCTIONS AT24CS01 driver private functions
 * @{
 */

/**
 * @brief   Validate requested EEPROM address range
 * @param   memAddr: Start memory address
 * @param   size: Number of bytes
 * @retval  1 if range is valid, 0 otherwise
 */
static uint8_t prvAT24CS01_IsRangeValid(uint8_t memAddr, uint16_t size)
{
    uint16_t endAddr;

    if(size == 0U)
    {
        return 0U;
    }

    endAddr = (uint16_t)memAddr + size;

    if(endAddr > AT24CS01_MEMORY_SIZE_BYTES)
    {
        return 0U;
    }

    return 1U;
}

/**
 * @brief   Read data from an AT24CS01 addressable region
 * @param   devAddr: 7-bit I2C device address of the requested region
 * @param   wordAddr: Internal word address within the selected region
 * @param   data: Pointer to receive buffer
 * @param   size: Number of bytes to read
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::at24cs01_status_t
 */
static at24cs01_status_t prvAT24CS01_ReadRegion(uint8_t devAddr,
                                                uint8_t wordAddr,
                                                uint8_t* data,
                                                uint16_t size,
                                                uint32_t timeout)
{
    uint8_t addr;

    if((data == NULL) || (size == 0U))
    {
        return AT24CS01_STATUS_ERROR;
    }

    addr = wordAddr;

    if(DRV_I2C_Transmit(DRV_I2C_INSTANCE_1,
                        (uint8_t)(devAddr << 1),
                        &addr,
                        1U,
                        timeout) != DRV_I2C_STATUS_OK)
    {
        return AT24CS01_STATUS_ERROR;
    }

    if(DRV_I2C_Receive(DRV_I2C_INSTANCE_1,
                       (uint8_t)(devAddr << 1),
                       data,
                       size,
                       timeout) != DRV_I2C_STATUS_OK)
    {
        return AT24CS01_STATUS_ERROR;
    }

    return AT24CS01_STATUS_OK;
}

/**
 * @brief   Read EEPROM data starting from a given memory address
 * @param   memAddr: EEPROM internal memory address
 * @param   data: Pointer to receive buffer
 * @param   size: Number of bytes to read
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::at24cs01_status_t
 */
static at24cs01_status_t prvAT24CS01_Read(uint8_t memAddr,
                                          uint8_t* data,
                                          uint16_t size,
                                          uint32_t timeout)
{
    if((data == NULL) || (prvAT24CS01_IsRangeValid(memAddr, size) == 0U))
    {
        return AT24CS01_STATUS_ERROR;
    }

    return prvAT24CS01_ReadRegion(AT24CS01_DEV_ADDR, memAddr, data, size, timeout);
}

static at24cs01_status_t prvAT24CS01_WritePage(uint8_t memAddr,
                                               const uint8_t* data,
                                               uint16_t size,
                                               uint32_t timeout)
{
    uint8_t txBuffer[1U + AT24CS01_PAGE_SIZE_BYTES];

    if((data == NULL) || (size == 0U) || (size > AT24CS01_PAGE_SIZE_BYTES))
    {
        return AT24CS01_STATUS_ERROR;
    }

    txBuffer[0] = memAddr;
    memcpy(&txBuffer[1], data, size);

    if(DRV_I2C_Transmit(DRV_I2C_INSTANCE_1,
                        (uint8_t)(AT24CS01_DEV_ADDR << 1),
                        txBuffer,
                        (uint32_t)(size + 1),
                        timeout) != DRV_I2C_STATUS_OK)
    {
        return AT24CS01_STATUS_ERROR;
    }

    HAL_Delay(AT24CS01_WRITE_CYCLE_TIME_MS);

    return AT24CS01_STATUS_OK;
}

/**
 * @}
 */

at24cs01_status_t AT24CS01_Init(void)
{
    drv_i2c_config_t config;

    config.clkFreq = 100U;

    if(DRV_I2C_Instance_Init(DRV_I2C_INSTANCE_1, &config) != DRV_I2C_STATUS_OK)
    {
        return AT24CS01_STATUS_ERROR;
    }

    return AT24CS01_STATUS_OK;
}


at24cs01_status_t AT24CS01_Ping(uint32_t timeout)
{
    uint8_t serialNumber[AT24CS01_SERIAL_SIZE_BYTES];

    memset(serialNumber, 0, AT24CS01_SERIAL_SIZE_BYTES);

    if(AT24CS01_ReadSerialNumber(serialNumber, timeout) != AT24CS01_STATUS_OK)
    {
        return AT24CS01_STATUS_ERROR;
    }

    return AT24CS01_STATUS_OK;
}


at24cs01_status_t AT24CS01_WriteByte(uint8_t memAddr, uint8_t data, uint32_t timeout)
{
    if(prvAT24CS01_IsRangeValid(memAddr, 1U) == 0U)
    {
        return AT24CS01_STATUS_ERROR;
    }

    return prvAT24CS01_WritePage(memAddr, &data, 1U, timeout);
}

at24cs01_status_t AT24CS01_ReadByte(uint8_t memAddr, uint8_t* data, uint32_t timeout)
{
    return prvAT24CS01_Read(memAddr, data, 1U, timeout);
}

at24cs01_status_t AT24CS01_Write(uint8_t memAddr,
                                 const uint8_t* data,
                                 uint16_t size,
                                 uint32_t timeout)
{
    uint16_t currentAddr;
    uint16_t remaining;
    uint16_t pageOffset;
    uint16_t spaceInPage;
    uint16_t chunkSize;

    if((data == NULL) || (prvAT24CS01_IsRangeValid(memAddr, size) == 0U))
    {
        return AT24CS01_STATUS_ERROR;
    }

    currentAddr = memAddr;
    remaining = size;

    while(remaining > 0U)
    {
        pageOffset = (uint16_t)(currentAddr % AT24CS01_PAGE_SIZE_BYTES);
        spaceInPage = (uint16_t)(AT24CS01_PAGE_SIZE_BYTES - pageOffset);
        chunkSize = (remaining < spaceInPage) ? remaining : spaceInPage;

        if(prvAT24CS01_WritePage((uint8_t)currentAddr,
                                 data,
                                 chunkSize,
                                 timeout) != AT24CS01_STATUS_OK)
        {
            return AT24CS01_STATUS_ERROR;
        }

        currentAddr = (uint16_t)(currentAddr + chunkSize);
        data += chunkSize;
        remaining = (uint16_t)(remaining - chunkSize);
    }

    return AT24CS01_STATUS_OK;
}

at24cs01_status_t AT24CS01_Read(uint8_t memAddr,
                                uint8_t* data,
                                uint16_t size,
                                uint32_t timeout)
{
    return prvAT24CS01_Read(memAddr, data, size, timeout);
}

at24cs01_status_t AT24CS01_ReadSerialNumber(uint8_t* data, uint32_t timeout)
{
    if(data == NULL)
    {
        return AT24CS01_STATUS_ERROR;
    }

    return prvAT24CS01_ReadRegion(AT24CS01_SERIAL_DEV_ADDR,
                                  AT24CS01_SERIAL_START_ADDR,
                                  data,
                                  AT24CS01_SERIAL_SIZE_BYTES,
                                  timeout);
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
