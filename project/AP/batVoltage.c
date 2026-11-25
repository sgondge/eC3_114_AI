#include "batVoltage.h"
#include "utils.h"

extern volatile float gBatVoltage = 0;

void ADC_Configuration(void)
{	//functionStatus = 100;
	CKCU_PeripClockConfig_TypeDef CKCUClock = {{0}};
  CKCUClock.Bit.ADC   = 1;
  CKCUClock.Bit.AFIO  = 1;
  CKCU_PeripClockConfig(CKCUClock, ENABLE);

  ADC_Reset(HT_ADC);

  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_7, AFIO_FUN_ADC);  // Config AFIO mode
	{
		CKCU_SetADCnPrescaler(CKCU_ADCPRE_ADC0, CKCU_ADCPRE_DIV64);    // Setting ADCLK frequency
		ADC_RegularGroupConfig(HT_ADC, CONTINUOUS_MODE, 1, 0);  // Mode, Length , SubLength
	
		ADC_SamplingTimeConfig(HT_ADC, 0);
		ADC_RegularChannelConfig(HT_ADC, ADC_CH_9, 0, 0);
		ADC_RegularTrigConfig(HT_ADC, ADC_TRIG_SOFTWARE);     //Trigger mode
	}

  ADC_IntConfig(HT_ADC, ADC_INT_SINGLE_EOC, ENABLE);

//  NVIC_EnableIRQ(ADC_IRQn);
  ADC_Cmd(HT_ADC, ENABLE);
  ADC_SoftwareStartConvCmd(HT_ADC, ENABLE);   // Trigger to start
}
