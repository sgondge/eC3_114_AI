/*********************************************************************************************************//**
 * @file    TouchKey/TouchKey_LIB/ht32_board_config.h
 * @version $Rev:: 2            $
 * @date    $Date:: 2021-06-09 #$
 * @brief   The header file of board configuration.
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
#ifndef __HT32_BOARD_CONFIG_H
#define __HT32_BOARD_CONFIG_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Settings ------------------------------------------------------------------------------------------------*/
#define IAP_CODE_SIZE                 (1024 * 12)          /* IAP code size in Bytes                        */

#define IAP_APFLASH_SIZE              (LIBCFG_FLASH_SIZE)
#define IAP_APSRAM_SIZE               (LIBCFG_RAM_SIZE)

#define IAP_APFLASH_START             (IAP_CODE_SIZE + 0)
#define IAP_APSRAM_START              (HT_SRAM_BASE)

#define BOOT_MODE_AP                  (0x55AAFAF0)
#define BOOT_MODE_IAP                 (0x55AAFAF5)
#define BOOT_MODE                     ((*((u32 volatile *) (HT_SRAM_BASE))))

/* 0: MCU stays awake (factory/battery accessible without I2C stove masters) */
#define ENABLE_MCU_LOW_POWER          0



/*********************************************************************************************************//**
  * @brief  HXT TEST MODE.
  * @retval None
  ***********************************************************************************************************/
   #if(0) //HXT TEST MODE 
    #define HXT_UART_DEBUG
   
    #if(1)
     #define HXT_READ_REF_ENV
    #else
     #define HXT_READ_RCC0_RCC1
    #endif
   
   #endif
   
/*********************************************************************************************************//**
  * @brief  Touch key Config.
  * @retval None
  ***********************************************************************************************************/
/* Settings ------------------------------------------------------------------------------------------------*/
#if defined(USE_HT32F54231_41)
    #define TKL_KeyAmount 24
   
    #define TKL_SEL_KEY0_AFIO    AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_4  ,AFIO_MODE_12);
    #define TKL_SEL_KEY1_AFIO    AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_5  ,AFIO_MODE_12);
    #define TKL_SEL_KEY2_AFIO    AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_6  ,AFIO_MODE_12);
    #define TKL_SEL_KEY3_AFIO    AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_7  ,AFIO_MODE_12);
    #define TKL_SEL_KEY4_AFIO    AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_15 ,AFIO_MODE_12);
    #define TKL_SEL_KEY5_AFIO    AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_0  ,AFIO_MODE_12);
    #define TKL_SEL_KEY6_AFIO    AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_8  ,AFIO_MODE_12);
    #define TKL_SEL_KEY7_AFIO    AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_9  ,AFIO_MODE_12);
    #define TKL_SEL_KEY8_AFIO    AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_10 ,AFIO_MODE_12);
    #define TKL_SEL_KEY9_AFIO    AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_11 ,AFIO_MODE_12);
    #define TKL_SEL_KEY10_AFIO   AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_12 ,AFIO_MODE_12);
    #define TKL_SEL_KEY11_AFIO   AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_13 ,AFIO_MODE_12);
    #define TKL_SEL_KEY12_AFIO   AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_14 ,AFIO_MODE_12);
    #define TKL_SEL_KEY13_AFIO   AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_15 ,AFIO_MODE_12);
    #define TKL_SEL_KEY14_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_0  ,AFIO_MODE_12);
    #define TKL_SEL_KEY15_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_1  ,AFIO_MODE_12);
    #define TKL_SEL_KEY16_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_2  ,AFIO_MODE_12);
    #define TKL_SEL_KEY17_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_3  ,AFIO_MODE_12);
    #define TKL_SEL_KEY18_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_4  ,AFIO_MODE_12);
    #define TKL_SEL_KEY19_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_5  ,AFIO_MODE_12);
    #define TKL_SEL_KEY20_AFIO   AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_1  ,AFIO_MODE_12);
    #define TKL_SEL_KEY21_AFIO   AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_2  ,AFIO_MODE_12);
    #define TKL_SEL_KEY22_AFIO   AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_3  ,AFIO_MODE_12);
    #define TKL_SEL_KEY23_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_6  ,AFIO_MODE_12);
    
    #define BOOT_IS_TK      KEY07_EN
    #define BOOT_AFIO       AFIO_PIN_9
    #define BOOT_GPIO       GPIO_PIN_9
    
    #define SWCLK_IS_TK     KEY10_EN
    #define SWCLK_AFIO      AFIO_PIN_12
    #define SWCLK_GPIO      GPIO_PIN_12
    
    #define SWDIO_IS_TK     KEY11_EN
    #define SWDIO_AFIO      AFIO_PIN_13
    #define SWDIO_GPIO      GPIO_PIN_13
    
  #ifdef  HXT_UART_DEBUG
    #define HXT_UART_BAUD_RATE                        115200
    #define HTCFG_UART_IPN                            UART0
    #define HTCFG_UART_TX_GPIO_ID                     GPIO_PB
    #define HTCFG_UART_TX_AFIO_PIN                    AFIO_PIN_7   //Connent Bs-eBridge RX pin
    #define HTCFG_UART_RX_GPIO_ID                     GPIO_PA
    #define HTCFG_UART_RX_GPIO_CLK                    PA
    #define HTCFG_UART_RX_GPIO_PORT                   HT_GPIOA
    #define HTCFG_UART_RX_GPIO_PIN                    GPIO_PIN_3
    #define HTCFG_UART_RX_AFIO_PIN                    AFIO_PIN_3
    #define HTCFG_UART_PORT                           HT_UART0
    #define HTCFG_UART_IRQn                           UART0_IRQn
    #define HTCFG_UART_IRQHandler                     UART0_IRQHandler
  #endif
#endif

#if defined(USE_HT32F54243_53)
    #define TKL_KeyAmount 28
    
    #define TKL_SEL_KEY0_AFIO    AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_4  ,AFIO_MODE_12);
    #define TKL_SEL_KEY1_AFIO    AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_5  ,AFIO_MODE_12);
    #define TKL_SEL_KEY2_AFIO    AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_6  ,AFIO_MODE_12);
    #define TKL_SEL_KEY3_AFIO    AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_7  ,AFIO_MODE_12);
    #define TKL_SEL_KEY4_AFIO    AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_15 ,AFIO_MODE_12);
    #define TKL_SEL_KEY5_AFIO    AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_0  ,AFIO_MODE_12);
    #define TKL_SEL_KEY6_AFIO    AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_10 ,AFIO_MODE_12);
    #define TKL_SEL_KEY7_AFIO    AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_11 ,AFIO_MODE_12);
    #define TKL_SEL_KEY8_AFIO    AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_8  ,AFIO_MODE_12);
    #define TKL_SEL_KEY9_AFIO    AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_9  ,AFIO_MODE_12);
    #define TKL_SEL_KEY10_AFIO   AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_10 ,AFIO_MODE_12);
    #define TKL_SEL_KEY11_AFIO   AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_11 ,AFIO_MODE_12);
    #define TKL_SEL_KEY12_AFIO   AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_12 ,AFIO_MODE_12);
    #define TKL_SEL_KEY13_AFIO   AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_13 ,AFIO_MODE_12);
    #define TKL_SEL_KEY14_AFIO   AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_14 ,AFIO_MODE_12);
    #define TKL_SEL_KEY15_AFIO   AFIO_GPxConfig(GPIO_PA ,AFIO_PIN_15 ,AFIO_MODE_12);
    #define TKL_SEL_KEY16_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_0  ,AFIO_MODE_12);
    #define TKL_SEL_KEY17_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_1  ,AFIO_MODE_12);
    #define TKL_SEL_KEY18_AFIO   AFIO_GPxConfig(GPIO_PD ,AFIO_PIN_1  ,AFIO_MODE_12);
    #define TKL_SEL_KEY19_AFIO   AFIO_GPxConfig(GPIO_PD ,AFIO_PIN_2  ,AFIO_MODE_12);
    #define TKL_SEL_KEY20_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_2  ,AFIO_MODE_12);
    #define TKL_SEL_KEY21_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_3  ,AFIO_MODE_12);
    #define TKL_SEL_KEY22_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_4  ,AFIO_MODE_12);
    #define TKL_SEL_KEY23_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_5  ,AFIO_MODE_12);
    #define TKL_SEL_KEY24_AFIO   AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_1  ,AFIO_MODE_12);
    #define TKL_SEL_KEY25_AFIO   AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_2  ,AFIO_MODE_12);
    #define TKL_SEL_KEY26_AFIO   AFIO_GPxConfig(GPIO_PC ,AFIO_PIN_3  ,AFIO_MODE_12);
    #define TKL_SEL_KEY27_AFIO   AFIO_GPxConfig(GPIO_PB ,AFIO_PIN_6  ,AFIO_MODE_12);
    
		#define HTCFG_ADC_IPN               ADC0
    #define HTCFG_ADC_IRQHandler        ADC_IRQHandler
				
    #define BOOT_IS_TK      KEY09_EN
    #define BOOT_AFIO       AFIO_PIN_9
    #define BOOT_GPIO       GPIO_PIN_9
    
    #define SWCLK_IS_TK     KEY12_EN
    #define SWCLK_AFIO      AFIO_PIN_12
    #define SWCLK_GPIO      GPIO_PIN_12
    
    #define SWDIO_IS_TK     KEY13_EN
    #define SWDIO_AFIO      AFIO_PIN_13
    #define SWDIO_GPIO      GPIO_PIN_13
    
  #ifdef  HXT_UART_DEBUG
    #define HXT_UART_BAUD_RATE                        115200
    #define HTCFG_UART_IPN                            UART0
    #define HTCFG_UART_TX_GPIO_ID                     GPIO_PB
    #define HTCFG_UART_TX_AFIO_PIN                    AFIO_PIN_9   //Connent Bs-eBridge RX pin
    #define HTCFG_UART_RX_GPIO_ID                     GPIO_PB
    #define HTCFG_UART_RX_GPIO_CLK                    PB
    #define HTCFG_UART_RX_GPIO_PORT                   HT_GPIOB
    #define HTCFG_UART_RX_GPIO_PIN                    GPIO_PIN_12
    #define HTCFG_UART_RX_AFIO_PIN                    AFIO_PIN_12
    #define HTCFG_UART_PORT                           HT_UART0
    #define HTCFG_UART_IRQn                           UART0_IRQn
    #define HTCFG_UART_IRQHandler                     UART0_IRQHandler
  #endif
#endif

    #define UartKeyAmount TKL_KeyAmount

#ifdef __cplusplus
}
#endif

#endif
