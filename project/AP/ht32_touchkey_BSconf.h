/*********************************************************************************************************//**
 * @file    ht32_touchkey_bsconf.h
 * @version $Rev:: 2            $
 * @date    $Date:: 2021-06-09 #$
 * @brief   The configuration file of HT32 Touch Key Library.
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
#ifndef __HT32_TOUCHKEY_BSCONF_H
#define __HT32_TOUCHKEY_BSCONF_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------------------------------------*/
/* Settings ------------------------------------------------------------------------------------------------*/


//*******************************************************************   
//*******************  W A R N I N G ********************************   
//*******************************************************************   
/*    
    
  If you want to modify the following parameters    
  Please contact BS FAE   
  If you change it yourself, you may have wrong   
      
*/    
//*******************************************************************   
//*******************  W A R N I N G ********************************   
//*******************************************************************   

//**************************************************************************//
                                                                            //
                                                                            //
//*******************************************************************       //
//in sleep per N sec caliration once REF                            *       //
//base time refer RTC                                               *       //
//Range 500~60000 mS                                                *       //
//*******************************************************************       //
#define   TKL_SleepCalTime        2000  //mS                                //
                                                                            //
//*******************************************************************       //
//in sleep per N sec check once balance Status                      *       //
//base time refer RTC                                               *       //
//Range 500~60000 mS                                                *       //
//*******************************************************************       //
#define   TKL_SleepBalanceTime    3000  //mS                                //
                                                                            //
//*******************************************************************       //
//Standby Default value                                             *       //
//base time refer 8mS                                               *       //
//Range 500~60000 mS                                                *       //
//*******************************************************************       //
#define   TKL_StandbyTimeDefault  1000  //mS                                //
                                                                            //
//*******************************************************************       //
//Normal check ReBalance time                                       *       //
//base time refer 8mS                                               *       //
//Range 500~60000 mS                                                *       //
//*******************************************************************       //
#define   TKL_ReBalanceTime       6000  //mS                                //
                                                                            //
//*******************************************************************       //
//Power On check ReBalance time                                     *       //
//base time refer 8mS                                               *       //
//Range 500~60000 mS                                                *       //
//*******************************************************************       //
#define   TKL_PonReBalanceTime    2000  //mS                                //
                                                                            //
//*******************************************************************       //
//Power on count timer                                              *       //
//base time refer 8mS                                               *       //
//Range 500~60000 mS                                                *       //
//*******************************************************************       //
#define   TKL_PonRunTimeNSec      8000  //mS                                //
                                                                            //
//*******************************************************************       //
//Rebalance RCC max value                                           *       //
//Range 1000~1023 RCC                                               *       //
//                                                                  *       //
//*******************************************************************       //
#define   TKL_UpUnReasonableCAP   1000  //RCC                               //
                                                                            //
																																						//
#define TKL_FastKey         			1																					//
																																						//
#define TKL_TouchScanCycle   			3																					//
																																						//
																																						//
#define TKL_NoiseLevel              16                                      //
#define kLibSampleLevel_1          254                                      //
#define kLibSampleLevel_2          510                                      //
#define kLibSampleLevel_3         1022                                      //
#define kLibSampleLevel_4         2046                                      //
#define kLibSampleLevel_5         3070                                      //
#define kLibSampleLevel_6         4094                                      //
#define kLibSampleLevel_7         6143                                      //
#define kLibSampleLevel_8         8191                                      //
#define kLibSampleLevel_9        12287                                      //
                                                                            //
#if TKL_HighSensitive == 0                                                  //
  #define TKL_SAMPLE_LEN          kLibSampleLevel_4                         //
  #define TKL_SLEEP_SAMPLE_LEN    kLibSampleLevel_2                         //
#endif                                                                      //
#if TKL_HighSensitive == 1                                                  //
  #define TKL_SAMPLE_LEN          kLibSampleLevel_6                         //
  #define TKL_SLEEP_SAMPLE_LEN    kLibSampleLevel_3                         //
#endif                                                                      //
#if TKL_HighSensitive == 2                                                  //
  #define TKL_SAMPLE_LEN          kLibSampleLevel_8                         //
  #define TKL_SLEEP_SAMPLE_LEN    kLibSampleLevel_4                         //
#endif                                                                      //
#if TKL_HighSensitive == 3                                                  //
  #define TKL_SAMPLE_LEN          kLibSampleLevel_9                         //
  #define TKL_SLEEP_SAMPLE_LEN    kLibSampleLevel_5                         //
#endif                                                                      //
//**************************************************************************//
  
/* Exported typedef ----------------------------------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif

