#ifndef CRC_H
#define CRC_H
#include "ht32.h"


//void CRC_Configuration(CRC_Mode crcMode, uint16_t seed);
////unsigned int GetCRC16(volatile unsigned char *puchMsg, unsigned int DataLen);
//extern const unsigned short g_McRctable_16[256];
//unsigned short GetCrc_16(unsigned char * pData, u8 nLength, unsigned short init, const unsigned short *ptable);
//unsigned short CRC_GetModbus16(unsigned char *pdata, u8 len);

#include "ht32.h"

void CRC_Configuration(void);

extern u32 seed;
//extern u16 CRC_Result;

#endif
