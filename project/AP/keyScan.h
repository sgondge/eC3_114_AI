#ifndef keyScan_H
#define keyScan_H

#include "ht32.h" 
#include "ht32_board.h"
#include "ht32_board_config.h"

#include "ht32_touchkey.h"
#include "ht32_touchkey_conf.h"
#include "ht32_touchkey_BSconf.h"

/**************************SLEEP MODE DEFINE*************************/
#define USE_SLEEP_MODE          0
#define USE_DEEP_SLEEP1_MODE    1
#define USE_DEEP_SLEEP2_MODE    2
/************************Select sleep mode***************************/
#define SEL_SLEEP_MODE          USE_DEEP_SLEEP2_MODE    
/********************************************************************/

void TKEY_KeySleepFun(void);
u8 TKL_USE_RAM[TKL_RAM_Len];

/* Global variables ----------------------------------------------------------------------------------------*/
#include "TocuhFunction.c"

int i=0;
u32 KeyData, a;
uint32_t touchKey;

void keyScan();

#endif