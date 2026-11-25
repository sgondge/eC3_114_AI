#ifndef __CLI_H
#define __CLI_H

#include "ht32.h"
int cli(HT_USART_TypeDef *thisUsart);
void my_fprintf(HT_USART_TypeDef *thisUsart, const char *format, ...);

extern bool isGSMbridge;

#endif
