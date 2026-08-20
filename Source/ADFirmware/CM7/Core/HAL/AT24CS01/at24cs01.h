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
#include "globalConfig.h"

/**
 * @defgroup HAL Hardware Abstraction Layer
 * @{
 */

/**
 * @defgroup AT24CS01_DRIVER AT24CS01 EEPROM Driver
 * @{
 */

/**
 * @defgroup AT24CS01_PUBLIC_DEFINES AT24CS01 driver public defines
 * @{
 */

#define AT24CS01_MEMORY_SIZE_BYTES       128U    /*!< Total EEPROM size in bytes */
#define AT24CS01_PAGE_SIZE_BYTES         8U      /*!< EEPROM page size in bytes */
#define AT24CS01_WRITE_CYCLE_TIME_MS     5U      /*!< Maximum EEPROM internal write cycle time */
#define AT24CS01_DEV_ADDR                0x50U   /*!< 7-bit I2C address for EEPROM array, A2:A0 = 000 */
#define AT24CS01_SERIAL_DEV_ADDR         0x58U   /*!< 7-bit I2C address for serial number area, A2:A0 = 000 */
#define AT24CS01_SERIAL_START_ADDR       0x80U   /*!< Serial number block start word address */
#define AT24CS01_SERIAL_SIZE_BYTES       16U     /*!< Factory-programmed serial number size in bytes */

/**
 * @}
 */

/**
 * @defgroup AT24CS01_PUBLIC_TYPES AT24CS01 driver public data types
 * @{
 */

/**
 * @brief AT24CS01 driver return status
 */
typedef enum
{
    AT24CS01_STATUS_OK = 0,      /*!< AT24CS01 operation successful */
    AT24CS01_STATUS_ERROR        /*!< AT24CS01 operation failed */
} at24cs01_status_t;

/**
 * @}
 */

/**
 * @defgroup AT24CS01_PUBLIC_FUNCTIONS AT24CS01 driver interface functions
 * @{
 */

/**
 * @brief   Initialize the AT24CS01 EEPROM driver
 * @retval  ::at24cs01_status_t
 */
at24cs01_status_t AT24CS01_Init(void);

/**
 * @brief   Ping the AT24CS01 device to verify communication
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::at24cs01_status_t
 */
at24cs01_status_t AT24CS01_Ping(uint32_t timeout);

/**
 * @brief   Write a single byte to EEPROM
 * @param   memAddr: EEPROM memory address
 * @param   data: Data byte to write
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::at24cs01_status_t
 */
at24cs01_status_t AT24CS01_WriteByte(uint8_t memAddr, uint8_t data, uint32_t timeout);

/**
 * @brief   Read a single byte from EEPROM
 * @param   memAddr: EEPROM memory address
 * @param   data: Pointer to variable where read byte will be stored
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::at24cs01_status_t
 */
at24cs01_status_t AT24CS01_ReadByte(uint8_t memAddr, uint8_t* data, uint32_t timeout);

/**
 * @brief   Write multiple bytes to EEPROM
 * @param   memAddr: Start EEPROM memory address
 * @param   data: Pointer to transmit buffer
 * @param   size: Number of bytes to write
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::at24cs01_status_t
 */
at24cs01_status_t AT24CS01_Write(uint8_t memAddr, const uint8_t* data, uint16_t size, uint32_t timeout);

/**
 * @brief   Read multiple bytes from EEPROM
 * @param   memAddr: Start EEPROM memory address
 * @param   data: Pointer to receive buffer
 * @param   size: Number of bytes to read
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::at24cs01_status_t
 */
at24cs01_status_t AT24CS01_Read(uint8_t memAddr, uint8_t* data, uint16_t size, uint32_t timeout);

/**
 * @brief   Read the factory-programmed 128-bit unique serial number
 * @param   data: Pointer to a buffer of at least AT24CS01_SERIAL_SIZE_BYTES bytes
 * @param   timeout: Communication timeout in milliseconds
 * @retval  ::at24cs01_status_t
 */
at24cs01_status_t AT24CS01_ReadSerialNumber(uint8_t* data, uint32_t timeout);

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
