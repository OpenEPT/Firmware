/** =====================================================================================
 *  @file 	bd_driver_flash.h
 * 	@brief 	Block Device (Embedded Flash) driver - Header File
 *	-------------------------------------------------------------------------------------
 *		Block device is storage device that supports reading and writing data in fixed-size
 *	blocks. Blocks size is generally 512 bytes or multiple of this size. It is important to
 *	mention that block device can be programmed with data size less than block size, and also
 *	data, which size is less than block size, can be read from block device. But, in order to
 *	delete  data from block storage device, which size is less than block size, erase process
 *	must be conducted  on the entire block. That means  if file  system wont to delete, for
 *	example file name which use only 12 bytes, it must erase entire block which minimum size
 *	is 512Bytes. So,  it is  important  to consider  this fact when using file system on MCU
 *	Embedded FLash Memory. In case  of LittleFs (and generally any other File system)  it is
 *	important  that  MCU Embedded flash memory  be organized  as block  storage device which
 *	maximum block (page) size is not bigger than 4KB and number of block should  not  be less
 *	than 256.
 *		In this file  are defined all  functions which is used to interface with block device.
 *	Embedded flash memory used with this functions is STML476RG MCU Flash memory. In order to
 *	access  these memory to write to and to read from  it, STM FLASH HAL Driver functions are
 *	used. IF user wont to  use these project with  some other type of memory, for example SPI
 *	flash, he must to implements his own block device function. In that case, functions defined
 *	in this file can be used as example how to properly implement that functions
 *
 *	---------------------------------------------------------------------------------------
 *  @date 	April 2020
 *  @author TregoLTD
 *  @author Haris Turkmanovic
 *  @email	harist@thevtool.com
 *  ========================================================================================
 */
/*----------------------------------- Standard Includes -----------------------------------*/
#include <string.h>
/*------------------------------------ STMCUbe Includes -----------------------------------*/
#include "m24c32.h"
/*------------------------------ Block Device Drive Includes ------------------------------*/
#include "bd_driver_flash.h"




int bd_driver_flash_init(const struct lfs_config *c,
                         const LFS_BD_Flash_Config *cfg)
{
    (void)c;
    (void)cfg;

    if(M24C32_Init() != M24C32_STATUS_OK)
        return -1;

    if(M24C32_Ping(100) != M24C32_STATUS_OK)
        return -1;

    return 0;
}


int bd_driver_flash_read(const struct lfs_config *c,
                         lfs_block_t block,
                         lfs_off_t off,
                         void* buffer,
                         lfs_size_t size)
{
    uint16_t addr = (block * c->block_size) + off;

    if(M24C32_Read(addr, (uint8_t*)buffer, size, 100) != M24C32_STATUS_OK)
        return -1;

    return 0;
}

int bd_driver_flash_prog(const struct lfs_config *c,
                         lfs_block_t block,
                         lfs_off_t off,
                         const void* buffer,
                         lfs_size_t size)
{
    uint16_t addr = (block * c->block_size) + off;

    if(M24C32_Write(addr, (const uint8_t*)buffer, size, 100) != M24C32_STATUS_OK)
        return -1;

    return 0;
}

int bd_driver_flash_erase(const struct lfs_config *c,
                          lfs_block_t block)
{
    uint8_t eraseBuf[M24C32_PAGE_SIZE_BYTES];
    memset(eraseBuf, 0xFF, sizeof(eraseBuf));

    uint16_t addr = block * c->block_size;
    uint16_t remaining = c->block_size;

    while(remaining > 0)
    {
        uint16_t chunk = (remaining > sizeof(eraseBuf)) ?
                          sizeof(eraseBuf) : remaining;

        if(M24C32_Write(addr, eraseBuf, chunk, 100) != M24C32_STATUS_OK)
            return -1;

        addr += chunk;
        remaining -= chunk;
    }

    return 0;
}

int bd_driver_flash_sync(const struct lfs_config *c)
{
    (void)c;
    return 0;
}
