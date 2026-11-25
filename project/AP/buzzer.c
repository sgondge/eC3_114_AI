#include "ht32.h"
#include "GPIO.h"
#include "BFTM0.h"
#include "buzzer.h"
#include "utils.h"

#define SCTM0_PRESCALE 4
#define PWM_DUTY_CYCLE  40

#define BEE_TIME_MS(t, f)               (f * t / 1000)

static u8 gBee_Time;
static u16 gBee_Active;
static u16 gBee_InActive;
static vu32 gBee_Count;


u32 Buzzer_IsFinish(void)
{	//functionStatus = 80;
  return (gBee_Time ? FALSE : TRUE);
}

void Buzzer_Init(u32 uFrequency)
{	//functionStatus = 81;
  u32 uReload;

  { /* Enable peripheral clock                                                                              */
    CKCU_PeripClockConfig_TypeDef CKCUClock = {{ 0 }};
    CKCUClock.Bit.AFIO = 1;
    CKCUClock.Bit.SCTM0 = 1;
    CKCU_PeripClockConfig(CKCUClock, ENABLE);
  }

  /* Configure AFIO mode as TM function                                                                     */
  AFIO_GPxConfig(GPIO_PD, AFIO_PIN_4, AFIO_FUN_PWM);

  uReload = (LIBCFG_MAX_SPEED / SCTM0_PRESCALE / uFrequency) - 1;

  { /* Time base configuration                                                                              */

    /* !!! NOTICE !!!
       Notice that the local variable (structure) did not have an initial value.
       Please confirm that there are no missing members in the parameter settings below in this function.
    */
    TM_TimeBaseInitTypeDef TimeBaseInit;

    TimeBaseInit.Prescaler =  SCTM0_PRESCALE - 1;
    TimeBaseInit.CounterReload = uReload;
    TimeBaseInit.RepetitionCounter = 0;
    TimeBaseInit.CounterMode = TM_CNT_MODE_UP;
    TimeBaseInit.PSCReloadTime = TM_PSC_RLD_IMMEDIATE;
    TM_TimeBaseInit(HT_SCTM0, &TimeBaseInit);

    /* Clear Update Event Interrupt flag since the "TM_TimeBaseInit()" writes the UEV1G bit                 */
    TM_ClearFlag(HT_SCTM0, TM_FLAG_UEV);
  }
{ /* Channel n output configuration                                                                       */

    /* !!! NOTICE !!!
       Notice that the local variable (structure) did not have an initial value.
       Please confirm that there are no missing members in the parameter settings below in this function.
    */
    TM_OutputInitTypeDef OutInit;

    OutInit.Channel = TM_CH_0;
    OutInit.OutputMode = TM_OM_PWM2;
    OutInit.Control = TM_CHCTL_DISABLE;
    OutInit.ControlN = TM_CHCTL_DISABLE;
    OutInit.Polarity = TM_CHP_NONINVERTED;
    OutInit.PolarityN = TM_CHP_NONINVERTED;
    OutInit.IdleState = MCTM_OIS_LOW;
    OutInit.IdleStateN = MCTM_OIS_HIGH;
    OutInit.Compare = ((uReload + 1) * (100 - PWM_DUTY_CYCLE)) / 100;
    OutInit.AsymmetricCompare = 0;
    TM_OutputInit(HT_SCTM0, &OutInit);
  }

  /* Enable TM interrupt                                                                                    */
  NVIC_EnableIRQ(SCTM0_IRQn);	
}

void Buzzer_Start(u8 uBeeTimes, u16 uFrequency, u16 uActive_ms, u16 uInActive_ms)
{
	//functionStatus = 82;
	u32 uReload = (LIBCFG_MAX_SPEED / SCTM0_PRESCALE / uFrequency) - 1;
  u32 uCompare =  ((uReload + 1) * (100 - PWM_DUTY_CYCLE)) / 100;

  gBee_Time = uBeeTimes;
  gBee_Active = BEE_TIME_MS(uActive_ms, uFrequency);
  gBee_InActive = BEE_TIME_MS(uInActive_ms, uFrequency);
  gBee_Count = gBee_Active + gBee_InActive;

  TM_SetCounterReload(HT_SCTM0, uReload);
  TM_SetCaptureCompare(HT_SCTM0, TM_CH_0, uCompare);

  TM_IntConfig(HT_SCTM0, TM_INT_UEV, ENABLE);
  TM_ChannelConfig(HT_SCTM0, TM_CH_0, TM_CHCTL_ENABLE);
  TM_Cmd(HT_SCTM0, ENABLE);
}

#if 0
void buzz (uint32_t mS) {
	uint32_t now;if 0
	//set GPIO Port D bit 4 to gpio output funtion
	AFIO_GPxConfig(GPIO_PD, AFIO_PIN_4, AFIO_FUN_GPIO);
	GPIO_DirectionConfig(HT_GPIOD, GPIO_PIN_4, GPIO_DIR_OUT);
	now = bftm0_ct;
	while (bftm0_ct < (now + mS)){
		if (bftm0_ct & 1) {
			GPIO_WriteOutBits (HT_GPIOD, 1<<4, SET);
		}
		else {
			GPIO_WriteOutBits (HT_GPIOD, 1<<4, RESET);
		}
	}		
	//reset pin to USART TX function
	AFIO_GPxConfig(GPIO_PD, AFIO_PIN_4, AFIO_FUN_USART_UART);
}

#else
void buzz (uint32_t mS) {
		//functionStatus = 83;
  Buzzer_Init(0);
  Buzzer_Start(mS, 2000, 50, 50);
}
#endif

void SCTM0_IRQHandler(void)
{	//functionStatus = 84;
  TM_ClearFlag(HT_SCTM0, TM_INT_UEV);

  if (gBee_Count)
  {
    gBee_Count--;
    if (gBee_Count == gBee_InActive)
    {
      TM_ChannelConfig(HT_SCTM0, TM_CH_0, TM_CHCTL_DISABLE);
			// reset the pin to the UART TX function
    }

    if (gBee_Count == 0)
    {
      gBee_Count = gBee_Active + gBee_InActive;
      TM_ChannelConfig(HT_SCTM0, TM_CH_0, TM_CHCTL_ENABLE);

      if (gBee_Time)
        gBee_Time--;

      if (gBee_Time == 0)
      {
        TM_Cmd(HT_SCTM0, DISABLE);
				AFIO_GPxConfig(GPIO_PD, AFIO_PIN_4, AFIO_FUN_USART_UART);

      }
    }
  }
}

void Buzzer_LongBeep(uint32_t uFrequency, uint16_t uActive_ms)
{	//functionStatus = 85;
    Buzzer_Init(uFrequency);
    Buzzer_Start(1, uFrequency, uActive_ms, 0);
}
