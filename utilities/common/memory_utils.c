/*********************************************************************************************************//**
 * @file    memory_utils.c
 * @brief   Memory utility functions for erasing flash memory.
 ************************************************************************************************************/

/* Includes ------------------------------------------------------------------------------------------------*/
#include "memory_utils.h"

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
Memory_EraseStatus Memory_EraseFromAddress(u32 Address, u32 Size)
{
  FLASH_State flashStatus;
  u32 pageAddress;
  u32 i;
  u32 *pFlashData;
  u32 *pBuffer;
  u32 eraseStartOffset;
  u32 eraseEndOffset;
  
  /* Check if address is in internal flash range */
  if (Address >= INTERNAL_FLASH_MAX_ADDR)
  {
    return MEMORY_ERASE_INVALID_ADDRESS;
  }
  
  /* Internal flash erase - must be word-aligned */
  if ((Address % 4) != 0 || (Size % 4) != 0)
  {
    return MEMORY_ERASE_INVALID_ADDRESS;
  }
  
  /* Calculate page address */
  pageAddress = (Address / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;
  
  /* Check if erase range is within the same page */
  if ((Address + Size) > (pageAddress + FLASH_PAGE_SIZE))
  {
    return MEMORY_ERASE_INVALID_ADDRESS; /* Erase range spans multiple pages */
  }
  
  /* Allocate buffer for page data (on stack, FLASH_PAGE_SIZE is typically 1KB) */
  u32 pageBuffer[FLASH_PAGE_SIZE / 4];
  pBuffer = pageBuffer;
  
  /* Read entire page from flash (memory-mapped, so direct read) */
  pFlashData = (u32 *)pageAddress;
  for (i = 0; i < (FLASH_PAGE_SIZE / 4); i++)
  {
    pBuffer[i] = pFlashData[i];
  }
  
  /* Calculate erase range within the page buffer */
  eraseStartOffset = (Address - pageAddress) / 4;
  eraseEndOffset = eraseStartOffset + (Size / 4);
  
  /* Modify buffer: set erase range to 0xFFFFFFFF (erased state) */
  for (i = eraseStartOffset; i < eraseEndOffset; i++)
  {
    pBuffer[i] = 0xFFFFFFFF;
  }
  
  /* Erase the page */
  flashStatus = FLASH_ErasePage(pageAddress);
  if (flashStatus != FLASH_COMPLETE)
  {
    /* Convert FLASH_State to Memory_EraseStatus */
    switch (flashStatus)
    {
      case FLASH_ERR_ADDR_OUT_OF_RANGE:
        return MEMORY_ERASE_INVALID_ADDRESS;
        
      case FLASH_ERR_WRITE_PROTECTED:
        return MEMORY_ERASE_WRITE_PROTECTED;
        
      case FLASH_TIME_OUT:
        return MEMORY_ERASE_TIMEOUT;
        
      default:
        return MEMORY_ERASE_FLASH_ERROR;
    }
  }
  
  /* Write back the modified page content */
  for (i = 0; i < (FLASH_PAGE_SIZE / 4); i++)
  {
    flashStatus = FLASH_ProgramWordData(pageAddress + (i * 4), pBuffer[i]);
    if (flashStatus != FLASH_COMPLETE)
    {
      /* Convert FLASH_State to Memory_EraseStatus */
      switch (flashStatus)
      {
        case FLASH_ERR_ADDR_OUT_OF_RANGE:
          return MEMORY_ERASE_INVALID_ADDRESS;
          
        case FLASH_ERR_WRITE_PROTECTED:
          return MEMORY_ERASE_WRITE_PROTECTED;
          
        case FLASH_TIME_OUT:
          return MEMORY_ERASE_TIMEOUT;
          
        default:
          return MEMORY_ERASE_FLASH_ERROR;
      }
    }
  }
  
  return MEMORY_ERASE_SUCCESS;
}

