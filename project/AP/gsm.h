#ifndef GSM_H
#define GSM_H

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "ht32.h"

void GSMpoweron (u16 pulseLength);
/** PWRKEY power-cycle; returns true when *ATREADY and AT OK are seen after boot. */
bool GSMreboot(void);
bool isGSMpoweredUp (void);
/** True when STATUS is up and a recent AT on UART0 returned OK (see GSMprobeATUartOk cache). */
bool GSMisReadyForAT(void);

void gsmApplyA7682eUpstreamTcpMssClamp(void);
/** Wait for modem boot URCs to finish before sending AT commands. */
void gsmWaitForBootQuiet(u32 quiet_ms, u32 max_ms);
#endif
