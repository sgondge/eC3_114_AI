#include "GPIO.h"
#include <stdint.h>
#include "utils.h"

#define MY_SEG_PU_PD GPIO_PR_DOWN
#define MY_COM_PU_PD GPIO_PR_DOWN


void GPIO_ModeConfig(u32 GPIO_Px, u32 GPIO_PIN)
{
		//functionStatus = 178;
  vu32* pGPxCFGR = ((vu32*)&HT_AFIO->GPACFGR[0]) + GPIO_Px * 2;
  u32 uRegTmp;

  if (GPIO_PIN > 7)
  {
    GPIO_PIN -= 8;
    pGPxCFGR++;
  }
  uRegTmp = *pGPxCFGR;
  uRegTmp &= ~(0xF << (GPIO_PIN * 4));
  uRegTmp |= (1 << (GPIO_PIN * 4));
  *pGPxCFGR = uRegTmp;
}


//-----------------------------------------------------------------------------


void GPIO_Configuration(void) {
	//functionStatus = 179;
//  volatile uint32_t * pGPACFGR = ((volatile uint32_t*)&HT_AFIO->GPACFGR[0]); //point to Port A config
  u32 uRegTmp;
  
			
  CKCU_PeripClockConfig_TypeDef CKCUClock = {{ 0 }};
  CKCUClock.Bit.PA    = 1;
  CKCUClock.Bit.PB    = 1;
  CKCUClock.Bit.PC    = 1;
	CKCUClock.Bit.PD    = 1;
  CKCUClock.Bit.AFIO  = 1;
  CKCU_PeripClockConfig(CKCUClock, ENABLE);
	
	// PORT A

// for LED tests
  AFIO_GPxConfig         (GPIO_PA, 0, AFIO_FUN_GPIO);
  AFIO_GPxConfig         (GPIO_PA, 1, AFIO_FUN_GPIO);
  AFIO_GPxConfig         (GPIO_PA, 2, AFIO_FUN_GPIO);
  AFIO_GPxConfig         (GPIO_PA, 3, AFIO_FUN_GPIO);
  AFIO_GPxConfig         (GPIO_PA, 4, AFIO_FUN_GPIO);
  AFIO_GPxConfig         (GPIO_PA, 5, AFIO_FUN_GPIO);
  AFIO_GPxConfig         (GPIO_PA, 6, AFIO_FUN_GPIO);
  AFIO_GPxConfig         (GPIO_PA, 7, AFIO_FUN_GPIO);
	((volatile  HT_GPIO_TypeDef*)HT_GPIOA_BASE)->DIRCR = 0xFF; //set port A bits 0-7 as output
	
	
	// PORT B
	AFIO_GPxConfig(GPIO_PB, AFIO_PIN_2, AFIO_FUN_GPIO);  //status input pin PB2
	AFIO_GPxConfig(GPIO_PB, AFIO_PIN_3, AFIO_FUN_GPIO);
	AFIO_GPxConfig(GPIO_PB, AFIO_PIN_6, AFIO_FUN_GPIO);  //status input from GSM modem
//  AFIO_GPxConfig(GPIO_PB, AFIO_PIN_13, AFIO_FUN_GPIO);
//  AFIO_GPxConfig(GPIO_PB, AFIO_PIN_14, AFIO_FUN_GPIO);

				AFIO_GPxConfig(GPIO_PB, AFIO_PIN_13, AFIO_FUN_GPIO);
				AFIO_GPxConfig(GPIO_PB, AFIO_PIN_14, AFIO_FUN_GPIO);
				GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_13, RESET);    
				GPIO_WriteOutBits(HT_GPIOB, GPIO_PIN_14, RESET);
			
				GPIO_ClearOutBits       (HT_GPIOB, GPIO_PIN_13);
				GPIO_DirectionConfig    (HT_GPIOB, GPIO_PIN_13, GPIO_DIR_OUT);
				GPIO_DriveConfig        (HT_GPIOB, GPIO_PIN_13, GPIO_DV_16MA);

				GPIO_ClearOutBits       (HT_GPIOB, GPIO_PIN_14);
				GPIO_DirectionConfig    (HT_GPIOB, GPIO_PIN_14, GPIO_DIR_OUT);
				GPIO_DriveConfig        (HT_GPIOB, GPIO_PIN_14, GPIO_DV_16MA);
			
	GPIO_DirectionConfig(HT_GPIOB, GPIO_PIN_2, GPIO_DIR_IN);
	GPIO_DirectionConfig(HT_GPIOB, GPIO_PIN_3, GPIO_DIR_OUT);
	GPIO_DirectionConfig(HT_GPIOB, GPIO_PIN_6, GPIO_DIR_IN);

  GPIO_PullResistorConfig(HT_GPIOB, GPIO_PIN_2, GPIO_PR_UP);
  GPIO_PullResistorConfig(HT_GPIOB, GPIO_PIN_6, GPIO_PR_UP); 
	
	GPIO_InputConfig(HT_GPIOB, GPIO_PIN_2, ENABLE);
	GPIO_InputConfig(HT_GPIOB, GPIO_PIN_6, ENABLE);

// PORT C
  AFIO_GPxConfig         (GPIO_PC, 1, AFIO_FUN_GPIO);
  AFIO_GPxConfig         (GPIO_PC, 2, AFIO_FUN_GPIO);
  AFIO_GPxConfig         (GPIO_PC, 3, AFIO_FUN_GPIO);
  AFIO_GPxConfig         (GPIO_PC, 14, AFIO_FUN_GPIO);
  AFIO_GPxConfig         (GPIO_PC, 15, AFIO_FUN_GPIO);

	uRegTmp = ((volatile  HT_GPIO_TypeDef*)HT_GPIOC_BASE)->DIRCR;
	uRegTmp |= 0xC00E;
	((volatile  HT_GPIO_TypeDef*)HT_GPIOC_BASE)->DIRCR = uRegTmp; //set bits GPIO Port Cits 1,2,3 and 14,5 5 as output

		// set the com and seg outputs
		((volatile  HT_GPIO_TypeDef*)HT_GPIOA_BASE)->DOUTR = 0x00FF;  // SEGS 0 = ON
		((volatile  HT_GPIO_TypeDef*)HT_GPIOB_BASE)->DOUTR = 0XFFFF;  // COMS 0 = ON
		((volatile  HT_GPIO_TypeDef*)HT_GPIOC_BASE)->DOUTR = 0xFFFF;  // COMS 0 = ON
		
//PORT D
	AFIO_GPxConfig(GPIO_PD, AFIO_PIN_0, AFIO_FUN_GPIO);
	GPIO_DirectionConfig(HT_GPIOD, GPIO_PIN_0, GPIO_DIR_IN); // BAT CHG STATUS
	GPIO_InputConfig(HT_GPIOD, GPIO_PIN_0, ENABLE);
	
}
//-----------------------------------------------------------------------------

void GPIO_Sleep_Mode_Configuration(void)
{
	AFIO_GPxConfig(GPIO_PA, AFIO_PIN_0 | AFIO_PIN_1 | AFIO_PIN_2 | AFIO_PIN_3 | AFIO_PIN_4 | AFIO_PIN_5 | AFIO_PIN_6 | AFIO_PIN_7, AFIO_FUN_GPIO);
	AFIO_GPxConfig(GPIO_PB, AFIO_PIN_2 | AFIO_PIN_3 | AFIO_PIN_4 | AFIO_PIN_5 | AFIO_PIN_6 | AFIO_PIN_7 | AFIO_PIN_8 | AFIO_PIN_9 /*| AFIO_PIN_13 | AFIO_PIN_14*/, AFIO_FUN_GPIO);
	AFIO_GPxConfig(GPIO_PC, AFIO_PIN_1 | AFIO_PIN_2 | AFIO_PIN_3 | AFIO_PIN_8 | AFIO_PIN_9 | AFIO_PIN_13 | AFIO_PIN_14, AFIO_FUN_GPIO);
	AFIO_GPxConfig(GPIO_PD, AFIO_PIN_3 | AFIO_PIN_4 | AFIO_PIN_5, AFIO_FUN_GPIO);
	
	GPIO_PullResistorConfig(HT_GPIOA, AFIO_PIN_0 | AFIO_PIN_1 | AFIO_PIN_2 | AFIO_PIN_3 | AFIO_PIN_4 | AFIO_PIN_5 | AFIO_PIN_6 | AFIO_PIN_7, GPIO_PR_DISABLE);
	GPIO_PullResistorConfig(HT_GPIOB, AFIO_PIN_2 | AFIO_PIN_3 | AFIO_PIN_4 | AFIO_PIN_5 | AFIO_PIN_6 | AFIO_PIN_7 | AFIO_PIN_8 | AFIO_PIN_9 /*| AFIO_PIN_13 | AFIO_PIN_14*/, GPIO_PR_DISABLE);
	GPIO_PullResistorConfig(HT_GPIOC, AFIO_PIN_1 | AFIO_PIN_2 | AFIO_PIN_3 | AFIO_PIN_8 | AFIO_PIN_9 | AFIO_PIN_13 | AFIO_PIN_14, GPIO_PR_DISABLE);
	GPIO_PullResistorConfig(HT_GPIOD, AFIO_PIN_3 | AFIO_PIN_4 | AFIO_PIN_5, GPIO_PR_DISABLE);

	GPIO_DirectionConfig(HT_GPIOA, AFIO_PIN_0 | AFIO_PIN_1 | AFIO_PIN_2 | AFIO_PIN_3 | AFIO_PIN_4 | AFIO_PIN_5 | AFIO_PIN_6 | AFIO_PIN_7, GPIO_DIR_IN);
	GPIO_DirectionConfig(HT_GPIOB, AFIO_PIN_2 | AFIO_PIN_3 | AFIO_PIN_4 | AFIO_PIN_5 | AFIO_PIN_6 | AFIO_PIN_7 | AFIO_PIN_8 | AFIO_PIN_9 /*| AFIO_PIN_13 | AFIO_PIN_14*/, GPIO_DIR_IN);
	GPIO_DirectionConfig(HT_GPIOC, AFIO_PIN_1 | AFIO_PIN_2 | AFIO_PIN_3 | AFIO_PIN_8 | AFIO_PIN_9 | AFIO_PIN_13 | AFIO_PIN_14, GPIO_DIR_IN);
	GPIO_DirectionConfig(HT_GPIOD, AFIO_PIN_3 | AFIO_PIN_4 | AFIO_PIN_5, GPIO_DIR_IN);
	
}

