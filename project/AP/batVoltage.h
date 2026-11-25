#ifndef BATVOLTAGE_H
#define BATVOLTAGE_H

#include <ht32.h>
//#include <string.h>
//#include <stdarg.h>
//#include <stdlib.h>
//#include <stdint.h>
//#include <ctype.h>

void ADC_Configuration(void);

extern vu32 gBATVoltageLevel;
extern volatile bool gBATV_SingleEOC;
extern volatile float gBatVoltage;

#endif
