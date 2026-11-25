/*********************************************************************************************************//**
 * @file    ht32_touchkey.h
 * @version $Rev:: V100         $
 * @date    $Date:: 2022-01-11 #$
 * @brief   The header file of HT32 Touch Key Library.
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
#ifndef __HT32_TOUCHKEY_H
#define __HT32_TOUCHKEY_H
#include "ht32.h"

#ifdef __cplusplus
 extern "C" {
#endif

   
/* User used Function -------------------------------------------------------------------------------------*/
extern u32  TKL_Get_Standby(void);
extern u16  TKL_Get_KeyRCCValue(u8 key,u8 freq);
extern u16  TKL_Get_KeyRef(u8 key);
extern u16  TKL_Get_KeyEnv(u8 key);
extern u16  TKL_Get_KeyThreshold(u8 key);
extern u32  TKL_Get_AllKeyState(void);
extern void TKL_Set_KeyThreshold(u8 key,u16 thr);
extern void TKL_Set_Standby(u32 value);
extern u32  TKL_Is_Time(u32 t);
extern u8   TKL_Is_AnyKeyPress(void);
extern u8   TKL_Is_KeyPress(u8 key);
extern u8   TKL_Is_Active(void);
extern u8   TKL_Is_Standby(void);
extern u8   TKL_Is_KeyScanCycle(void);
extern void TKL_Reset(void);
extern void TKL_RAM_Setting(u8 *pRAM);
  
/* Includes ------------------------------------------------------------------------------------------------*/
/* Settings ------------------------------------------------------------------------------------------------*/
/* Exported typedef ----------------------------------------------------------------------------------------*/

typedef struct
{
    u16     SampleTime;
    u16     SleepSampleTime;
    u16     SleepBalanceTime;
    u16     SleepCalTime;
    u16     StandbyTimeDefault;
    u16     PonReBalanceTime;
    u16     ReBalanceTime;
    u16     LowUnBalanceOffset;
    u16     HighUnBalanceOffset;
    u16     UpUnReasonableCAP;
    u16     PonRunTimeNSec;
    u16     RefCalTime;
    u16     NoiseLevel;
    u16     MaxOnHoldTime;
    u8      KeyDebounce;
    u8      KeyAmount;
    u8      TKOSC[2];
    u8      KeyThr[32];
    u8      FastKey;
    u8      MovingCalibration;
    u8      ScanCycle;
}TKL_Config_TypeDef;
		
#define TKL_RAM_Len   (68*TKL_KeyAmount)

#define kT2mS     (1<<0)
#define kT4mS     (1<<1)
#define kT8mS     (1<<2)
#define kT16mS    (1<<3)
#define kT32mS    (1<<4)
#define kT64mS    (1<<5)
#define kT128mS   (1<<6)
#define kT256mS   (1<<7)
#define kT512mS   (1<<8)
#define kT1024mS  (1<<9)
#define kT2048mS  (1<<10)


/****************************************************************************************************/
/* TK Control Register (TKCR) */
#define RODY            13
#define ASMTO           10
#define ASMP             8
#define TK16S            5
#define TKFS             3
#define TKMOD            1
#define TKST             0

//Select the touch key OSC frequency
//TKOSC:touch key OSC
#define TKOSC_MASK                      ((u32)~(RANGE_4<<TKFS))
#define TKOSC_1MHZ                      ((u32)(0<<TKFS))
#define TKOSC_3MHZ                      ((u32)(1<<TKFS))
#define TKOSC_7MHZ                      ((u32)(2<<TKFS))
#define TKOSC_11MHZ                     ((u32)(3<<TKFS))



/****************************************************************************************************/
#define TKL_NOT_SLEEP     0
#define TKL_FIRST_SLEEP   1
#define TKL_NORMAL_SLEEP  2




/* Exported macro ------------------------------------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------------------------------------*/
extern void TKL_IRQHandler(void);
extern void TKL_MainRoutine(void);
extern void TKL_TimerHandler(void);
extern void TKL_LowPowerExit(void);
extern u8   TKL_LowSleepEnter(u8 ScanSeq);
extern u8   TKL_LowPowerCheckWake(u8 ScanSeq, u32 SleepBaseTime); 

extern void TKL_Parame(TKL_Config_TypeDef TKL_Config);
extern void TKL_StandbyTimeCount(void);

#ifdef  HXT_UART_DEBUG
extern void TKL_HXT_UART_TEST(void);
extern void TKL_USART_Configuration(void);
extern u16  TKL_UART_DATA[];
extern u16  TKL_USART_TxBuf[];
extern vu32 TKL_USART_TxIndex;
extern vu32 TKL_USART_TxLen;
#endif


#ifdef __cplusplus
}
#endif

#endif
