/*********************************************************************************************************//**
 * @file    memory_utils.h
 * @brief   Memory utility functions for erasing flash memory.
 ************************************************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------------------------------------*/
#ifndef __MEMORY_UTILS_H
#define __MEMORY_UTILS_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------------------------------------*/
#include "ht32f5xxxx_flash.h"

/** @addtogroup Utilities
  * @{
  */

/** @addtogroup Common
  * @{
  */

/** @addtogroup MEMORY_UTILS MEMORY_UTILS
  * @{
  */

/* Exported constants --------------------------------------------------------------------------------------*/
/** @defgroup MEMORY_UTILS_Exported_Constants Memory Utils exported constants
  * @{
  */
#define INTERNAL_FLASH_MAX_ADDR        (0x20000000UL)  /*!< Maximum address for internal flash memory        */
/**
  * @}
  */

/* Exported types ------------------------------------------------------------------------------------------*/
/** @defgroup MEMORY_UTILS_Exported_Types Memory Utils exported types
  * @{
  */
/**
 * @brief  Enumeration of memory erase return status.
 */
typedef enum
{
  MEMORY_ERASE_SUCCESS              = 0,
  MEMORY_ERASE_INVALID_ADDRESS,
  MEMORY_ERASE_FLASH_ERROR,
  MEMORY_ERASE_TIMEOUT,
  MEMORY_ERASE_WRITE_PROTECTED
} Memory_EraseStatus;
/**
  * @}
  */

/* Exported functions --------------------------------------------------------------------------------------*/
/** @defgroup MEMORY_UTILS_Exported_Functions Memory Utils exported functions
  * @{
  */
/*********************************************************************************************************//**
  * @brief  Erases memory at the specified address range without erasing the entire page.
  * @param  Address: Memory address to erase from (must be within flash memory range < 0x20000000).
  * @param  Size: Number of bytes to erase (must be word-aligned, i.e., multiple of 4).
  *         The function will preserve other data in the same page by reading, modifying,
  *         erasing the page, and writing back the modified content.
  * @retval Memory_EraseStatus
  *   - \ref MEMORY_ERASE_SUCCESS: Erase operation completed successfully
  *   - \ref MEMORY_ERASE_INVALID_ADDRESS: Address is out of valid range
  *   - \ref MEMORY_ERASE_FLASH_ERROR: Flash operation error
  *   - \ref MEMORY_ERASE_TIMEOUT: Operation timeout
  *   - \ref MEMORY_ERASE_WRITE_PROTECTED: Memory is write protected
  * @note   HSI must keep turn on when doing the Flash operation.
  * @note   Address and Size must be word-aligned (multiple of 4 bytes).
  * @note   This function reads the entire page, modifies only the specified range,
  *         erases the page, and writes back the modified content to preserve other data.
  ***********************************************************************************************************/
Memory_EraseStatus Memory_EraseFromAddress(u32 Address, u32 Size);
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __MEMORY_UTILS_H ---------------------------------------------------------------------------------*/


