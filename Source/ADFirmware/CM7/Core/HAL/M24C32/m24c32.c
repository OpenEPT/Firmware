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

#include "m24c32.h"

#include "main.h"
#include "drv_i2c.h"

#include <string.h>

/**
 * @defgroup HAL Hardware Abstraction Layer
 * @{
 */

/**
 * @defgroup M24C32_DRIVER M24C32 EEPROM Driver
 * @{
 */

/**
 * @defgroup M24C32_PRIVATE_FUNCTIONS M24C32 driver private functions
 * @{
 */

/**
 * @brief   Validate requested EEPROM address range
 * @param   memAddr: Start memory address
 * @param   size: Number of bytes
 * @retval  1 if range is valid, 0 otherwise
 */
static uint8_t prvM24C32_IsRangeValid(uint16_t memAddr, uint16_t size)
{
    uint32_t endAddr;

    if(size == 0U)
    {
        return 0U;
    }

    endAddr = (uint32_t)memAddr + (uint32_t)size;
    if(endAddr > M24C32_MEMORY_SIZE_BYTES)
    {
        return 0U;
    }

    return 1U;
}

/**
 * @brief   Read EEPROM data starting from a given memory address
 * @param   memAddr: EEPROM internal memory address
 * @param   data: Pointer to receive buffer
 * @param   size: Number of bytes to read
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::m24c32_status_t
 */
static m24c32_status_t prvM24C32_Read(uint16_t memAddr, uint8_t* data, uint16_t size, uint32_t timeout)
{
    uint8_t devAddr;
    uint8_t addrBytes[2];

    if((data == NULL) || (prvM24C32_IsRangeValid(memAddr, size) == 0U))
    {
        return M24C32_STATUS_ERROR;
    }

    addrBytes[0] = (uint8_t)((memAddr >> 8) & 0xFFU);
    addrBytes[1] = (uint8_t)(memAddr & 0xFFU);

    devAddr = (uint8_t)(M24C32_DEV_ADDR << 1);

    if(DRV_I2C_Transmit(DRV_I2C_INSTANCE_2, devAddr, addrBytes, 2U, timeout) != DRV_I2C_STATUS_OK)
    {
        return M24C32_STATUS_ERROR;
    }

    if(DRV_I2C_Receive(DRV_I2C_INSTANCE_2, (uint8_t)(devAddr | 0x01U), data, size, timeout) != DRV_I2C_STATUS_OK)
    {
        return M24C32_STATUS_ERROR;
    }

    return M24C32_STATUS_OK;
}

/**
 * @brief   Write one EEPROM page chunk
 * @param   memAddr: EEPROM internal memory address
 * @param   data: Pointer to transmit buffer
 * @param   size: Number of bytes to write in this chunk
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::m24c32_status_t
 */
static m24c32_status_t prvM24C32_WritePage(uint16_t memAddr, const uint8_t* data, uint16_t size, uint32_t timeout)
{
    uint8_t devAddr;
    uint8_t txBuffer[2U + M24C32_PAGE_SIZE_BYTES];

    if((data == NULL) || (size == 0U) || (size > M24C32_PAGE_SIZE_BYTES))
    {
        return M24C32_STATUS_ERROR;
    }

    txBuffer[0] = (uint8_t)((memAddr >> 8) & 0xFFU);
    txBuffer[1] = (uint8_t)(memAddr & 0xFFU);
    memcpy(&txBuffer[2], data, size);

    devAddr = (uint8_t)(M24C32_DEV_ADDR << 1);

    if(DRV_I2C_Transmit(DRV_I2C_INSTANCE_2, devAddr, txBuffer, (uint32_t)(size + 2U), timeout) != DRV_I2C_STATUS_OK)
    {
        return M24C32_STATUS_ERROR;
    }

    /*
     * EEPROM requires internal write cycle time after page write.
     * Fixed delay is used here because current DRV_I2C API does not expose
     * a dedicated ACK-poll helper.
     */
    HAL_Delay(M24C32_WRITE_CYCLE_TIME_MS);

    return M24C32_STATUS_OK;
}

/**
 * @}
 */

m24c32_status_t M24C32_Init(void)
{
    /*
     * Same usage pattern as BQ25150 HAL:
     * initialize the underlying I2C instance used by the device.
     */
	drv_i2c_config_t config;
	config.clkFreq = 100;
    if(DRV_I2C_Instance_Init(DRV_I2C_INSTANCE_2, &config) != DRV_I2C_STATUS_OK)
    {
        return M24C32_STATUS_ERROR;
    }

    return M24C32_STATUS_OK;
}

m24c32_status_t M24C32_Ping(uint32_t timeout)
{
    uint8_t dummy = 0U;

    /*
     * A simple 1-byte read from address 0x0000 is used as communication check.
     */
    if(prvM24C32_Read(0U, &dummy, 1U, timeout) != M24C32_STATUS_OK)
    {
        return M24C32_STATUS_ERROR;
    }

    return M24C32_STATUS_OK;
}

m24c32_status_t M24C32_WriteByte(uint16_t memAddr, uint8_t data, uint32_t timeout)
{
    return prvM24C32_WritePage(memAddr, &data, 1U, timeout);
}

m24c32_status_t M24C32_ReadByte(uint16_t memAddr, uint8_t* data, uint32_t timeout)
{
    return prvM24C32_Read(memAddr, data, 1U, timeout);
}

m24c32_status_t M24C32_Write(uint16_t memAddr, const uint8_t* data, uint16_t size, uint32_t timeout)
{
    uint16_t currentAddr;
    uint16_t remaining;
    uint16_t chunkSize;
    uint16_t pageOffset;
    uint16_t spaceInPage;

    if((data == NULL) || (prvM24C32_IsRangeValid(memAddr, size) == 0U))
    {
        return M24C32_STATUS_ERROR;
    }

    currentAddr = memAddr;
    remaining = size;

    while(remaining > 0U)
    {
        pageOffset = (uint16_t)(currentAddr % M24C32_PAGE_SIZE_BYTES);
        spaceInPage = (uint16_t)(M24C32_PAGE_SIZE_BYTES - pageOffset);
        chunkSize = (remaining < spaceInPage) ? remaining : spaceInPage;

        if(prvM24C32_WritePage(currentAddr, data, chunkSize, timeout) != M24C32_STATUS_OK)
        {
            return M24C32_STATUS_ERROR;
        }

        currentAddr = (uint16_t)(currentAddr + chunkSize);
        data += chunkSize;
        remaining = (uint16_t)(remaining - chunkSize);
    }

    return M24C32_STATUS_OK;
}

m24c32_status_t M24C32_Read(uint16_t memAddr, uint8_t* data, uint16_t size, uint32_t timeout)
{
    return prvM24C32_Read(memAddr, data, size, timeout);
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
