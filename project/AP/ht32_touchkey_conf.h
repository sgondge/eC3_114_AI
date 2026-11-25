/*********************************************************************************************************//**
 * @file    ht32_touchkey_conf.h
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
#ifndef __HT32_TOUCHKEY_CONF_H
#define __HT32_TOUCHKEY_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------------------------------------*/
/* Settings ------------------------------------------------------------------------------------------------*/
#define RTC_USE_LSE                     (0)
#define WAKEUP_TIME_TEST                (1) // Shall be remove in formal example

#define TKL_ENABLE                      1
#define TKL_DISABLE                     0


;//;--------------------------------
;//;-TOUCH KEY LIBRARY VARIES DEFINE
;//;--------------------------------

//-------- <<< Use Configuration Wizard in Context Menu >>> ------------------*/

// <h> TKL_Config
  // <q> PowerSave
  // <i> Low power mode, 0=disable 1=enalbe 
  #define PowerSave             	0
  
  // <o> TKL_HighSensitive 
  // <i> Touch key Sensitive, 0=Low 1=Normal 2=High Sensitive
  // <i> Increasing the sensitivity will also increase the noise relatively
	// <i> Therefore, suggested settings normal sensitivity 
  // <0=>     Low
  // <1=>     Normal
  // <2=>     High
  #define TKL_HighSensitive     	1
	
  // <o> TKL_KeyDebounce
  // <i> Debounce in output key status, Range 0~200
  // <0-200>
  #define TKL_KeyDebounce         3

  // <q> TKL_MovingCalibration
  // <i> Touch key Moving Calibration keys easier to operate
  // <i> 0=Close 1=Open
  #define TKL_MovingCalibration		1
	
  // <o> TKL_RefCalTime 
  // <i> Ref Cal time (AutoCalibrationPeriod)
  // <i> The faster speed, the better the anti-interference, and the worse the sensitivity
  // <i> The slower speed, the worse the anti-interference and the better the sensitivity
  // <200=>     0.2S     
  // <400=>     0.4S
  // <600=>     0.6S
  // <800=>     0.8S
  // <1000=>    1S
  // <2000=>    2S
  // <3000=>    3S
  // <4000=>    4S
  // <5000=>    5S
  // <10000=>   10S
  // <20000=>   20S
  #define  TKL_RefCalTime  600
  
  // <o> TKL_MaxOnHoldTime 
  // <i> key holding time ,if time out will reset key status to non-press
  // <500=>       0.5S     
  // <1000=>      1S
  // <2000=>      2S
  // <3000=>      3S
  // <4000=>      4S
  // <5000=>      5S
  // <10000=>     10S
  // <20000=>     20S
  // <30000=>     30S
  // <60000=>     1MIN
  // <120000=>    2MIN
  // <180000=>    3MIN
  // <240000=>    4MIN
  // <300000=>    5MIN
  #define  TKL_MaxOnHoldTime  1000
  
// </h>


// <h> TKL_ENABLE_KEY
  // <i> HT32F54241 max 24KEY 
  // <i> HT32F54241 max 28KEY 
  // <i> If the setting over the maximum key, it will be set disable
  
  // <e> KEY00_EN
    // <i> 0=KEY00 disable 1=KEY00 enalbe 
      #define KEY00_EN                1
    // <o> Key00Threshold
    // <i> suggestion range 10 ~ 255
    // <10-255>
      #define Key00Threshold          65
  // </e>
  
  // <e> KEY01_EN
    // <i> 0=KEY01 disable 1=KEY01 enalbe 
      #define KEY01_EN                1
    // <o> Key01Threshold
    // <i> suggestion range 10 ~ 255
    // <10-255>
      #define Key01Threshold          65
  // </e>
  
  // <e> KEY02_EN     
    // <i> 0=KEY02  disable 1=KEY02  enalbe
      #define KEY02_EN                1    
    // <o> Key02Threshold     
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key02Threshold          65  
  // </e>

  // <e> KEY03_EN     
    // <i> 0=KEY03  disable 1=KEY03  enalbe
      #define KEY03_EN                1    
    // <o> Key03Threshold     
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key03Threshold          65  
  // </e>

  // <e> KEY04_EN     
    // <i> 0=KEY04  disable 1=KEY04  enalbe
      #define KEY04_EN                1
    // <o> Key04Threshold     
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key04Threshold          65  
  // </e>

  // <e> KEY05_EN     
    // <i> 0=KEY05  disable 1=KEY05  enalbe
      #define KEY05_EN                1
    // <o> Key05Threshold     
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key05Threshold          65  
  // </e>

  // <e> KEY06_EN     
    // <i> 0=KEY06  disable 1=KEY06  enalbe
      #define KEY06_EN                1
    // <o> Key06Threshold      
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key06Threshold          65  
  // </e>

  // <e> KEY07_EN     
    // <i> 0=KEY07  disable 1=KEY07  enalbe
      #define KEY07_EN                1
    // <o> Key07Threshold     
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key07Threshold          65  
  // </e>

  // <e> KEY08_EN    
     // <i> 0=KEY08  disable 1=KEY08  enalb
       #define KEY08_EN               1
     // <o> Key08Threshold     
     // <i> suggestion range 10 ~ 255   
     // <10-255>     
       #define Key08Threshold         65 
   // </e>
  // <e> KEY09_EN     
    // <i> 0=KEY09  disable 1=KEY09  enalbe
      #define KEY09_EN                1
    // <o> Key09Threshold     
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key09Threshold          65  
  // </e>

  // <e> KEY10_EN    
    // <i> 0=KEY10 disable 1=KEY10 enalbe
    // <i> note:the key pin share with CLK
      #define KEY10_EN                1    
    // <o> Key10Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key10Threshold          65  
  // </e>

  // <e> KEY11_EN    
    // <i> 0=KEY11 disable 1=KEY11 enalbe
    // <i> note:the key pin share with DIO
      #define KEY11_EN                1    
    // <o> Key11Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key11Threshold          65  
  // </e>

  // <e> KEY12_EN    
    // <i> 0=KEY12 disable 1=KEY12 enalbe
      #define KEY12_EN                1
    // <o> Key12Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key12Threshold          45  
  // </e>

  // <e> KEY13_EN    
    // <i> 0=KEY13 disable 1=KEY13 enalbe
      #define KEY13_EN                1
    // <o> Key13Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key13Threshold          45  
  // </e>

  // <e> KEY14_EN    
    // <i> 0=KEY14 disable 1=KEY14 enalbe
      #define KEY14_EN                1
    // <o> Key14Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key14Threshold          45  
  // </e>

  // <e> KEY15_EN    
    // <i> 0=KEY15 disable 1=KEY15 enalbe
      #define KEY15_EN                1
    // <o> Key15Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key15Threshold          65  
  // </e>

  // <e> KEY16_EN    
    // <i> 0=KEY16 disable 1=KEY16 enalbe
      #define KEY16_EN                1
    // <o> Key16Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key16Threshold          65  
  // </e>

  // <e> KEY17_EN    
    // <i> 0=KEY17 disable 1=KEY17 enalbe
      #define KEY17_EN                1
    // <o> Key17Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key17Threshold          65  
  // </e>

  // <e> KEY18_EN    
    // <i> 0=KEY18 disable 1=KEY18 enalbe
      #define KEY18_EN                0
    // <o> Key18Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key18Threshold          24  
  // </e>

  // <e> KEY19_EN    
    // <i> 0=KEY19 disable 1=KEY19 enalbe
      #define KEY19_EN                0
    // <o> Key19Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key19Threshold          24  
  // </e>

  // <e> KEY20_EN    
    // <i> 0=KEY20 disable 1=KEY20 enalbe
      #define KEY20_EN                0
    // <o> Key20Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key20Threshold          24  
  // </e>

  // <e> KEY21_EN    
    // <i> 0=KEY21 disable 1=KEY21 enalbe
      #define KEY21_EN                0
    // <o> Key21Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key21Threshold          24  
  // </e>

  // <e> KEY22_EN    
    // <i> 0=KEY22 disable 1=KEY22 enalbe
      #define KEY22_EN                0
    // <o> Key22Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key22Threshold          24  
  // </e>

  // <e> KEY23_EN    
    // <i> 0=KEY23 disable 1=KEY23 enalbe
      #define KEY23_EN                0
    // <o> Key23Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key23Threshold          24  
  // </e>

  // <e> KEY24_EN    
    // <i> 0=KEY24 disable 1=KEY24 enalbe
      #define KEY24_EN                0    
    // <o> Key24Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key24Threshold          24  
  // </e>

  // <e> KEY25_EN    
    // <i> 0=KEY25 disable 1=KEY25 enalbe
      #define KEY25_EN                0    
    // <o> Key25Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key25Threshold          24  
  // </e>

  // <e> KEY26_EN    
    // <i> 0=KEY26 disable 1=KEY26 enalbe
      #define KEY26_EN                0    
    // <o> Key26Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key26Threshold          24  
  // </e>

  // <e> KEY27_EN    
    // <i> 0=KEY27 disable 1=KEY27 enalbe
      #define KEY27_EN                0    
    // <o> Key27Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key27Threshold          24  
  // </e>

  // <e> KEY28_EN    
    // <i> 0=KEY28 disable 1=KEY28 enalbe
      #define KEY28_EN                0
    // <o> Key28Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key28Threshold          24  
  // </e>

  // <e> KEY29_EN    
    // <i> 0=KEY29 disable 1=KEY29 enalbe
      #define KEY29_EN                0
    // <o> Key29Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key29Threshold          24  
  // </e>

  // <e> KEY30_EN    
    // <i> 0=KEY30 disable 1=KEY30 enalbe
      #define KEY30_EN                0 
    // <o> Key30Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key30Threshold          24  
  // </e>

  // <e> KEY31_EN    
    // <i> 0=KEY31 disable 1=KEY31 enalbe
      #define KEY31_EN                0
    // <o> Key31Threshold    
    // <i> suggestion range 10 ~ 255    
    // <10-255>      
      #define Key31Threshold          24  
  // </e>
  
// </h>


/*--------------------- Clock Configuration ----------------------------------*/


 #if  TKL_KeyAmount > 0
     #define USE_KEY00    KEY00_EN
 #else
     #define USE_KEY00   0 
 #endif

 #if  TKL_KeyAmount > 1     
    #define USE_KEY01   KEY01_EN  
 #else   
    #define USE_KEY01   0 
 #endif

 #if  TKL_KeyAmount > 2     
    #define USE_KEY02   KEY02_EN  
 #else   
    #define USE_KEY02   0 
 #endif

 #if  TKL_KeyAmount > 3     
    #define USE_KEY03   KEY03_EN  
 #else   
    #define USE_KEY03   0 
 #endif

 #if  TKL_KeyAmount > 4     
    #define USE_KEY04   KEY04_EN  
 #else   
    #define USE_KEY04   0 
 #endif

 #if  TKL_KeyAmount > 5     
    #define USE_KEY05   KEY05_EN  
 #else   
    #define USE_KEY05   0 
 #endif

 #if  TKL_KeyAmount > 6     
    #define USE_KEY06   KEY06_EN  
 #else   
    #define USE_KEY06   0 
 #endif

 #if  TKL_KeyAmount > 7     
    #define USE_KEY07   KEY07_EN  
 #else   
    #define USE_KEY07   0 
 #endif

 #if  TKL_KeyAmount > 8     
    #define USE_KEY08   KEY08_EN  
 #else   
    #define USE_KEY08   0 
 #endif

 #if  TKL_KeyAmount > 9     
    #define USE_KEY09   KEY09_EN  
 #else   
    #define USE_KEY09   0 
 #endif

 #if  TKL_KeyAmount > 10    
    #define USE_KEY10  KEY10_EN 
 #else   
    #define USE_KEY10  0 
 #endif

 #if  TKL_KeyAmount > 11    
    #define USE_KEY11  KEY11_EN 
 #else   
    #define USE_KEY11  0 
 #endif

 #if  TKL_KeyAmount > 12    
    #define USE_KEY12  KEY12_EN 
 #else   
    #define USE_KEY12  0 
 #endif

 #if  TKL_KeyAmount > 13    
    #define USE_KEY13  KEY13_EN 
 #else   
    #define USE_KEY13  0 
 #endif

 #if  TKL_KeyAmount > 14    
    #define USE_KEY14  KEY14_EN 
 #else   
    #define USE_KEY14  0 
 #endif

 #if  TKL_KeyAmount > 15    
    #define USE_KEY15  KEY15_EN 
 #else   
    #define USE_KEY15  0 
 #endif

 #if  TKL_KeyAmount > 16    
    #define USE_KEY16  KEY16_EN 
 #else   
    #define USE_KEY16  0 
 #endif

 #if  TKL_KeyAmount > 17    
    #define USE_KEY17  KEY17_EN 
 #else   
    #define USE_KEY17  0 
 #endif

 #if  TKL_KeyAmount > 18    
    #define USE_KEY18  KEY18_EN 
 #else   
    #define USE_KEY18  0 
 #endif

 #if  TKL_KeyAmount > 19    
    #define USE_KEY19  KEY19_EN 
 #else   
    #define USE_KEY19  0 
 #endif

 #if  TKL_KeyAmount > 20    
    #define USE_KEY20  KEY20_EN 
 #else   
    #define USE_KEY20  0 
 #endif

 #if  TKL_KeyAmount > 21    
    #define USE_KEY21  KEY21_EN 
 #else   
    #define USE_KEY21  0 
 #endif

 #if  TKL_KeyAmount > 22    
    #define USE_KEY22  KEY22_EN 
 #else   
    #define USE_KEY22  0 
 #endif

 #if  TKL_KeyAmount > 23    
    #define USE_KEY23  KEY23_EN 
 #else   
    #define USE_KEY23  0 
 #endif

 #if  TKL_KeyAmount > 24    
    #define USE_KEY24  KEY24_EN 
 #else   
    #define USE_KEY24  0 
 #endif

 #if  TKL_KeyAmount > 25    
    #define USE_KEY25  KEY25_EN 
 #else   
    #define USE_KEY25  0 
 #endif

 #if  TKL_KeyAmount > 26    
    #define USE_KEY26  KEY26_EN 
 #else   
    #define USE_KEY26  0 
 #endif

 #if  TKL_KeyAmount > 27    
    #define USE_KEY27  KEY27_EN 
 #else   
    #define USE_KEY27  0 
 #endif

 #if  TKL_KeyAmount > 28    
    #define USE_KEY28  KEY28_EN 
 #else   
    #define USE_KEY28  0 
 #endif

 #if  TKL_KeyAmount > 29    
    #define USE_KEY29  KEY29_EN 
 #else   
    #define USE_KEY29  0 
 #endif

 #if  TKL_KeyAmount > 30    
    #define USE_KEY30  KEY30_EN 
 #else   
    #define USE_KEY30  0 
 #endif

 #if  TKL_KeyAmount > 31    
    #define USE_KEY31  KEY31_EN 
 #else   
    #define USE_KEY31  0 
 #endif
  
/* Exported typedef ----------------------------------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif

