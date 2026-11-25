#include "ht32.h"
#include "GPIO.h"
#include "BFTM0.h"
#include "UART0.h"
#include "USART1.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include "gsm.h"
#include "LEDC.h"
#include "utils.h"
#include "FSM.h"
#include "GPTM0.h"

static void gsm_wait_ms_yielding(uint32_t ms)
{
	uint32_t t = bftm0_ct;
	while ((bftm0_ct - t) < ms) {
		GPTM0_ProcessEvents();
	}
}

// PB3 is the output controlling the GSM PWRKEY pin
// PB6 is the input reading the GSM STATUS pin

#define GSM_A7682E_CLAMP_IPV4_MTU 1240
#define GSM_XSTR(s) GSM_STR(s)
#define GSM_STR(s) #s

bool GSMisPoweredUp = false;

void gsmApplyA7682eUpstreamTcpMssClamp(void)
{
	sendToGSM("AT+CSOC=\"Mss\"," GSM_XSTR(GSM_A7682E_CLAMP_IPV4_MTU));
	Systick_Delay_us(25000);
}

void GSMpoweron (u16 pulseLength) {
	// the A7862E is powered on by raising PB3 for 50mS
	volatile uint32_t sysTickNow;
	// status is confirmed by PB6 going high after 7 seconds
		GPIO_SetOutBits((HT_GPIO_TypeDef*) HT_GPIOB_BASE, GPIO_PIN_3);
		//wait here for pulseLength mS
		sysTickNow = bftm0_ct;
		while (1)  { 
			if ((bftm0_ct - sysTickNow) > pulseLength) break;
			GPTM0_ProcessEvents();
			if (currentState != STATE_FACTORY) {
				runLoadingAnimationNonBlocking();
			}
		}		
		GPIO_ClearOutBits((HT_GPIO_TypeDef*) HT_GPIOB_BASE, GPIO_PIN_3);
}

bool GSMreboot(void) {
	uint32_t t;
	u8 attempt;

	gsmInvalidateAtProbeCache();

	/* Long PWRKEY only when STATUS says up — avoids re-shutdown when already off. */
	if (isGSMpoweredUp()) {
		GSMpoweron(2500);
		t = bftm0_ct;
		while (isGSMpoweredUp() && ((bftm0_ct - t) < 10000u)) {
			GPTM0_ProcessEvents();
		}
		gsm_wait_ms_yielding(1000u);
		gsmInvalidateAtProbeCache();
	}

	for (attempt = 0; attempt < 3u; attempt++) {
		GSMpoweron(50);
		t = bftm0_ct;
		while (!isGSMpoweredUp() && ((bftm0_ct - t) < 10000u)) {
			GPTM0_ProcessEvents();
		}
		if (isGSMpoweredUp()) {
			break;
		}
	}
	if (!isGSMpoweredUp()) {
		return false;
	}

	gsmAllowFplmnWriteAfterGsmReboot();
	gsmWaitForBootQuiet(3000, 20000);

	for (attempt = 0; attempt < 5u; attempt++) {
		if (GSMprobeATUartOk()) {
			return true;
		}
		gsm_wait_ms_yielding(1000u);
	}
	return false;
}

bool isGSMpoweredUp (void) {
		//functionStatus = 177;
	// read the status pin
	FlagStatus status;	
	status =  GPIO_ReadInBit(((HT_GPIO_TypeDef*)HT_GPIOB_BASE), GPIO_PIN_6);
	return ((bool)!status);// == SET);
}

bool GSMisReadyForAT(void) {
	if (!isGSMpoweredUp()) {
		return false;
	}
	return GSMprobeATUartOk();
}

void gsmWaitForBootQuiet(u32 quiet_ms, u32 max_ms)
{
	uint32_t start = bftm0_ct;
	uint32_t last_activity = bftm0_ct;
	u16 prev_cnt = rxd_comm0.cnt;

	while ((bftm0_ct - start) < max_ms) {
		if (rxd_comm0.cnt != prev_cnt) {
			prev_cnt = rxd_comm0.cnt;
			last_activity = bftm0_ct;
		}
		if ((bftm0_ct - last_activity) >= quiet_ms) {
			break;
		}
		GPTM0_ProcessEvents();
	}
	UART0_ClearRxBuffer();
}
