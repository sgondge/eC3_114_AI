#include "ht32.h"
#include "CRC.h"

void CRC_Configuration(void)
{
  /* !!! NOTICE !!!
     Notice that the local variable (structure) did not have an initial value.
     Please confirm that there are no missing members in the parameter settings below in this function.
  */
	CRC_InitTypeDef CRC_InitStructure;
	CKCU_PeripClockConfig_TypeDef CKCUClock = {{0}};
  CKCUClock.Bit.CRC        = 1;
  CKCU_PeripClockConfig(CKCUClock, ENABLE);
	
  CRC_InitStructure.Mode = CRC_16_POLY;
  CRC_InitStructure.uSeed = seed;
  CRC_InitStructure.uCR = 0;
  CRC_Init(HT_CRC, &CRC_InitStructure);
	
}

u32 seed = 0xFFFF;
//u16 CRC_Result = 0;
