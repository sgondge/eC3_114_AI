/*********************************************************************************************************//**
 * @file    ht32f54241_TKM.h
 * @version $Rev:: V100         $
 * @date    $Date:: 2022-01-11 #$
 * @brief   The header file of the TKM library.
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

/* Define to prevent recursive inclusion -------------------------------------------------------------------*/
#ifndef __HT32F54241_TKM_H
#define __HT32F54241_TKM_H
#include "ht32.h"

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------------------------------------*/
#include "ht32.h"

/** @addtogroup HT32F54241_Peripheral_Driver HT32F54241 Peripheral Driver
  * @{
  */

/** @addtogroup Touch
  * @{
  */




/* Exported constants --------------------------------------------------------------------------------------*/
/** @defgroup Touch_Exported_Constants Touch exported constants
  * @{
  */

#define HT_TKM_BASE             (HT_APBPERIPH_BASE + 0x1A000)    /* 0x4001A000                             */
#define HT_TKM                  ((TKM_TypeDef *) HT_TKM_BASE)




/****************************************************************************************************/
#define TKMn            2
#define RANGE_2         0x01
#define RANGE_4         0x03
#define RANGE_8         0x07
#define RANGE_16        0x0f

#define REFOSC_STABLE_DELAY_MASK        ((u32)~(RANGE_8<<RODY))
#define REFOSC_STABLE_DELAY2            ((u32)(1<<RODY))
#define REFOSC_STABLE_DELAY4            ((u32)(2<<RODY))
#define REFOSC_STABLE_DELAY8            ((u32)(3<<RODY))
#define REFOSC_STABLE_DELAY16           ((u32)(4<<RODY))
#define REFOSC_STABLE_DELAY32           ((u32)(5<<RODY))
#define REFOSC_STABLE_DELAY64           ((u32)(6<<RODY))
//Periodic auto scan mode time-out selection
//PST:Periodic scan Timer
#define PST_TIMEOUT_MASK                ((u32)~(RANGE_8<<ASMTO))
#define PST_TIMEOUT_256MS               ((u32)(0<<ASMTO))
#define PST_TIMEOUT_512MS               ((u32)(1<<ASMTO))
#define PST_TIMEOUT_1024MS              ((u32)(2<<ASMTO))
#define PST_TIMEOUT_2048MS              ((u32)(3<<ASMTO))
#define PST_TIMEOUT_4096MS              ((u32)(4<<ASMTO))
#define PST_TIMEOUT_8192MS              ((u32)(5<<ASMTO))
//Periodic auto scan mode period selection
//PSP:Periodic scan period
#define PSP_MASK                        ((u32)~(RANGE_4<<ASMP))
#define PSP_512MS                       ((u32)(0<<ASMP))
#define PSP_256MS                       ((u32)(1<<ASMP))
#define PSP_128MS                       ((u32)(2<<ASMP))
#define PSP_64MS                        ((u32)(3<<ASMP))
//Select the touch key module 16-bit counter clock source
//TK16_CLK:touch key module 16-bit counter clock source
#define TK16_CLK_MASK                   ((u32)~(RANGE_4<<TK16S))
#define TK16_CLK_FPCLK_DIV_16           ((u32)(0<<TK16S))
#define TK16_CLK_FPCLK_DIV_32           ((u32)(1<<TK16S))
#define TK16_CLK_FPCLK_DIV_64           ((u32)(2<<TK16S))
#define TK16_CLK_FPCLK_DIV_128          ((u32)(3<<TK16S))
//Touch key mode select
//TK_MODE:Touch key mode
#define TK_MODE_MASK                    ((u32)~(RANGE_4<<TKMOD))
#define TK_MODE_AUTO                    ((u32)(0<<TKMOD))
#define TK_MODE_MANUAL                  ((u32)(1<<TKMOD))
#define TK_MODE_PERIODIC_AUTO           ((u32)(2<<TKMOD))

//Start touch key detection control
#define TKM_START                       ((u32)(1<<TKST))
#define TKM_STOP                        ((u32)~(RANGE_2<<TKST))
/****************************************************************************************************/

/****************************************************************************************************/
/* TK Status Register (TKSR) */
#define TKBUSY          4
#define TKCFOV          3
#define TK16OV          2
#define TKRCOVF         1
#define TKTHF           0
/****************************************************************************************************/

/****************************************************************************************************/
/* TKMn Control Register (TKMnCR) */
#define MnTSS           8
#define MnROEN          7
#define MnKOEN          6
#define MnDFEN          5
#define MnSOFC          3
#define MnSOF           0

#define TKM_CLOCK_MASK                  ((u32)~(RANGE_4<<MnTSS))
#define TKM_CLOCK_REF                   ((u32)(0<<MnTSS))
#define TKM_CLOCK_FPCLK_DIV_32          ((u32)(1<<MnTSS))
#define TKM_CLOCK_FPCLK_DIV_64          ((u32)(2<<MnTSS))
#define TKM_CLOCK_FPCLK_DIV_128         ((u32)(3<<MnTSS))

/****************************************************************************************************/

/****************************************************************************************************/
/* TKMn Key Configuration Register (TKMnKCFGR) */
#define KEY0             0
#define KEY1             1
#define KEY2             2
#define KEY3             3

#define MnSK3           22
#define MnSK2           20
#define MnSK1           18
#define MnSK0           16

#define MnK3THS         11
#define MnK2THS         10
#define MnK1THS          9
#define MnK0THS          8
#define MnK3EN           3
#define MnK2EN           2
#define MnK1EN           1
#define MnK0EN           0
/****************************************************************************************************/

/**
 * @brief Touch Key Module
 */
typedef struct
{
  __IO uint32_t CR;              /*!< 0x000          Touch Key Module Control Register                      */
  __IO uint32_t KCFGR;           /*!< 0x004          Touch Key Module Key Configuration Register            */
  __IO uint32_t SR;              /*!< 0x008          Touch Key Module Status Register                       */
  __IO uint32_t ROCPR;           /*!< 0x00C          Touch Key Module Reference OSC Capacitor Register      */
  __IO uint32_t K3CPR;           /*!< 0x010          Touch Key Module Key 3 Capacitor Register              */
  __IO uint32_t K2CPR;           /*!< 0x014          Touch Key Module Key 2 Capacitor Register              */
  __IO uint32_t K1CPR;           /*!< 0x018          Touch Key Module Key 1 Capacitor Register              */
  __IO uint32_t K0CPR;           /*!< 0x01C          Touch Key Module Key 0 Capacitor Register              */
  __IO uint32_t CFCNTR;          /*!< 0x020          Touch Key Module C/F Counter Register                  */
  __IO uint32_t K3CNTR;          /*!< 0x024          Touch Key Module Key 3 Counter Register                */
  __IO uint32_t K2CNTR;          /*!< 0x028          Touch Key Module Key 2 Counter Register                */
  __IO uint32_t K1CNTR;          /*!< 0x02C          Touch Key Module Key 1 Counter Register                */
  __IO uint32_t K0CNTR;          /*!< 0x030          Touch Key Module Key 0 Counter Register                */
  __IO uint32_t K3THR;           /*!< 0x034          Touch Key Module Key 3 Threshold Register              */
  __IO uint32_t K2THR;           /*!< 0x038          Touch Key Module Key 2 Threshold Register              */
  __IO uint32_t K1THR;           /*!< 0x03C          Touch Key Module Key 1 Threshold Register              */
  __IO uint32_t K0THR;           /*!< 0x040          Touch Key Module Key 0 Threshold Register              */
       uint32_t RESERVED0[47];   /*!< 0x044 - 0x0FC  Reserved                                               */
} TKMx_TypeDef;

/**
 * @brief Touch Key
 */
typedef struct
{
                                 /* TKEY: 0x4001A000                                                        */
  __IO uint32_t  TKCR;           /*!< 0x000          Touch Key Control Register                             */
  __IO uint32_t  TKCNTR;         /*!< 0x004          Touch Key Counter Register                             */
  __IO uint32_t  TKTSCRR;        /*!< 0x008          Touch Key Time Slot Counter Reload Register            */
  __IO uint32_t  TKIER;          /*!< 0x00C          Touch Key Interrupt Enable Register                    */
  __IO uint32_t  TKSR;           /*!< 0x010          Touch Key Status Register                              */
       uint32_t  RESERVED0[59];  /*!< 0x014 - 0x0FC  Reserved                                               */
  TKMx_TypeDef TKM[TKMn];        /*!< 0x100          Touch Key Module 0 registers                           */
//  TKMx_TypeDef TKM1;           /*!< 0x200          Touch Key Module 1 registers                           */
//  TKMx_TypeDef TKM2;           /*!< 0x300          Touch Key Module 2 registers                           */
//  TKMx_TypeDef TKM3;           /*!< 0x400          Touch Key Module 3 registers                           */
//  TKMx_TypeDef TKM4;           /*!< 0x500          Touch Key Module 4 registers                           */
//  TKMx_TypeDef TKM5;           /*!< 0x600          Touch Key Module 5 registers                           */
} TKM_TypeDef;
/****************************************************************************************************/


#define TIMING0     0
#define TIMING1     1 

/**
  * @}
  */

/* Exported functions --------------------------------------------------------------------------------------*/
/** @defgroup Touch_Exported_Functions Touch Exported Functions
  * @{
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

#endif
