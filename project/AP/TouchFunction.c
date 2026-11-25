/*********************************************************************************************************//**
 * @file    TouchKey/TouchKey_6CH/main.c
 * @version $Rev:: 2            $
 * @date    $Date:: 2021-06-09 #$
 * @brief   Main program.
 *************************************************************************************************************/
#include "TouchFunction.h" //Li's change
#include "ht32_board_config.h"//Li's change
#include "FSM.h"//Li's change
#include "RTC.h"//Li's change
#include "GPIO.h"//Li's change
#include "I2C0.h"//Li's change
#include "I2C1.h"//Li's change
#include "bftm0.h"//Li's change
#include "USART1.h"//Li's change
#include "UART0.h"//Li's change
#include "LEDC.h"//Li's change
#include "eCooker.h"//Li's change
#include "CRC.h"//Li's change
#include "ht32_touchkey.h"
#include "ht32_touchkey_conf.h"
#include "ht32_touchkey_BSconf.h"
#include "batVoltage.h"
#include "ht32f5xxxx_pwrcu.h"


/* Global variables ----------------------------------------------------------------------------------------*/
#define USE_SLEEP_MODE          0
#define USE_DEEP_SLEEP1_MODE    1
#define USE_DEEP_SLEEP2_MODE    2

#define SEL_SLEEP_MODE          USE_DEEP_SLEEP1_MODE
#define DEEP_SLEEP_POLL_MAX_MS  30000u
#define LONG_PRESS_THRESHOLD_MS 800
#define Long_Press_Time    200

	volatile bool buttonPress = false;
//	int keyScan(void);
	void keyScan(void);
	u16 Key_Task(void);
	u32 KeyData;
	u8 touchkey, scanKey;
	
	
#ifdef  HXT_UART_DEBUG
  void  TKL_HXT_UART_TEST(void);
  void  TKL_USART_Configuration(void);
  u16   TKL_UART_DATA[(UartKeyAmount<<1)];
  u16   TKL_USART_TxBuf[UartKeyAmount*4+2];
  vu32  TKL_USART_TxIndex=0;
  vu32  TKL_USART_TxLen=0;
#endif


void TKL_InitConfig(void);
void SetTouchKeyAFIO(void);
void ExitSleepModePrepare(void);
void EnterSleepModePrepare(void);
void TKL_Configuration(void);
u8 Touch_Key_Main_Sleep_Function(u8 SleepMode);
u8 gIsEnterLowPowerMode = 0;
u8 SleepLoopIndex;
u8 TKL_USE_RAM[TKL_RAM_Len];

extern volatile u16 Enter_Sleep_Mode_IIC0_Cnt;
extern volatile u16 Enter_Sleep_Mode_IIC1_Cnt;

extern void  TKL_Set_EnableAllKey(u32 KeySel);

static void TouchKey_EnableConfiguredKeys(void)
{
  TKL_Set_EnableAllKey(
      (USE_KEY00 <<0)|(USE_KEY01<<1 )|(USE_KEY02<<2 )|(USE_KEY03<<3 )|
      (USE_KEY04 <<4)|(USE_KEY05<<5 )|(USE_KEY06<<6 )|(USE_KEY07<<7 )|
      (USE_KEY08 <<8)|(USE_KEY09<<9 )|(USE_KEY10<<10)|(USE_KEY11<<11)|
      (USE_KEY12<<12)|(USE_KEY13<<13)|(USE_KEY14<<14)|(USE_KEY15<<15)|
      (USE_KEY16<<16)|(USE_KEY17<<17)|(USE_KEY18<<18)|(USE_KEY19<<19)|
      (USE_KEY20<<20)|(USE_KEY21<<21)|(USE_KEY22<<22)|(USE_KEY23<<23)|
      (USE_KEY24<<24)|(USE_KEY25<<25)|(USE_KEY26<<26)|(USE_KEY27<<27)|
      (USE_KEY28<<28)|(USE_KEY29<<29)|(USE_KEY30<<30)|(USE_KEY31<<31)
    );
}

/*********************************************************************************************************//**
  * @brief  Touch Key Main Function
  * @retval None
  ***********************************************************************************************************/
void Touch_Key_Main_Function(){
  gIsEnterLowPowerMode = TKL_NOT_SLEEP;
	
 #if PowerSave == 0
		TKL_Set_Standby(TKL_StandbyTimeDefault);
 #endif
	
  //Open Touch IRQ
  NVIC_EnableIRQ(TKEY_IRQn);
  NVIC_SetPriority(TKEY_IRQn, 3);
  TKEY_IntConfig(TKEY_INT_TKRCOVE, ENABLE);
//  SetTouchKeyAFIO();
  
  //Touch Mail Function
  TKL_MainRoutine();

 #ifdef  HXT_UART_DEBUG
  TKL_HXT_UART_TEST();
 #endif
 
  SleepLoopIndex = 0;
}

/*********************************************************************************************************//**
  * @brief  Touch Key Sleep Main Function
  * @retval None
  ***********************************************************************************************************/
u8 Touch_Key_Main_Sleep_Function(u8 SleepMode){
  
  if(SleepLoopIndex > 3) SleepLoopIndex = 0;
  
  if(gIsEnterLowPowerMode == TKL_NOT_SLEEP){
    EnterSleepModePrepare();
		gIsEnterLowPowerMode = TKL_FIRST_SLEEP;
  }
  else{
    gIsEnterLowPowerMode = TKL_LowPowerCheckWake((SleepLoopIndex+3)%4,32);
  }

  if(gIsEnterLowPowerMode){
    gIsEnterLowPowerMode = TKL_LowSleepEnter(SleepLoopIndex);
  }
  else{
    SleepLoopIndex++;
		TKL_Set_Standby(TKL_StandbyTimeDefault);
    return 0;  //Exit Sleep Mode
  }

 #ifdef  HXT_UART_DEBUG
  if(gIsEnterLowPowerMode && SleepLoopIndex == 3){
    TKL_USART_TxIndex = 0;
    TKL_HXT_UART_TEST();
    while(USART_GetIntStatus(HTCFG_UART_PORT, USART_INT_TXDE));
  }
 #endif

  SleepLoopIndex++;
  
  if(gIsEnterLowPowerMode){
    return 1;
  }
  else{
		TKL_Set_Standby(TKL_StandbyTimeDefault);
    return 0;
  }
}


/*********************************************************************************************************//**
  * @brief  Enter & Exit Prepare
  * @retval Only used by USE_SLEEP_MODE
  ***********************************************************************************************************/

void EnterSleepModePrepare(void){
  NVIC_DisableIRQ(BFTM0_IRQn);
  NVIC_DisableIRQ(TKEY_IRQn);
  
  /* Disable peripheral clock                                                                              */
  { 
    CKCU_PeripClockConfig_TypeDef CKCUClock = {{ 0 }};
    CKCUClock.Bit.GPTM0 = 1;
    CKCUClock.Bit.GPTM1 = 1;
    CKCUClock.Bit.BFTM0 = 1;
    CKCUClock.Bit.AFIO  = 1;
    CKCU_PeripClockConfig(CKCUClock, DISABLE);
  }
}

void ExitSleepModePrepare(void){
  TKL_LowPowerExit();   //Exit Standby mode
  BFTM0_Configuration(); //ReWork BFTM IRQ
  
  /* Enable peripheral clock                                                                              */
  { 
    CKCU_PeripClockConfig_TypeDef CKCUClock = {{ 0 }};
    CKCUClock.Bit.GPTM0 = 1;
    CKCUClock.Bit.GPTM1 = 1;
    CKCUClock.Bit.BFTM0 = 1;
    CKCUClock.Bit.AFIO  = 1;
    CKCUClock.Bit.TKEY  = 1;
    CKCU_PeripClockConfig(CKCUClock, ENABLE);
  }

  /* Restore touch key AFIO mapping and interrupts after waking from sleep */
  SetTouchKeyAFIO();
  TouchKey_EnableConfiguredKeys();
  NVIC_EnableIRQ(BFTM0_IRQn);
  NVIC_EnableIRQ(TKEY_IRQn);
  TKEY_IntConfig(TKEY_INT_TKRCOVE, ENABLE);
}

/*********************************************************************************************************//**
  * @brief  Touch key Configuration.
  * @retval None
  ***********************************************************************************************************/
void TKL_Configuration(void){
  TKL_RAM_Setting(TKL_USE_RAM);
  TKL_InitConfig();
  SetTouchKeyAFIO();
}


/* Touch key scanning and event detection */
//int keyScan(void) {
//    Touch_Key_Main_Function();  // Main touch key loop
//    
//    if (!TKL_Is_Standby()) {  // Check sleep status
//        if (TKL_Is_KeyScanCycle()) {
//            KeyData = TKL_Get_AllKeyState();
//            
//            // Detect key presses and set corresponding events
//            if (KeyData & (1 << 12)) { touchkey = EVENT_KEY_PRESS_9; buttonPress = true; }
//            else if (KeyData & (1 << 13)) { touchkey = EVENT_BOOST_KEY; buttonPress = true; }
//            else if (KeyData & (1 << 0)) { touchkey = EVENT_PAYGO_KEY; buttonPress = true; }
//            else if (KeyData & (1 << 1)) { touchkey = EVENT_SYNC_KEY; buttonPress = true; }
//            else if (KeyData & (1 << 2)) { touchkey = EVENT_L_PLATE_SELECT_KEY; buttonPress = true; }
//            else if (KeyData & (1 << 3)) { touchkey = EVENT_KEY_PRESS_0; buttonPress = true; }
//            else if (KeyData & (1 << 4)) { touchkey = EVENT_KEY_PRESS_2; buttonPress = true; }
//            else if (KeyData & (1 << 5)) { touchkey = EVENT_KEY_PRESS_1; buttonPress = true; }
//            else if (KeyData & (1 << 6)) { touchkey = EVENT_KEY_PRESS_3; buttonPress = true; }
//            else if (KeyData & (1 << 7)) { touchkey = EVENT_KEY_PRESS_4; buttonPress = true; }
//            else if (KeyData & (1 << 8)) { touchkey = EVENT_KEY_PRESS_5; buttonPress = true; }
//            else if (KeyData & (1 << 9)) { touchkey = EVENT_KEY_PRESS_6; buttonPress = true; }
//            else if (KeyData & (1 << 10)) { touchkey = EVENT_KEY_PRESS_7; buttonPress = true; }
//            else if (KeyData & (1 << 11)) { touchkey = EVENT_KEY_PRESS_8; buttonPress = true; }
//            else if (KeyData & (1 << 14)) { touchkey = EVENT_R_PLATE_SELECT_KEY; buttonPress = true; }
//            else if (KeyData & (1 << 15)) { touchkey = EVENT_LOCK_KEY; buttonPress = true; }
//            else if (KeyData & (1 << 17)) { touchkey = EVENT_ONOFF_KEY; buttonPress = true; }
//            else if (KeyData & (1 << 16)) { touchkey = EVENT_TIMER_KEY; buttonPress = true; }
//            else { touchkey = 19; buttonPress = false; }
//        }
//    } else {  // Enter standby sleep mode if no touch activity
//        while (TKL_Is_Standby()) {
//            if (Touch_Key_Main_Sleep_Function(SEL_SLEEP_MODE)) {
//                switch (SEL_SLEEP_MODE) {
//                    case USE_SLEEP_MODE:
//                        PWRCU_Sleep(PWRCU_SLEEP_ENTRY_WFI);
//                        break;
//                    case USE_DEEP_SLEEP1_MODE:
//                        PWRCU_DeepSleep1(PWRCU_SLEEP_ENTRY_WFI);
//                        break;
//                    case USE_DEEP_SLEEP2_MODE:
//                        PWRCU_DeepSleep2(PWRCU_SLEEP_ENTRY_WFI);
//                        break;
//                }
//            }
//        }
//        ExitSleepModePrepare();
//    }
//    return touchkey;
//}

volatile u8 Start_Caculate_Time_Flag = 0;
volatile u8 Key_Finish = 0;
volatile u8 Key_Long_Finish = 0;
volatile u8 Key_L_Pan_3s_Finish = 0;
volatile u8 Key_L_Pan_3s_Value = 0;
volatile u16 Factory_L_Pan_Long_Accum_ms = 0;
static u8 Factory_L_Pan_Holding = 0;
static u32 Factory_L_Pan_Segment_Start = 0;
static u32 Factory_L_Pan_Last_Seen = 0;
static u32 Factory_L_Pan_Led_Last_Toggle = 0;
static u8 Factory_L_Pan_Led_On = 0;
volatile u8 Key_R_Pan_10s_Finish = 0;
volatile u8 Key_R_Pan_10s_Value = 0;
volatile u16 Factory_R_Pan_Long_Accum_ms = 0;
static u8 Factory_R_Pan_Holding = 0;
static u32 Factory_R_Pan_Segment_Start = 0;
static u32 Factory_R_Pan_Last_Seen = 0;
static u32 Factory_R_Pan_Led_Last_Toggle = 0;
static u8 Factory_R_Pan_Led_On = 0;
volatile u16 Start_Caculate_Time = 0;
volatile u32 KeyData_Value = 0, Key_Long_Value = 0, Key_Short_Value = 0, Key_Short_Data_Value = 0;

static void factory_l_pan_hold_led_update(u8 holding)
{
    u32 now;
    u8 led_on;

    if (!holding) {
        if (Factory_L_Pan_Led_On || Factory_L_Pan_Led_Last_Toggle) {
            LEDC_redLED(L_PLATE, false);
            Factory_L_Pan_Led_On = 0;
            Factory_L_Pan_Led_Last_Toggle = 0;
        }
        return;
    }

    now = (u32)bftm0_ct;
    if (Factory_L_Pan_Led_Last_Toggle == 0) {
        Factory_L_Pan_Led_Last_Toggle = now;
    }

    /* Phase from elapsed time so blink stays regular even when touch scan is irregular. */
    led_on = (u8)(1u - (((now - Factory_L_Pan_Led_Last_Toggle) / LED_BLINK_INTERVAL_MS) & 1u));
    if (led_on != Factory_L_Pan_Led_On) {
        Factory_L_Pan_Led_On = led_on;
        LEDC_redLED(L_PLATE, led_on ? true : false);
    }
}

void TouchFunction_UpdateFactoryLPanHold(u32 key_data)
{
    u32 now;
    u32 active_end;
    u32 total_ms;

    if (currentState != STATE_FACTORY || Key_L_Pan_3s_Finish) {
        factory_l_pan_hold_led_update(0);
        return;
    }

    now = (u32)bftm0_ct;

    if (key_data & L_PAN_KEY_BIT) {
        if (!Factory_L_Pan_Holding) {
            Factory_L_Pan_Holding = 1;
            Factory_L_Pan_Segment_Start = now;
        }
        Factory_L_Pan_Last_Seen = now;
    } else if (Factory_L_Pan_Holding) {
        if ((now - Factory_L_Pan_Last_Seen) > FACTORY_L_PAN_RELEASE_GRACE_MS) {
            Factory_L_Pan_Long_Accum_ms += (u16)(Factory_L_Pan_Last_Seen - Factory_L_Pan_Segment_Start);
            Factory_L_Pan_Holding = 0;
        }
    }

    if (!Factory_L_Pan_Holding) {
        factory_l_pan_hold_led_update(0);
        return;
    }

    active_end = (key_data & L_PAN_KEY_BIT) ? now : Factory_L_Pan_Last_Seen;
    total_ms = Factory_L_Pan_Long_Accum_ms + (active_end - Factory_L_Pan_Segment_Start);
    if (total_ms >= FACTORY_L_PAN_REBOOT_HOLD_MS) {
        Key_L_Pan_3s_Value = 1;
        Key_L_Pan_3s_Finish = 1;
        Factory_L_Pan_Long_Accum_ms = 0;
        Factory_L_Pan_Holding = 0;
    }

    factory_l_pan_hold_led_update(Factory_L_Pan_Holding);
}

void TouchFunction_ResetFactoryLPanHold(void)
{
    Key_L_Pan_3s_Finish = 0;
    Key_L_Pan_3s_Value = 0;
    Factory_L_Pan_Long_Accum_ms = 0;
    Factory_L_Pan_Holding = 0;
    Factory_L_Pan_Segment_Start = 0;
    Factory_L_Pan_Last_Seen = 0;
    factory_l_pan_hold_led_update(0);
}

void TouchFunction_FactoryLPanLedPoll(void)
{
    if (currentState != STATE_FACTORY || Key_L_Pan_3s_Finish || !Factory_L_Pan_Holding) {
        return;
    }
    factory_l_pan_hold_led_update(1);
}

static void factory_r_pan_hold_led_update(u8 holding)
{
    u32 now;
    u8 led_on;

    if (!holding) {
        if (Factory_R_Pan_Led_On || Factory_R_Pan_Led_Last_Toggle) {
            LEDC_redLED(R_PLATE, false);
            Factory_R_Pan_Led_On = 0;
            Factory_R_Pan_Led_Last_Toggle = 0;
        }
        return;
    }

    now = (u32)bftm0_ct;
    if (Factory_R_Pan_Led_Last_Toggle == 0) {
        Factory_R_Pan_Led_Last_Toggle = now;
    }

    led_on = (u8)(1u - (((now - Factory_R_Pan_Led_Last_Toggle) / LED_BLINK_INTERVAL_MS) & 1u));
    if (led_on != Factory_R_Pan_Led_On) {
        Factory_R_Pan_Led_On = led_on;
        LEDC_redLED(R_PLATE, led_on ? true : false);
    }
}

void TouchFunction_UpdateFactoryRPanHold(u32 key_data)
{
    u32 now;
    u32 active_end;
    u32 total_ms;

    if (currentState != STATE_FACTORY || Key_R_Pan_10s_Finish) {
        factory_r_pan_hold_led_update(0);
        return;
    }

    now = (u32)bftm0_ct;

    if (key_data & R_PAN_KEY_BIT) {
        if (!Factory_R_Pan_Holding) {
            Factory_R_Pan_Holding = 1;
            Factory_R_Pan_Segment_Start = now;
        }
        Factory_R_Pan_Last_Seen = now;
    } else if (Factory_R_Pan_Holding) {
        if ((now - Factory_R_Pan_Last_Seen) > FACTORY_L_PAN_RELEASE_GRACE_MS) {
            Factory_R_Pan_Long_Accum_ms += (u16)(Factory_R_Pan_Last_Seen - Factory_R_Pan_Segment_Start);
            Factory_R_Pan_Holding = 0;
        }
    }

    if (!Factory_R_Pan_Holding) {
        factory_r_pan_hold_led_update(0);
        return;
    }

    active_end = (key_data & R_PAN_KEY_BIT) ? now : Factory_R_Pan_Last_Seen;
    total_ms = Factory_R_Pan_Long_Accum_ms + (active_end - Factory_R_Pan_Segment_Start);
    if (total_ms >= FACTORY_R_PAN_RESET_HOLD_MS) {
        Key_R_Pan_10s_Value = 1;
        Key_R_Pan_10s_Finish = 1;
        Factory_R_Pan_Long_Accum_ms = 0;
        Factory_R_Pan_Holding = 0;
    }

    factory_r_pan_hold_led_update(Factory_R_Pan_Holding);
}

void TouchFunction_ResetFactoryRPanHold(void)
{
    Key_R_Pan_10s_Finish = 0;
    Key_R_Pan_10s_Value = 0;
    Factory_R_Pan_Long_Accum_ms = 0;
    Factory_R_Pan_Holding = 0;
    Factory_R_Pan_Segment_Start = 0;
    Factory_R_Pan_Last_Seen = 0;
    factory_r_pan_hold_led_update(0);
}

void TouchFunction_FactoryRPanLedPoll(void)
{
    if (currentState != STATE_FACTORY || Key_R_Pan_10s_Finish || !Factory_R_Pan_Holding) {
        return;
    }
    factory_r_pan_hold_led_update(1);
}

static u32 key_short_wait_mask(void)
{
    u32 mask = 0x3200Au;

    if (currentState == STATE_FACTORY) {
        mask |= L_PAN_KEY_BIT | R_PAN_KEY_BIT;
    }
    return mask;
}

void Key_Scan()
{
    if(!TKL_Is_Standby())
    {
        if(TKL_Is_KeyScanCycle())
        {
            KeyData = TKL_Get_AllKeyState();            
//            if( ( KeyData & 0x19801 ) == 0 )
            if( KeyData != 0 )
            {
                Start_Caculate_Time_Flag = 1;
                if( ( Start_Caculate_Time > 20 ) && ( Key_Finish == 0 ) && ( Key_Long_Finish == 0 ) )
                {
                    if( KeyData & key_short_wait_mask() )
                    {
                        KeyData_Value = KeyData;
                        Key_Short_Data_Value = 0;
                    }
                    else
                    {
                        Key_Short_Data_Value = KeyData;
                        KeyData_Value = 0;
                    }
//                    Key_Short_Value = KeyData;
                    Key_Finish = 1;
                }

                if( Start_Caculate_Time > LONG_PRESS_THRESHOLD_MS )
                {
                    if( Key_Long_Finish == 0 )
                    {
                        Key_Long_Value = KeyData;
                    }
                    /* Disable BFTM0 IRQ to protect read-modify-write (BFTM0 can preempt GPTM0) */
                    NVIC_DisableIRQ(BFTM0_IRQn);
                    Start_Caculate_Time -= Long_Press_Time;
                    NVIC_EnableIRQ(BFTM0_IRQn);
                    Key_Short_Value = 0;
                    Key_Long_Finish = 1;
                    Key_Finish = 0;
                }
            }
//            else if( ( KeyData & 0x19801 ) != 0 )
//            {
//                
//            }
            else
            {
                Start_Caculate_Time_Flag = 0;
                Start_Caculate_Time = 0;
                Key_Long_Finish = 0;
                if( Key_Finish == 1 )
                {
                    if (currentState == STATE_FACTORY &&
                        (KeyData_Value & (L_PAN_KEY_BIT | R_PAN_KEY_BIT))) {
                        Key_Finish = 0;
                        KeyData_Value = 0;
                    } else {
                        Key_Short_Value = KeyData_Value;
                        Key_Finish = 0;
                    }
                }
            }

            if (currentState == STATE_FACTORY) {
                TouchFunction_UpdateFactoryLPanHold(KeyData);
                TouchFunction_UpdateFactoryRPanHold(KeyData);
            }
        }
    }
		else
		{
			Deepsleep_Start_Flag = false;
			ExitSleepModePrepare();
		}
}

void DeepSleepMode_Configuration(void)
{
#if (ENABLE_MCU_LOW_POWER == 0)
	return;
#else
	CKCU_PeripClockConfig_TypeDef CKCUClock = {{0}};
	if( ( Enter_Sleep_Mode_IIC1_Cnt > IIC_Lost_Connect_Timeout ) && ( Enter_Sleep_Mode_IIC0_Cnt > IIC_Lost_Connect_Timeout ) )
	{	
		extern unsigned long epoch_time;
		extern unsigned long g_clbs_epoch;
		extern bool g_clbs_epoch_valid;
		u32 rtc_counter_before_sleep = RTC_GetCounter();
		unsigned long epoch_time_before_sleep = epoch_time;
		
		I2C_LowPowerBusRelease();
		GPIO_Sleep_Mode_Configuration();
		I2C_LowPowerWakePins_Configuration();
		CKCUClock.Bit.I2C0 		= 1;
		CKCUClock.Bit.I2C1 		= 1;
		CKCUClock.Bit.UART0 	= 1;
		CKCUClock.Bit.USART1 	= 1;
		CKCUClock.Bit.SCTM0 	= 1;
		CKCUClock.Bit.LEDC  	= 1;
		CKCUClock.Bit.ADC   	= 1;
		CKCUClock.Bit.PDMA    = 1;
		CKCUClock.Bit.CRC     = 1;
		CKCUClock.Bit.PA      = 1;
		CKCUClock.Bit.PD      = 1;
		CKCUClock.Bit.GPTM0   = 1;
  	CKCU_PeripClockConfig(CKCUClock, DISABLE);
		
		EXTI_WakeupEventIntConfig(ENABLE);
		g_i2c_deep_sleep_active = true;
		u32 sleep_enter_bftm0 = bftm0_ct;
		
  	u32 rtc_counter_after_wake = 0;
		bool counter_read_immediately = false;
		
		while( ( Enter_Sleep_Mode_IIC1_Cnt != 0 ) && ( Enter_Sleep_Mode_IIC0_Cnt != 0 ) )
		{
				switch (SEL_SLEEP_MODE)
				{
					case USE_SLEEP_MODE:
							PWRCU_Sleep(PWRCU_SLEEP_ENTRY_WFI);
							break;
					case USE_DEEP_SLEEP1_MODE:
							PWRCU_DeepSleep1(PWRCU_SLEEP_ENTRY_WFI);
							break;
					case USE_DEEP_SLEEP2_MODE:
							PWRCU_DeepSleep2Ex(PWRCU_SLEEP_ENTRY_WFI);
							break;
				}

				if (I2C_LowPowerWakePending())
				{
					I2C_LowPowerClearWakeFlags();
					volatile uint32_t rtc_delay = 100;
					while(rtc_delay--);
					rtc_counter_after_wake = RTC_GetCounter();
					counter_read_immediately = true;
					break;
				}

				if ((bftm0_ct - sleep_enter_bftm0) >= DEEP_SLEEP_POLL_MAX_MS)
				{
					I2C_LowPowerClearWakeFlags();
					volatile uint32_t rtc_delay = 100;
					while(rtc_delay--);
					rtc_counter_after_wake = RTC_GetCounter();
					counter_read_immediately = true;
					break;
				}
		}

		g_i2c_deep_sleep_active = false;
		
		if (!counter_read_immediately) {
			volatile uint32_t rtc_delay = 100;
			while(rtc_delay--);
			rtc_counter_after_wake = RTC_GetCounter();
		}
		
		u32 rtc_ticks_diff;
		
		if (rtc_counter_after_wake >= rtc_counter_before_sleep) {
			rtc_ticks_diff = rtc_counter_after_wake - rtc_counter_before_sleep;
		} else {
			rtc_ticks_diff = (0xFFFFFFFFUL - rtc_counter_before_sleep) + rtc_counter_after_wake + 1;
		}
		
		unsigned long sleep_seconds = rtc_ticks_diff / 32;
		
		if (sleep_seconds > 0) {
			unsigned long expected_epoch_time = epoch_time_before_sleep + sleep_seconds;
			
			if (epoch_time < expected_epoch_time) {
				unsigned long missing_seconds = expected_epoch_time - epoch_time;
				epoch_time += missing_seconds;
				
				if (g_clbs_epoch_valid) {
					g_clbs_epoch += missing_seconds;
				}
			}
		}
		CKCUClock.Bit.I2C0 		= 1;
		CKCUClock.Bit.I2C1 		= 1;
		CKCUClock.Bit.UART0 	= 1;
		CKCUClock.Bit.USART1 	= 1;
		CKCUClock.Bit.SCTM0 	= 1;
		CKCUClock.Bit.LEDC  	= 1;
		CKCUClock.Bit.ADC   	= 1;
		CKCUClock.Bit.PDMA    = 1;
		CKCUClock.Bit.CRC     = 1;
		CKCUClock.Bit.PA      = 1;
		CKCUClock.Bit.PB      = 1;
		CKCUClock.Bit.PC      = 1;
		CKCUClock.Bit.PD      = 1;
		CKCUClock.Bit.GPTM0   = 1;
		// BFTM1 and BKP should already be enabled (they weren't disabled)
		CKCU_PeripClockConfig(CKCUClock, ENABLE);
		I2C0_Configuration();
		I2C1_Configuration();
		I2C_LowPowerBusRestore();
		GPIO_Configuration();
		RETARGET_Configuration();
		UART0_Configuration();
		USART1_Configuration();
		LEDC_Configuration();
		RTC_Configuration();
		BFTM1_Configuration();  // Re-configure BFTM1 to ensure epoch timer continues
		eCooker_Configuration();
		ADC_Configuration();
		CRC_Configuration();
		
		GPIO_SetOutBits(HT_GPIOB, GPIO_PIN_13);
		GPIO_SetOutBits(HT_GPIOB, GPIO_PIN_14);
		ExitSleepModePrepare();
	}
#endif
}


/* Snapshot buffer in .bss to avoid 12 bytes stack in Key_Task (stack is only 512B on this MCU) */
static u32 key_task_snapshot[3];

u16 Key_Task()
{
    u8 l_pan_3s = Key_L_Pan_3s_Value;
    u8 r_pan_10s = Key_R_Pan_10s_Value;

    /* Snapshot to avoid multi-word tearing between Key_Scan (ISR) and main loop */
    key_task_snapshot[0] = Key_Short_Value;
    key_task_snapshot[1] = Key_Short_Data_Value;
    key_task_snapshot[2] = Key_Long_Value;
    Key_Short_Data_Value = 0;
    Key_Short_Value = 0;
    Key_Long_Value = 0;
    Key_L_Pan_3s_Value = 0;
    Key_R_Pan_10s_Value = 0;

    touchkey = 19;
    buttonPress = false;

    if (l_pan_3s) {
        touchkey = EVENT_L_PLATE_SELECT_KEY_LONG;
        buttonPress = true;
        return touchkey;
    }

    if (r_pan_10s) {
        touchkey = EVENT_R_PLATE_SELECT_KEY_LONG;
        buttonPress = true;
        return touchkey;
    }

    if (key_task_snapshot[0] & (1 << 15) || key_task_snapshot[1] & (1 << 15)) { touchkey = EVENT_LOCK_KEY; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 12) || key_task_snapshot[1] & (1 << 12)) { touchkey = EVENT_KEY_PRESS_9; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 0) || key_task_snapshot[1] & (1 << 0)) { touchkey = EVENT_PAYGO_KEY; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 1) || key_task_snapshot[1] & (1 << 1)) { touchkey = EVENT_SYNC_KEY; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 2) || key_task_snapshot[1] & (1 << 2)) {
        if (currentState != STATE_FACTORY) {
            touchkey = EVENT_L_PLATE_SELECT_KEY;
            buttonPress = true;
        }
    }
    else if (key_task_snapshot[0] & (1 << 3) || key_task_snapshot[1] & (1 << 3)) { touchkey = EVENT_KEY_PRESS_0; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 4) || key_task_snapshot[1] & (1 << 4)) { touchkey = EVENT_KEY_PRESS_2; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 5) || key_task_snapshot[1] & (1 << 5)) { touchkey = EVENT_KEY_PRESS_1; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 6) || key_task_snapshot[1] & (1 << 6)) { touchkey = EVENT_KEY_PRESS_3; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 7) || key_task_snapshot[1] & (1 << 7)) { touchkey = EVENT_KEY_PRESS_4; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 8) || key_task_snapshot[1] & (1 << 8)) { touchkey = EVENT_KEY_PRESS_5; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 9) || key_task_snapshot[1] & (1 << 9)) { touchkey = EVENT_KEY_PRESS_6; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 10) || key_task_snapshot[1] & (1 << 10)) { touchkey = EVENT_KEY_PRESS_7; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 11) || key_task_snapshot[1] & (1 << 11)) { touchkey = EVENT_KEY_PRESS_8; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 14) || key_task_snapshot[1] & (1 << 14)) {
        if (currentState != STATE_FACTORY) {
            touchkey = EVENT_R_PLATE_SELECT_KEY;
            buttonPress = true;
        }
    }
    else if (key_task_snapshot[0] & (1 << 13) || key_task_snapshot[1] & (1 << 13)) { touchkey = EVENT_BOOST_KEY; buttonPress = true; }
    else if (key_task_snapshot[0] & (1 << 17) || key_task_snapshot[1] & (1 << 17))
    {
        touchkey = EVENT_ONOFF_KEY;
        buttonPress = true;
    }
    else if (key_task_snapshot[0] & (1 << 16) || key_task_snapshot[1] & (1 << 16)) { touchkey = EVENT_TIMER_KEY; buttonPress = true; }

    if (key_task_snapshot[2] & (1 << 1))
    {
        touchkey = EVENT_SYNC_KEY_LONG;
        buttonPress = true;
    }
    else if (key_task_snapshot[2] & (1 << 16))
    {
        touchkey = EVENT_TIMER_KEY_LONG;
        buttonPress = true;
    }
    else if (key_task_snapshot[2] & (1 << 15)) { touchkey = EVENT_LOCK_KEY_LONG; buttonPress = true; }
    else if ( (key_task_snapshot[2] & (1 << 3)) && (key_task_snapshot[2] & (1 << 13)) )
    {
        touchkey = EVENT_KEY_PRESS_0_B;
        buttonPress = true;
    }

    return touchkey;
}



/*int keyScan(void) {
    static u32 lockPressTime = 0;         
    static u8 lockLongPressDetected = 0;  
    static u8 lockIsPressed = 0;        

		static u32 boostPressTime = 0;         
    static u8 boostLongPressDetected = 0;  
    static u8 boostIsPressed = 0;        
	
	
		static u32 syncPressTime = 0;         
    static u8 syncLongPressDetected = 0;  
    static u8 syncIsPressed = 0;        
		
    Touch_Key_Main_Function(); 

    if (!TKL_Is_Standby()) { 
        if (TKL_Is_KeyScanCycle()) {
            KeyData = TKL_Get_AllKeyState();

				
            if (KeyData & (1 << 15)) {  
                if (!lockIsPressed) {  
                    lockIsPressed = 1;
                    lockPressTime = bftm0_ct; 
                    lockLongPressDetected = 0; 
                }

                if (!lockLongPressDetected &&
                    (bftm0_ct - lockPressTime) >= LONG_PRESS_THRESHOLD_MS) {
                    lockLongPressDetected = 1;
//                    printf("Long press!\n");  
                    touchkey = EVENT_LOCK_KEY;
										buttonPress = true; 
                }
										
            } else if (lockIsPressed) {  
                if (lockIsPressed) {  
                    lockIsPressed = 0; 
                    if (!lockLongPressDetected) {
                   //     printf("Short press!\n");
                        touchkey = EVENT_LOCK_KEY;
												buttonPress = true;
                    }
                }
            }
						
						
						 else if (KeyData & (1 << 13)) {  
                if (!boostIsPressed) {  
                    boostIsPressed = 1;
                    boostPressTime = bftm0_ct; 
                    boostLongPressDetected = 0; 
                }

                if (!boostLongPressDetected &&
                    (bftm0_ct - boostPressTime) >= LONG_PRESS_THRESHOLD_MS) {
                    boostLongPressDetected = 1;
//                    printf("Long press!\n");  
                    touchkey = EVENT_BOOST_KEY;
										buttonPress = true; 
                }
										
            } else if (boostIsPressed) {  
                if (boostIsPressed) {  
                    boostIsPressed = 0; 
                    if (!boostLongPressDetected) {
//                        printf("Short press!\n");
                        touchkey = EVENT_BOOST_KEY;
												buttonPress = true; 
                    }
                }
            }
						
						
						else if (KeyData & (1 << 1)) {  
                if (!syncIsPressed) {  
                    syncIsPressed = 1;
                    syncPressTime = bftm0_ct; 
                    syncLongPressDetected = 0; 
                }

                if (!syncLongPressDetected &&
                    (bftm0_ct - syncPressTime) >= LONG_PRESS_THRESHOLD_MS) {
                    syncLongPressDetected = 1;
//                    printf("Long press!\n");  
                    touchkey = EVENT_SYNC_KEY;
										buttonPress = true; 
                }
										
            } else if (syncIsPressed) {  
                if (syncIsPressed) {  
                    syncIsPressed = 0; 
                    if (!syncLongPressDetected) {
//                        printf("Short press!\n");
                        touchkey = EVENT_SYNC_KEY;
												buttonPress = true; 
                    }
                }
            }
						
						else if (KeyData & (1 << 15)) { touchkey = EVENT_LOCK_KEY; buttonPress = true; }
            else if (KeyData & (1 << 12)) { touchkey = EVENT_KEY_PRESS_9; buttonPress = true; }
            else if (KeyData & (1 << 0)) { touchkey = EVENT_PAYGO_KEY; buttonPress = true; }
            else if (KeyData & (1 << 1)) { touchkey = EVENT_SYNC_KEY; buttonPress = true; }
            else if (KeyData & (1 << 2)) { touchkey = EVENT_L_PLATE_SELECT_KEY; buttonPress = true; }
            else if (KeyData & (1 << 3)) { touchkey = EVENT_KEY_PRESS_0; buttonPress = true; }
            else if (KeyData & (1 << 4)) { touchkey = EVENT_KEY_PRESS_2; buttonPress = true; }
            else if (KeyData & (1 << 5)) { touchkey = EVENT_KEY_PRESS_1; buttonPress = true; }
            else if (KeyData & (1 << 6)) { touchkey = EVENT_KEY_PRESS_3; buttonPress = true; }
            else if (KeyData & (1 << 7)) { touchkey = EVENT_KEY_PRESS_4; buttonPress = true; }
            else if (KeyData & (1 << 8)) { touchkey = EVENT_KEY_PRESS_5; buttonPress = true; }
            else if (KeyData & (1 << 9)) { touchkey = EVENT_KEY_PRESS_6; buttonPress = true; }
            else if (KeyData & (1 << 10)) { touchkey = EVENT_KEY_PRESS_7; buttonPress = true; }
            else if (KeyData & (1 << 11)) { touchkey = EVENT_KEY_PRESS_8; buttonPress = true; }
            else if (KeyData & (1 << 14)) { touchkey = EVENT_R_PLATE_SELECT_KEY; buttonPress = true; }
            else if (KeyData & (1 << 17)) { touchkey = EVENT_ONOFF_KEY; buttonPress = true; }
            else if (KeyData & (1 << 16)) { touchkey = EVENT_TIMER_KEY; buttonPress = true; }
            else { touchkey = 19; buttonPress = false; }
        }
    } else {  
        while (TKL_Is_Standby()) {
            if (Touch_Key_Main_Sleep_Function(SEL_SLEEP_MODE)) {
                switch (SEL_SLEEP_MODE) {
                    case USE_SLEEP_MODE:
                        PWRCU_Sleep(PWRCU_SLEEP_ENTRY_WFI);
                        break;
                    case USE_DEEP_SLEEP1_MODE:
                        PWRCU_DeepSleep1(PWRCU_SLEEP_ENTRY_WFI);
                        break;
                    case USE_DEEP_SLEEP2_MODE:
                        PWRCU_DeepSleep2(PWRCU_SLEEP_ENTRY_WFI);
                        break;
                }
            }
        }
        ExitSleepModePrepare();
    }
    return touchkey;
		
}

*/


/* Private macro -------------------------------------------------------------------------------------------*/
/* Global variables ----------------------------------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------------------------------------*/
/* Global functions ----------------------------------------------------------------------------------------*/
/*********************************************************************************************************//**
  * @brief  FUNCTIONDESCRIPTION.
  * @param  P1DESCRIPTION
  *         @arg ARGDESCRIPTION
  *         @arg ARGDESCRIPTION
  * @param  P2DESCRIPTION
  * @retval R1DESCRIPTION
  ***********************************************************************************************************/


const char DefKeyThr[]={
    Key00Threshold,  Key01Threshold,  Key02Threshold,  Key03Threshold,
    Key04Threshold,  Key05Threshold,  Key06Threshold,  Key07Threshold,
    Key08Threshold,  Key09Threshold,  Key10Threshold,  Key11Threshold,
    Key12Threshold,  Key13Threshold,  Key14Threshold,  Key15Threshold,
    Key16Threshold,  Key17Threshold,  Key18Threshold,  Key19Threshold,
    Key20Threshold,  Key21Threshold,  Key22Threshold,  Key23Threshold,
    Key24Threshold,  Key25Threshold,  Key26Threshold,  Key27Threshold,
    Key28Threshold,  Key29Threshold,  Key30Threshold,  Key31Threshold,
  };

void TKL_InitConfig(void){
		u8 i=0;
  {
    TKL_Config_TypeDef TKL_Config = {{0}};
    //touch freq0

    TKL_Config.TKOSC[0]               = TKOSC_3MHZ;
    //touch freq1
    TKL_Config.TKOSC[1]               = TKOSC_11MHZ;

    //in sleep per N sec check once balance Status
    TKL_Config.SleepBalanceTime       = TKL_SleepBalanceTime;

    //in sleep per N sec caliration once REF
    TKL_Config.SleepCalTime           = TKL_SleepCalTime;

    //Standby Default value
    TKL_Config.StandbyTimeDefault     = TKL_StandbyTimeDefault;

    //Power On check ReBalance time
    TKL_Config.PonReBalanceTime       = TKL_PonReBalanceTime;

    //Normal check ReBalance time
    TKL_Config.ReBalanceTime          = TKL_ReBalanceTime;

    //Power on count timer
    TKL_Config.PonRunTimeNSec         = TKL_PonRunTimeNSec;

    //Normal REF/OSC counter
    TKL_Config.SampleTime             = TKL_SAMPLE_LEN;

    //Sleep REF/OSC counter
    TKL_Config.SleepSampleTime        = TKL_SLEEP_SAMPLE_LEN;

    //Rebalance check ENV low limit value
    TKL_Config.LowUnBalanceOffset     = TKL_SAMPLE_LEN - 32;

    //Rebalance check ENV high limit value
    TKL_Config.HighUnBalanceOffset    = TKL_SAMPLE_LEN + 32;

    //Rebalance RCC max value
    TKL_Config.UpUnReasonableCAP      = TKL_UpUnReasonableCAP;

    //Ref Cal time
    TKL_Config.RefCalTime             = TKL_RefCalTime;

    //Check Noise Thr
    TKL_Config.NoiseLevel             = TKL_NoiseLevel;

    //Debounce in output key status
    TKL_Config.KeyDebounce            = TKL_KeyDebounce;

    //key holding time ,if time out will reset key status to non-press
    TKL_Config.MaxOnHoldTime          = TKL_MaxOnHoldTime/32;

    //Touch Librery used key amount
    TKL_Config.KeyAmount              = TKL_KeyAmount;
		
    //Touch Librery used FastKey
    TKL_Config.FastKey                = TKL_FastKey;
		
    //Touch Librery used Moving Calibration
    TKL_Config.MovingCalibration      = TKL_MovingCalibration;
		
    //Touch Librery Scan Cycle
    TKL_Config.ScanCycle              = TKL_TouchScanCycle;
    
    
    for(i=0;i<TKL_KeyAmount;i++){
      TKL_Config.KeyThr[i] = DefKeyThr[i];
    }
    
    TKL_Parame(TKL_Config);
  }
  
  TouchKey_EnableConfiguredKeys();
}

extern u8 IsUseKey(u8);
void SetTouchKeyAFIO(void){
  /*--------------------------------------------------------------------------------------------------------*/
  /* set tkey clock                                                                                         */
  /*--------------------------------------------------------------------------------------------------------*/
  {
    CKCU_PeripClockConfig_TypeDef CKCUClock = {{ 0 }};
    CKCUClock.Bit.TKEY = 1;
    CKCUClock.Bit.AFIO = 1;
    CKCU_PeripClockConfig(CKCUClock, ENABLE);
  }
  
  #if TKL_KeyAmount > 0 
    #if USE_KEY00 
      TKL_SEL_KEY0_AFIO; 
    #endif  
  #endif
  #if TKL_KeyAmount > 1 
    #if USE_KEY01 
      TKL_SEL_KEY1_AFIO; 
    #endif  
  #endif
  #if TKL_KeyAmount > 2 
    #if USE_KEY02 
      TKL_SEL_KEY2_AFIO; 
    #endif  
  #endif
  #if TKL_KeyAmount > 3 
    #if USE_KEY03 
      TKL_SEL_KEY3_AFIO; 
    #endif  
  #endif
  #if TKL_KeyAmount > 4 
    #if USE_KEY04 
      TKL_SEL_KEY4_AFIO; 
    #endif  
  #endif
  #if TKL_KeyAmount > 5 
    #if USE_KEY05 
      TKL_SEL_KEY5_AFIO; 
    #endif  
  #endif
  #if TKL_KeyAmount > 6 
    #if USE_KEY06 
      TKL_SEL_KEY6_AFIO; 
    #endif  
  #endif
  #if TKL_KeyAmount > 7 
    #if USE_KEY07 
      TKL_SEL_KEY7_AFIO; 
    #endif  
  #endif
  #if TKL_KeyAmount > 8 
    #if USE_KEY08 
      TKL_SEL_KEY8_AFIO; 
    #endif  
  #endif
  #if TKL_KeyAmount > 9 
    #if USE_KEY09 
      TKL_SEL_KEY9_AFIO; 
    #endif  
  #endif
  #if TKL_KeyAmount > 10
    #if USE_KEY10
      TKL_SEL_KEY10_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 11
    #if USE_KEY11
      TKL_SEL_KEY11_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 12
    #if USE_KEY12
      TKL_SEL_KEY12_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 13
    #if USE_KEY13
      TKL_SEL_KEY13_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 14
    #if USE_KEY14
      TKL_SEL_KEY14_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 15
    #if USE_KEY15
      TKL_SEL_KEY15_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 16
    #if USE_KEY16
      TKL_SEL_KEY16_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 17
    #if USE_KEY17
      TKL_SEL_KEY17_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 18
    #if USE_KEY18
      TKL_SEL_KEY18_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 19
    #if USE_KEY19
      TKL_SEL_KEY19_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 20
    #if USE_KEY20
      TKL_SEL_KEY20_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 21
    #if USE_KEY21
      TKL_SEL_KEY21_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 22
    #if USE_KEY22
      TKL_SEL_KEY22_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 23
    #if USE_KEY23
      TKL_SEL_KEY23_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 24
    #if USE_KEY24
      TKL_SEL_KEY24_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 25
    #if USE_KEY25
      TKL_SEL_KEY25_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 26
    #if USE_KEY26
      TKL_SEL_KEY26_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 27
    #if USE_KEY27
      TKL_SEL_KEY27_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 28
    #if USE_KEY28
      TKL_SEL_KEY28_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 29
    #if USE_KEY29
      TKL_SEL_KEY29_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 30
    #if USE_KEY30
      TKL_SEL_KEY30_AFIO;
    #endif  
  #endif
  #if TKL_KeyAmount > 31
    #if USE_KEY31
      TKL_SEL_KEY31_AFIO;
    #endif  
  #endif
}


#ifdef  HXT_UART_DEBUG


u8 DelaySend,HXT_CNT;
void TKL_HXT_UART_TEST(void){
  u8 i;

  if(gIsEnterLowPowerMode){
      DelaySend = 100;
  }
  else if(TKL_Is_Time(kT2mS)){
    if(TKL_USART_TxIndex==0 && TKL_Is_Active()){
      DelaySend++;
    }
  }
	//if(TKL_Is_KeyScanCycle())
  if(DelaySend > 0){
    DelaySend = 0;
    HXT_CNT++;
    if(HXT_CNT>7) HXT_CNT=0;
    if(HXT_CNT<3) TKL_USART_TxBuf[0]=0x04;
    else          TKL_USART_TxBuf[0]=0x02;

    for(i=0;i<UartKeyAmount;i++){
      if(TKL_USART_TxBuf[0] == 0x04){

       #ifdef HXT_READ_REF_ENV
        TKL_UART_DATA[i]=TKL_Get_KeyRef(i);
       #else
        TKL_UART_DATA[i]=TKL_Get_KeyRCCValue(i,0);
       #endif

      }
      else{
       #ifdef HXT_READ_REF_ENV
        TKL_UART_DATA[i]=TKL_Get_KeyEnv(i);
       #else
        TKL_UART_DATA[i]=TKL_Get_KeyRCCValue(i,1);
       #endif
      }

      TKL_USART_TxBuf[(i<<2)+1]=(((TKL_UART_DATA[i]%256)/16)+0x30);
      TKL_USART_TxBuf[(i<<2)+2]=(((TKL_UART_DATA[i]%256)%16)+0x30);
      TKL_USART_TxBuf[(i<<2)+3]=((TKL_UART_DATA[i]/4096)+0x30);
      TKL_USART_TxBuf[(i<<2)+4]=(((TKL_UART_DATA[i]%4096)/256)+0x30);
    }

    TKL_USART_TxBuf[UartKeyAmount*4+1]=0x03;
    TKL_USART_TxLen=TKL_USART_TxIndex=UartKeyAmount*4+2;

    //USART_IntConfig(RETARGET_USART_PORT,USART_INT_TXC, ENABLE);
    USART_IntConfig(HTCFG_UART_PORT, USART_INT_TXDE | USART_INT_TXC, ENABLE);
  }
}

void TKL_USART_Configuration(void)
{
  { /* Enable peripheral clock of AFIO, UxART                                                               */
    CKCU_PeripClockConfig_TypeDef CKCUClock = {{0}};
    CKCUClock.Bit.AFIO                   = 1;
    CKCUClock.Bit.HTCFG_UART_RX_GPIO_CLK = 1;
    CKCUClock.Bit.HTCFG_UART_IPN         = 1;
    CKCU_PeripClockConfig(CKCUClock, ENABLE);
  }

  /* Turn on UxART Rx internal pull up resistor to prevent unknow state                                     */
  GPIO_PullResistorConfig(HTCFG_UART_RX_GPIO_PORT, HTCFG_UART_RX_GPIO_PIN, GPIO_PR_UP);

  /* Config AFIO mode as UxART function.                                                                    */
  AFIO_GPxConfig(HTCFG_UART_TX_GPIO_ID, HTCFG_UART_TX_AFIO_PIN, AFIO_FUN_USART_UART);
  //AFIO_GPxConfig(HTCFG_UART_RX_GPIO_ID, HTCFG_UART_RX_AFIO_PIN, AFIO_FUN_USART_UART);

  {
    /* UxART configured as follow:
          - BaudRate = HXT_UART_BAUD_RATE
          - Word Length = 8 Bits
          - One Stop Bit
          - None parity bit
    */

    /* !!! NOTICE !!!
       Notice that the local variable (structure) did not have an initial value.
       Please confirm that there are no missing members in the parameter settings below in this function.
    */
    USART_InitTypeDef USART_InitStructure = {0};
    USART_InitStructure.USART_BaudRate = HXT_UART_BAUD_RATE;
    USART_InitStructure.USART_WordLength = USART_WORDLENGTH_8B;
    USART_InitStructure.USART_StopBits = USART_STOPBITS_1;
    USART_InitStructure.USART_Parity = USART_PARITY_NO;
    USART_InitStructure.USART_Mode = USART_MODE_NORMAL;
    USART_Init(HTCFG_UART_PORT, &USART_InitStructure);
  }

  /* Enable UxART interrupt of NVIC                                                                         */
  NVIC_EnableIRQ(HTCFG_UART_IRQn);

  /* Enable UxART Rx interrupt                                                                              */
  //USART_IntConfig(HTCFG_UART_PORT, USART_INT_RXDR, ENABLE);

  /* Enable UxART Tx and Rx function                                                                        */
  USART_TxCmd(HTCFG_UART_PORT, ENABLE);
  //USART_RxCmd(HTCFG_UART_PORT, ENABLE);
}

#endif

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */
