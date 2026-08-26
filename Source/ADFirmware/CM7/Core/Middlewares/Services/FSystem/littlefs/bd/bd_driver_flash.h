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
#ifndef LFS_FLASHBD_DRIVER_H_
#define LFS_FLASHBD_DRIVER_H_
/*----------------------------------- Standard Includes -----------------------------------*/
#include <stdint.h>
/*----------------------------------- LittleFS Includes -----------------------------------*/
#include "../lfs.h"
#include "globalConfig.h"

/*------------------------------ Block Device Configuration -------------------------------*/
/*Minimum data size which can be program to the flash. See MCU flash datasheet*/
#define BD_DRIVER_FLASH_OFFSET				CONF_FSYSTEM_OFFSET //8 bytes



/*! @struct LFS_BD_Flash_Config
    @brief 	This structure is used to pass configuration to embedded flash block device
*/
typedef struct{
	/*Depends on type of flash memory, this value can be -1 (FF) or 0 (00)
	 * and indicate which value is stored to flash after erase*/
    int32_t 	EraseValue;
	/*Number of first block in flash memory which will be used with file system*/
    uint32_t 	StartBlock;
    /*Number of the flash memory bank which will be used by file system*/
    uint32_t 	StartBank;
    /*Start address of the embedded flash memory part which will be used with file system*/
    void *Buffer;
}LFS_BD_Flash_Config;

/*! @struct LFS_BD_Flash
    @brief 	Structure which is used to store all data important for block device
*/
typedef struct{
    /*Start address of the embedded flash memory part which will be used with file system*/
    uint8_t *buffer;
    /*Configuration structure assigned to block device*/
    const LFS_BD_Flash_Config *cfg;
} LFS_BD_Flash;
/**
 * @brief 	Initialize block device with user define configuration
 *
 * @param 	LFSConfig			- 	Pointer to LittleFS configuration structure.
 * @param 	FlashBDCfg			- 	Pointer to user initialized configuration structure which will be assigned to block device.
 */
int bd_driver_flash_init(const struct lfs_config *LFSConfig, const LFS_BD_Flash_Config *FlashBDCfg);
/**
 * @brief 	Read data from block inside a block device
 *
 * @param 	LFSConfig			- 	Pointer to LittleFS configuration structure.
 * @param 	BlockNo				- 	Number of block from which data will be read
 * @param 	Offset				- 	Offset in block
 * @param 	ReadSize			- 	Size of data which will be read from block device
 */
int bd_driver_flash_read(const struct lfs_config *LFSConfig, lfs_block_t BlockNo, lfs_off_t Offset, void *Buffer, lfs_size_t ReadSize);
/**
 * @brief 	Write data to block inside a block device
 *
 * @param 	LFSConfig			- 	Pointer to LittleFS configuration structure.
 * @param 	BlockNo				- 	Number of block where data will be written
 * @param 	Offset				- 	Offset in block
 * @param 	DataSize			- 	Size of data which will be written to block device
 */
int bd_driver_flash_prog(const struct lfs_config *LFSConfig, lfs_block_t BlockNo, lfs_off_t Offset,const void *Data, lfs_size_t DataSize);
/**
 * @brief 	Erase block
 *
 * 			Block must be erased before programming
 *
 * @param 	LFSConfig			- 	Pointer to LittleFS configuration structure.
 * @param 	BlockNo				- 	Number of block which will be erased
 */
int bd_driver_flash_erase(const struct lfs_config *LFSConfig, lfs_block_t BlockNo);
/**
 * @brief 	Write back data from block_device cache to memory
 *
 * 			This function always return 0 because this block device does not have internal cache
 */
int bd_driver_flash_sync(const struct lfs_config *LFSConfig);

#endif /* LITTLEFS_BD_DRIVER_LFS_FLASHBD_DRIVER_H_ */
