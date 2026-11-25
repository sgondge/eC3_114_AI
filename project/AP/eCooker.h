#ifndef ECOOKER_H
#define ECOOKER_H

#include <ht32.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define WAIT_100_mS 100
#define WAIT_250_mS 250
#define WAIT_500_mS 500
#define WAIT_1_SECOND  1000
#define WAIT_3_SECONDS 3000

#define SINGLE_STOVE 1
#define DOUBLE_STOVE 2

typedef struct  eCooker_struc {
	uint8_t  eCookerVersion;
	uint8_t  gsmModemType;
	uint8_t  numberOfPlates;
	char* sid;
	bool isPBCIconfigured;
	bool isTKconfigured;
} eCooker_t;

//SINGLE GLOBAL eCooker configuration structure
extern eCooker_t eCooker;
extern bool Deepsleep_Start_Flag;
extern bool ecConfig;

void eCooker_Configuration (void);
bool isStoveI2cConnected(void);
#endif
