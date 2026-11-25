/*********************************************************************************************************//**
 * @file    TouchKey/TouchKey_LIB/ht32f5xxxx_01_it.c
 * @version $Rev:: 2            $
 * @date    $Date:: 2021-06-09 #$
 * @brief   This file provides all interrupt service routine.
 *************************************************************************************************************
 * @attention
 *
 * Firmware Disclaimer Information
 *
 * 1. The customer hereby acknowledges and agrees that the program technical documentation, including the
 *    code, which is supplied by Holtek Semiconductor Inc., (hereinafter referred to as "HOLTEK") is the
 *    proprietary and confidential intellectual property of HOLTEK, and is protected by copyright law and
 *    other intellectual property laws.
 *
 * 2. The customer hereby acknowledges and agrees that the program technical documentation, including the
 *    code, is confidential information belonging to HOLTEK, and must not be disclosed to any third parties
 *    other than HOLTEK and the customer.
 *
 * 3. The program technical documentation, including the code, is provided "as is" and for customer reference
 *    only. After delivery by HOLTEK, the customer shall use the program technical documentation, including
 *    the code, at their own risk. HOLTEK disclaims any expressed, implied or statutory warranties, including
 *    the warranties of merchantability, satisfactory quality and fitness for a particular purpose.
 *
 * <h2><center>Copyright (C) Holtek Semiconductor Inc. All rights reserved</center></h2>
 ************************************************************************************************************/

/* Includes ------------------------------------------------------------------------------------------------*/
#include "ht32.h"
#include "ht32_board.h"
#include "ht32_board_config.h"
#include "ht32_touchkey.h"
#include "I2C0.h"


/** @addtogroup HT32_Series_Peripheral_Examples HT32 Peripheral Examples
  * @{
  */

/** @addtogroup TouchKey_Examples TouchKey
  * @{
  */

/** @addtogroup TouchKey_LIB
  * @{
  */

void HardFault_Handler_C(uint32_t* stack_pointer);

/* Global functions ----------------------------------------------------------------------------------------*/
/*********************************************************************************************************//**
 * @brief   This function handles NMI exception.
 * @retval  None
 ************************************************************************************************************/
void NMI_Handler(void)
{
}

/*********************************************************************************************************//**
 * @brief   This function handles Hard Fault exception.
 * @retval  None
 ************************************************************************************************************/
//void HardFault_Handler(void)
//{
//  #if 1

//  static vu32 gIsContinue = 0;

//  /*--------------------------------------------------------------------------------------------------------*/
//  /* For development FW, MCU run into the while loop when the hardfault occurred.                           */
//  /* 1. Stack Checking                                                                                      */
//  /*    When a hard fault exception occurs, MCU push following register into the stack (main or process     */
//  /*    stack). Confirm R13(SP) value in the Register Window and typing it to the Memory Windows, you can   */
//  /*    check following register, especially the PC value (indicate the last instruction before hard fault).*/
//  /*    SP + 0x00    0x04    0x08    0x0C    0x10    0x14    0x18    0x1C                                   */
//  /*           R0      R1      R2      R3     R12      LR      PC    xPSR                                   */
//  while (gIsContinue == 0)
//  {
//  }
//  /* 2. Step Out to Find the Clue                                                                           */
//  /*    Change the variable "gIsContinue" to any other value than zero in a Local or Watch Window, then     */
//  /*    step out the HardFault_Handler to reach the first instruction after the instruction which caused    */
//  /*    the hard fault.                                                                                     */
//  /*--------------------------------------------------------------------------------------------------------*/

//  #else

//  /*--------------------------------------------------------------------------------------------------------*/
//  /* For production FW, you shall consider to reboot the system when hardfault occurred.                    */
//  /*--------------------------------------------------------------------------------------------------------*/
//  NVIC_SystemReset();

//  #endif
//}


void HardFault_Handler(void) 
{
    // Immediate reset (no conditions)
    NVIC_SystemReset();

    // Fallback in case reset fails (e.g., due to hardware failure)
    while (1) {
			NVIC_SystemReset();
			break;
    }
}

/*********************************************************************************************************//**
 * @brief   This function handles SVCall exception.
 * @retval  None
 ************************************************************************************************************/
void SVC_Handler(void)
{
}

/*********************************************************************************************************//**
 * @brief   This function handles PendSVC exception.
 * @retval  None
 ************************************************************************************************************/
void PendSV_Handler(void)
{
}

/*********************************************************************************************************//**
 * @brief   This function handles SysTick Handler.
 * @retval  None
 ************************************************************************************************************/
void SysTick_Handler(void)
{
}

/*********************************************************************************************************//**
 * @brief   This function handles RTC interrupt.
 * @retval  None
 * @details In RTC interrupt service routine:
 ************************************************************************************************************/
//void RTC_IRQHandler(void)
//{
//  u32 uFlags = RTC_GetFlagStatus(); // Read Clear

//  if (uFlags & RTC_FLAG_CSEC) // Second interrupt
//  {

//  }

//  #if 0
//  if (uFlags & RTC_FLAG_CM) // Match interrupt
//  {

//  }
//  #endif
//}

/*********************************************************************************************************//**
 * @brief   This function handles BFTM0 interrupt.
 * @retval  None
 ************************************************************************************************************/
//void BFTM0_IRQHandler(void)
//{
//  BFTM_ClearFlag(HT_BFTM0);
//  TKL_TimerHandler();
//}


/*********************************************************************************************************//**
 * @brief   This function handles TKEY interrupt.
 * @retval  None
 ************************************************************************************************************/
void TKEY_IRQHandler(void)
{
  TKL_IRQHandler();
}


/*********************************************************************************************************//**
 * @brief   This function handles ADC interrupt.
 * @retval  None
 ************************************************************************************************************/
void HTCFG_ADC_IRQHandler(void)
{
    extern vu32 gBATVoltageLevel;
  extern volatile bool gBATV_SingleEOC;
    
  ADC_ClearIntPendingBit(HT_ADC, ADC_FLAG_SINGLE_EOC);
  gBATVoltageLevel = (HT_ADC->DR[0] & 0x0FFF);
  gBATV_SingleEOC = TRUE;
}

/*********************************************************************************************************//**
 * @brief   This function handles UART interrupt.
 * @retval  None
 ************************************************************************************************************/

void EXTI4_15_IRQHandler(void)
{
		I2C_LowPowerClearWakeFlags();
}

//void UART0_IRQHandler(void)
//{
//  
//#ifdef  HXT_UART_DEBUG
//  if (USART_GetIntStatus(HTCFG_UART_PORT, USART_INT_TXDE) &&
//      USART_GetFlagStatus(HTCFG_UART_PORT, USART_FLAG_TXDE))
//  {
//    if(TKL_USART_TxIndex>0){
//      USART_SendData(HTCFG_UART_PORT,TKL_USART_TxBuf[(TKL_USART_TxLen-TKL_USART_TxIndex)]);
//      TKL_USART_TxIndex--;
//    }
//    else{
//      USART_IntConfig(HTCFG_UART_PORT, USART_INT_TXDE, DISABLE);
//    }
//  }
//  
//  
//  if (USART_GetIntStatus(HTCFG_UART_PORT, USART_INT_TXC) &&
//      USART_GetFlagStatus(HTCFG_UART_PORT, USART_FLAG_TXC))
//  {
//    USART_IntConfig(HTCFG_UART_PORT, USART_INT_TXC, DISABLE);
//  }
//  
//#endif
//  
//}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

