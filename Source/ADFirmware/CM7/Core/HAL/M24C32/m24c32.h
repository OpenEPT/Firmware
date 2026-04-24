/**
 ******************************************************************************
 * @file    m24c32.h
 *
 * @brief   M24C32 EEPROM driver provides hardware abstraction layer for
 *          controlling and accessing the ST M24C32 I2C EEPROM memory.
 *          This driver supports device presence check, single-byte and
 *          multi-byte read/write operations, page-aligned writes, and
 *          address range validation for reliable non-volatile data storage.
 *          All M24C32 driver interface functions, defines, and types are
 *          declared in this header file.
 *
 * @author  elektronika
 * @date    April 2026
 ******************************************************************************
 */

#ifndef CORE_HAL_M24C32_M24C32_H_
#define CORE_HAL_M24C32_M24C32_H_

#include <stdint.h>

/**
 * @defgroup HAL Hardware Abstraction Layer
 * @{
 */

/**
 * @defgroup M24C32_DRIVER M24C32 EEPROM Driver
 * @{
 */

/**
 * @defgroup M24C32_PUBLIC_DEFINES M24C32 driver public defines
 * @{
 */
#define M24C32_MEMORY_SIZE_BYTES         4096U   /*!< Total EEPROM size in bytes */
#define M24C32_PAGE_SIZE_BYTES           32U     /*!< EEPROM page size in bytes */
#define M24C32_WRITE_CYCLE_TIME_MS       10U     /*!< EEPROM internal write cycle time */
#define M24C32_DEV_ADDR                  0x55U   /*!< 7-bit I2C device address */
/**
 * @}
 */

/**
 * @defgroup M24C32_PUBLIC_TYPES M24C32 driver public data types
 * @{
 */

/**
 * @brief M24C32 driver return status
 */
typedef enum
{
    M24C32_STATUS_OK = 0,       /*!< M24C32 operation successful */
    M24C32_STATUS_ERROR         /*!< M24C32 operation failed */
} m24c32_status_t;

/**
 * @}
 */

/**
 * @defgroup M24C32_PUBLIC_FUNCTIONS M24C32 driver interface functions
 * @{
 */

/**
 * @brief   Initialize the M24C32 EEPROM driver
 * @retval  ::m24c32_status_t
 */
m24c32_status_t M24C32_Init(void);

/**
 * @brief   Ping the M24C32 device to verify communication
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::m24c32_status_t
 */
m24c32_status_t M24C32_Ping(uint32_t timeout);

/**
 * @brief   Write a single byte to EEPROM
 * @param   memAddr: EEPROM memory address
 * @param   data: Data byte to write
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::m24c32_status_t
 */
m24c32_status_t M24C32_WriteByte(uint16_t memAddr, uint8_t data, uint32_t timeout);

/**
 * @brief   Read a single byte from EEPROM
 * @param   memAddr: EEPROM memory address
 * @param   data: Pointer to variable where read byte will be stored
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::m24c32_status_t
 */
m24c32_status_t M24C32_ReadByte(uint16_t memAddr, uint8_t* data, uint32_t timeout);

/**
 * @brief   Write multiple bytes to EEPROM
 * @param   memAddr: Start EEPROM memory address
 * @param   data: Pointer to transmit buffer
 * @param   size: Number of bytes to write
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::m24c32_status_t
 */
m24c32_status_t M24C32_Write(uint16_t memAddr, const uint8_t* data, uint16_t size, uint32_t timeout);

/**
 * @brief   Read multiple bytes from EEPROM
 * @param   memAddr: Start EEPROM memory address
 * @param   data: Pointer to receive buffer
 * @param   size: Number of bytes to read
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::m24c32_status_t
 */
m24c32_status_t M24C32_Read(uint16_t memAddr, uint8_t* data, uint16_t size, uint32_t timeout);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_HAL_M24C32_M24C32_H_ */
