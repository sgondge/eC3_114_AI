#include "ht32.h"
#include "FSM.h"
#include <stdlib.h>
#include <string.h>
#include "LEDC.h"
#include "cli.h"
#include "buzzer.h"
#include "Payload.h"
#include "Utils.h"
#include "buzzer.h"
#include "eCooker.h"
#include "RTC.h"
#include "EEPROM.h"
#include "gsm.h"
#include "I2C0.h"
#include "I2C1.h"
#include "CRC.h"
#include "stove_stats.h"
#include <stdio.h>
#include "ota_commands.h"
#include "ht32_touchkey.h"
#include "TouchFunction.h"

void fotaUpdate(void);

long main_onT = 0;
bool cook_done = false;
u8 I2C_Tx_Data[8] = {0};
volatile u8 I2C_Rx_Data[16] = {0};
u8 I2C1_Tx_Data[8] = {0};
volatile u8 I2C1_Rx_Data[16] = {0};
unsigned char CRC_calculation[3] = {0};
unsigned int CRC_16_value = 0;

uint8_t selectedPan = 0;
static uint32_t click_bftm0 = 0;
bool eventTriggered = false;
bool syncTriggered = false;
bool batlvlTriggered = false;
static uint32_t sync_bftm0 = 0;
static uint32_t batlvl_bftm0 = 0;
static uint32_t atecdisp_bftm0 = 0;
bool atecdispTriggered = false;
bool plateConfigTriggered = false;
static uint32_t plateConfig_bftm0 = 0;
bool HTriggered = false;
static uint32_t H_bftm0 = 0;

static int digitPosition = 0;
bool timer = false;
bool boost = false;
bool EVENT_ON_STATUS = false;
bool EVENT_OFF_STATUS = false;
bool CONNECT_COMMAND_STATUS = false;

uint16_t ntimer = 0;
uint16_t nlock = 0;

plate left = {0, false, 0, 0, {0,0,0,0}, false, false};
plate right = {0, false, 0, 0, {0,0,0,0}, false, false};

CookingSession leftPanSession = { 'L', 0, 0, 0, false };
CookingSession rightPanSession = { 'R', 0, 0, 0, false };

volatile CookingTimeMonitor cookingTimeMonitor = {0U, 0U};

unsigned long synced_epoch_time = 0;

static const uint32_t MIN_VALID_EPOCH = 1735689600UL; // Jan 1, 2025 00:00:00 UTC

static bool session_time_is_epoch(uint32_t t) {
    return (bool)(t >= MIN_VALID_EPOCH);
}

static uint32_t session_time_now(bool use_epoch) {
    return use_epoch ? getEpochTimeRTC() : (uint32_t)device_time;
}

static uint32_t get_cooking_monitor_now(void) {
    return __init_sync_time__ ? getEpochTimeRTC() : (uint32_t)device_time;
}

static uint32_t cooking_session_duration(const CookingSession* session, uint32_t now) {
    if (!session) {
        return 0U;
    }

    // If the session is not active or onTime got corrupted/never set, treat as inactive.
    if (!session->active || session->onTime == 0U) {
        return 0U;
    }

    bool start_is_epoch = session_time_is_epoch(session->onTime);
    bool now_is_epoch   = session_time_is_epoch(now);

    uint32_t effective_now = now;

    if (!start_is_epoch && now_is_epoch) {
        effective_now = (uint32_t)device_time;
    } else if (start_is_epoch && !now_is_epoch) {
        return 0U;
    }

    if (effective_now < session->onTime) {
        return 0U;
    }

    return effective_now - session->onTime;
}

void updateCookingTimeMonitor(void) {
    uint32_t now = get_cooking_monitor_now();

    cookingTimeMonitor.left_seconds = cooking_session_duration(&leftPanSession, now);
    if (cookingTimeMonitor.left_seconds > 7200U) {
        cookingTimeMonitor.left_seconds = 7200U;
    }

    if (eCooker.numberOfPlates == DOUBLE_STOVE) {
        cookingTimeMonitor.right_seconds = cooking_session_duration(&rightPanSession, now);
        if (cookingTimeMonitor.right_seconds > 7200U) {
            cookingTimeMonitor.right_seconds = 7200U;
        }
    } else {
        cookingTimeMonitor.right_seconds = 0U;
    }
}

void syncDelay(void){
	syncTriggered = false;
	sync_bftm0 = bftm0_ct;
}

void ATECdispDelay(void){
	atecdispTriggered = false;
	atecdisp_bftm0 = bftm0_ct;
}

void plateConfigDelay(void){
	plateConfigTriggered = false;
	plateConfig_bftm0 = bftm0_ct;
}


void batlvlDelay(void){
	batlvlTriggered = false;
	batlvl_bftm0 = bftm0_ct;
}

void HDelay(void){
	HTriggered = false;
	H_bftm0 = bftm0_ct;
}

void closeSession(CookingSession* session) {
    if (session->active) {
        const bool on_is_epoch = session_time_is_epoch(session->onTime);
        session->offTime = session_time_now(on_is_epoch);
        if (session->offTime < session->onTime) {
            session->offTime = session_time_now(on_is_epoch);
        }
        if ((session->offTime - session->onTime) > 5U) {
            if ((session->offTime - session->onTime) > 7200U) {
                session->onTime = session->offTime - 7200U;
            }
            cook_done = true;
            if (session->offTime > session->onTime) {
                int32_t sess_count = Emulated_EEPROM_Read_Cooking_Session_Num();
                if (sess_count >= (int32_t)COOK_SESSION_MAX_COUNT) {
                    // Flash full: overwrite the last slot without incrementing
                    Emulated_EEPROM_Overwrite_Last_Cook_Session(
                        session->pan,
                        session->onTime,
                        session->offTime,
                        session->power,
                        globalPayload.stat.Ag,
                        globalPayload.stat.Vg
                    );
                } else {
                    Emulated_EEPROM_Write_Cook_Session(
                        sess_count,
                        session->pan,
                        session->onTime,
                        session->offTime,
                        session->power,
                        globalPayload.stat.Ag,
                        globalPayload.stat.Vg
                    );
                    Emulated_EEPROM_Write_Cooking_Session_Num(sess_count + 1);
                }
            }
        }
        session->active = false;
        session->onTime = 0U;
        session->offTime = 0U;
        session->power = 0U;
    }
    updateCookingTimeMonitor();
}


void startSession(CookingSession* session, uint16_t newPower) {
    closeSession(session);
    session->onTime = __init_sync_time__ ? getEpochTimeRTC() : device_time;
    session->power = newPower;
    session->active = true;
    session->offTime = 0U;
    updateCookingTimeMonitor();
		pg_avg_reset();
}


void startSessionForSelectedPan(uint8_t pan, uint16_t power) {
    if (pan == 0 && !left.error) {
        startSession(&leftPanSession, power);
        left.power = power;   
    } else if (pan == 1 && !right.error) {
        startSession(&rightPanSession, power);  
        right.power = power; 
    }
		
		((pan == 0 && left.boost) || (pan == 1 && right.boost)) ? (void)0 : (writePowerLevel(left.power, right.power));

		if(eCooker.numberOfPlates == SINGLE_STOVE){
			I2C_Tx_Data[0] = 3;
			I2C_Tx_Data[1] = power;
			
			/**********************  CRC calculation for Single Stove ****************************************/
			CRC_calculation[0] = I2C_Tx_Data[0];
			CRC_calculation[1] = I2C_Tx_Data[1];
			CRC_calculation[2] = 0x00;		// temperature setting
			//CRC_16_value = CRC_GetModbus16(CRC_calculation, sizeof(CRC_calculation));
			CRC_16_value = CRC_16(seed,CRC_calculation, sizeof(CRC_calculation));
			I2C_Tx_Data[3] = (CRC_16_value >> 8) & 0xFF; // Most significant byte
			I2C_Tx_Data[4] = CRC_16_value & 0xFF;        // Least significant byte			
			/*************************************************************************************************/
		}
		else{
			if(selectedPan==0){
				I2C_Tx_Data[0] = 1;
				I2C_Tx_Data[1] = power;
				
				/**********************  CRC calculation for Double Stove Left /1 PAN ****************************************/
				CRC_calculation[0] = I2C_Tx_Data[0];
				CRC_calculation[1] = I2C_Tx_Data[1];
				CRC_calculation[2] = 0x00;		// temperature setting
				CRC_16_value = CRC_16(seed,CRC_calculation, sizeof(CRC_calculation));
				
				I2C_Tx_Data[3] = (CRC_16_value >> 8) & 0xFF; // Most significant byte
				I2C_Tx_Data[4] = CRC_16_value & 0xFF;        // Least significant byte	
				/*************************************************************************************************************/
			}
			else{
				//change this to plateId 1 to have stove replacement
				I2C1_Tx_Data[0] = 1;
				I2C1_Tx_Data[1] = power;
				
				/**********************  CRC calculation for Double Stove Right /2 PAN ****************************************/
				CRC_calculation[0] = I2C1_Tx_Data[0];
				CRC_calculation[1] = I2C1_Tx_Data[1];
				CRC_calculation[2] = 0x00;		// temperature setting
				CRC_16_value = CRC_16(seed, CRC_calculation, sizeof(CRC_calculation));
				
				I2C1_Tx_Data[3] = (CRC_16_value >> 8) & 0xFF; // Most significant byte
				I2C1_Tx_Data[4] = CRC_16_value & 0xFF;        // Least significant byte	
				/*************************************************************************************************************/
			}
		}
//		//pbciSetPower(pan, power);
//		pbciPeriodicFunction();
}

uint16_t prev = 0;

void BoostMode(uint8_t selected_pan, bool enable){	
		switch(selected_pan){
			case 0:
				//LEFT PLATE ON BOOST
			enable ? (
				writeBoostLevel(selected_pan),
				prev = left.power,
				left.power = 10,			
				I2C_Tx_Data[0] = 1,
				I2C_Tx_Data[1] = left.power,
				right.power = 0,
				I2C1_Rx_Data[0] = 2,
				I2C1_Tx_Data[1] = right.power,
				right.timer ? (
					right.mins = 0,
					right.secs = 0,
					right.timer = false,
					right.timerDigits[0] = 0,
					right.timerDigits[1] = 0,
					right.timerDigits[2] = 0,
					right.timerDigits[3] = 0,
					(selectedPan == 1) ? LEDC_redLED(TIMER_KEY, false) : (void)0,
					// Exit STATE_COUNTDOWN if currently viewing right plate countdown
					(currentState == STATE_COUNTDOWN && selectedPan == 1) ? (
						writePowerLevel(left.power, right.power),
						LEDC_setSingleRedLED(right.power),
						enterState(STATE_COOKING)
					) : (void)0
				) : (void)0,
				left.boost = true
					) :
				(
					left.power = prev,
					I2C_Tx_Data[0] = 1,
					I2C_Tx_Data[1] = prev,
					LEDC_setSingleRedLED(left.power),
					writePowerLevel(left.power, right.power),
					left.power != 0 ? startSessionForSelectedPan(selected_pan, left.power) : closeSession(&leftPanSession),
					left.boost = false
				);
				break;
			
			case 1:
				//RIGHT PLATE ON BOOST
			enable ? (
				writeBoostLevel(selected_pan),
				prev = right.power,
				right.power = 10,				
				I2C1_Rx_Data[0] = 2,
				I2C1_Tx_Data[1] = right.power,
				left.power = 0,
				I2C_Tx_Data[0] = 1,
				I2C_Tx_Data[1] = left.power,
				left.timer ? (
					left.mins = 0,
					left.secs = 0,
					left.timer = false,
					left.timerDigits[0] = 0,
					left.timerDigits[1] = 0,
					left.timerDigits[2] = 0,
					left.timerDigits[3] = 0,
					(selectedPan == 0) ? LEDC_redLED(TIMER_KEY, false) : (void)0,
					// Exit STATE_COUNTDOWN if currently viewing left plate countdown
					(currentState == STATE_COUNTDOWN && selectedPan == 0) ? (
						writePowerLevel(left.power, right.power),
						LEDC_setSingleRedLED(left.power),
						enterState(STATE_COOKING)
					) : (void)0
				) : (void)0,
				right.boost = true
					) :
				(
					right.power = prev,				
					I2C1_Rx_Data[0] = 2,
					I2C1_Tx_Data[1] = prev,	
					LEDC_setSingleRedLED(right.power),
					writePowerLevel(left.power, right.power),
					right.power != 0 ? startSessionForSelectedPan(selected_pan, right.power) : closeSession(&rightPanSession),
					right.boost = false
				);			
		}
}


volatile State currentState = STATE_ONOFF;
State previousState;
Event lastEvent = EVENT_INVALID;

Event getEventFromInput(u8 input) {
    switch (input) {
        case 3 : return EVENT_KEY_PRESS_0;
        case 4 : return EVENT_KEY_PRESS_1;
        case 5 : return EVENT_KEY_PRESS_2;
        case 6 : return EVENT_KEY_PRESS_3;
        case 7 : return EVENT_KEY_PRESS_4;
        case 8 : return EVENT_KEY_PRESS_5;
        case 9 : return EVENT_KEY_PRESS_6;
        case 10: return EVENT_KEY_PRESS_7;
        case 11: return EVENT_KEY_PRESS_8;
        case 12: return EVENT_KEY_PRESS_9;
        case 17: return EVENT_ONOFF_KEY;
        case 13: return EVENT_BOOST_KEY;
        case 0 : return EVENT_PAYGO_KEY;
        case 1 : return EVENT_SYNC_KEY;
        case 16: return EVENT_TIMER_KEY;
        case 15: return EVENT_LOCK_KEY;
        case 2 : return EVENT_L_PLATE_SELECT_KEY;
        case 14: return EVENT_R_PLATE_SELECT_KEY;
        case 18: return EVENT_READ_ERROR;
				case 20: return EVENT_LOCK_KEY_LONG;
				case 21: return EVENT_TIMER_KEY_LONG;
				case 22: return EVENT_BOOST_KEY_LONG;
				case 23: return EVENT_SYNC_KEY_LONG;
				case 24: return EVENT_KEY_PRESS_0_B;
				case 25: return EVENT_L_PLATE_SELECT_KEY_LONG;
				case 26: return EVENT_R_PLATE_SELECT_KEY_LONG;
        default: return EVENT_INVALID;
    }}

void enterState(State newState) {
		extern bool updateAnimationActive;
		uint32_t primask = __get_PRIMASK();
		__disable_irq();

		if (updateAnimationActive && newState != STATE_ONOFF && currentState == STATE_ONOFF) {
			__set_PRIMASK(primask);
			return;  // Block transition from STATE_ONOFF to any other state
		}

		(currentState == STATE_COUNTDOWN) || (currentState == STATE_LOCK)? (void)0 : Buzzer_LongBeep(2000, 250);
		previousState = currentState;
    currentState = newState;
    lastEvent = EVENT_INVALID;
    __set_PRIMASK(primask);

    /* Wake touch controller as soon as we leave OFF state so keys are accepted sooner */
    if (previousState == STATE_ONOFF && newState != STATE_ONOFF) {
        TKL_LowPowerExit();
        networkAllowFplmnAfterStoveOn();
    }

    if (newState == STATE_ONOFF) {

				/* Run every iteration so FOTA runs in STATE_ONOFF even when ecConfig is false */
				fotaUpdate();
        I2C_Tx_Data[1] = 0;
        I2C1_Tx_Data[1] = 0;
    }

    if (newState == STATE_FACTORY) {
        TouchFunction_ResetFactoryLPanHold();
        TouchFunction_ResetFactoryRPanHold();
        factoryConnHold();
    }
}

void handleEvent(Event event) {
    switch (currentState) {
        case STATE_IDLE:
            stateIdleHandler(event);
            break;
        case STATE_COOKING:
            stateCookingHandler(event);
            break;
//        case STATE_ERROR:
//            stateErrorHandler(event);
//            break;
        case STATE_ONOFF:
            stateOnOffHandler(event);
            break;
        case STATE_FACTORY:
            stateFactoryHandler(event);
            break;
        case STATE_LOCK:
            stateLockHandler(event);
            break;
        case STATE_SYNC:
            stateSyncHandler(event);
            break;
        case STATE_PAYGO:
            statePayGoHandler(event);
            break;
				case STATE_TIMER:
						stateTimerHandler(event);
						break;
				case STATE_COUNTDOWN:
						stateCountDownHandler(event);
						break;
				default:
            break;
    }
}

void stateOnOffHandler(Event event) {
    switch (event) {
        case EVENT_ONOFF_KEY:
						if(updateAnimationActive){
							return;  // Ignore ON/OFF key presses during update
						}
						resetCGSNRetryCountOnGSMReset();
						resetEpochRetryStateOnGSMReset();
						if (!isStoveI2cConnected()) {
							/* No stove I2C master: block turn-on only, leave display unchanged */
							break;
						}
						// I2C present but plate count not configured yet
						if(eCooker.numberOfPlates == 0){
							writeATECToSevenSeg();
							plateConfigDelay();
						}
    					else {
							sendToGSMAndPrintResponse(HT_USART1, "AT", "OK");
							globalPayload.stat.stvcnt++;
							deviceInteractionIdle();
							HTriggered = true;
							main_onT = getEpochTimeRTC();
							LEDC_redLED(POWER_KEY, true);
							writePowerLevel(left.power, right.power);
							eCooker.numberOfPlates == DOUBLE_STOVE ? (void)0 : LEDC_setSingleRedLED(left.power);
							eCooker.numberOfPlates == DOUBLE_STOVE ? enterState(STATE_IDLE) : (enterState(STATE_COOKING));
//							if(PayloadOption==0)
//								sendToGSMAndPrintResponse(HT_USART1, "AT+CMQTTCONNECT?","tcp://a3b6socfqdtpq4-ats.iot.ap-southeast-1.amazonaws.com:8883") ? handleSyncLED(LED_STEADY, 'g') : (handleSyncLED(LED_BLINK, 'r'),connect());
//							else
//								isGSMpoweredUp() ? handleSyncLED(LED_STEADY, 'g') : handleSyncLED(LED_BLINK, 'r');
							EVENT_ON_STATUS = true;
							fwUpdate.success = false;
							}
						break;
        case EVENT_READ_ERROR:
            enterState(STATE_ERROR);
            break;
				case EVENT_LOCK_KEY_LONG:
					Buzzer_LongBeep(2000, 250);
					writeBatLevel(getBatV()); //change this to the actual battery volatage
					batlvlDelay();
						break;
				case EVENT_SYNC_KEY_LONG:
					if(PayloadOption == 0){
						PayloadOption = 1;
						writeSevenSegInt(1);
					}
					else{
						PayloadOption = 0;
						writeSevenSegInt(0);
					}
					break;
				case EVENT_KEY_PRESS_0_B:
					buzz(5);
					disableHdispSwitch();
					enterState(STATE_FACTORY);
					writeATECToSevenSeg();
					ATECdispDelay();
				break;
//				case EVENT_PAYGO_KEY:
//            enterState(STATE_PAYGO);
//            break;
        default:
            break;
		}
}

void stateIdleHandler(Event event) {
    switch (event) {
        case EVENT_ONOFF_KEY:
					LEDC_redLED(POWER_KEY, false);
					handlePaygoLED(LED_OFF, 'g');
					handleSyncLED(LED_OFF, 'g');
					clearSevenSegDisplay();
					LEDC_redLED(R_PLATE, false);
					LEDC_redLED(L_PLATE, false);
					LEDC_redLED(TIMER_KEY, false);
					LEDC_turnOffAllRedLEDs();
					enterState(STATE_ONOFF);
					EVENT_OFF_STATUS = true;
//					gracefulDisconnect();
//					sendToGSMAndPrintResponse(HT_USART1,"AT+CSCLK=2","OK");	
          break;
				
				case EVENT_L_PLATE_SELECT_KEY:
          //printf("Plate selection changed to L\n");
					selectedPan = 0;
					LEDC_redLED(R_PLATE, false);
					LEDC_redLED(L_PLATE, true);
					left.timer ? LEDC_redLED(TIMER_KEY, true) : LEDC_redLED(TIMER_KEY, false);
					writePowerLevel(left.power, right.power);
					LEDC_setSingleRedLED(left.power);
					enterState(STATE_COOKING);
          break;
				
				case EVENT_R_PLATE_SELECT_KEY:
          //printf("Plate selection changed to R\n");
					selectedPan = 1;
					LEDC_redLED(R_PLATE, true);
					LEDC_redLED(L_PLATE, false);
					right.timer ? LEDC_redLED(TIMER_KEY, true) : LEDC_redLED(TIMER_KEY, false);
					writePowerLevel(left.power, right.power);
					LEDC_setSingleRedLED(right.power);
					enterState(STATE_COOKING);
          break;
				
        case EVENT_LOCK_KEY_LONG:
					nlock++;
					LEDC_redLED(LOCK_KEY, true);
          enterState(STATE_LOCK);
          break;
				
        case EVENT_SYNC_KEY_LONG:
					enterState(STATE_SYNC);
					Buzzer_LongBeep(2000, 250);
					CONNECT_COMMAND_STATUS = true;
          break;
				
// DISABLED PAYGO
//        case EVENT_PAYGO_KEY:
//					LEDC_rgbLED(1, false, false, true);
//					Buzzer_LongBeep(2000, 250);
//					writeATECToSevenSeg();
//					connect();
//          enterState(STATE_PAYGO);
//          break;
//        
//				default:
//          break;
			}
}

void eventDelay(void){
	eventTriggered = false;
	click_bftm0 = bftm0_ct;
}

static inline int current_power_cap(void) {
    return g_params.max_power_w;      // 0..10 (10 often treated as BOOST)
}

// Start session if <= cap, otherwise buzz and refuse.
static bool request_power_with_cap(int pan /*0:left,1:right*/, int level /*0..10*/) {
    int cap = current_power_cap();
    if (level > cap) {
        buzz(3);
        return false;
    }
    startSessionForSelectedPan(pan, level);
    if (level <= 9) {                 // UI shows 0..9; boost usually not shown here
        LEDC_setSingleRedLED(level);
    }
    return true;
}

// Immediately enforce cap on both pans (call after lb change).
void enforce_max_power_cap_from_lb(void) {
    int cap = current_power_cap();
    bool any = false;

    // If boost is active and cap < 10, turn boost off.
    if (left.boost  && cap < 10)  { BoostMode(0, false); any = true; }
    if (right.boost && cap < 10)  { BoostMode(1, false); any = true; }

    // Clamp power levels > cap
    if (left.power  > cap)  { request_power_with_cap(0, cap); any = true; }
    if (right.power > cap)  { request_power_with_cap(1, cap); any = true; }

    if (any) {
        writePowerLevel(left.power, right.power);
        // Refresh selected pan LED
        if (selectedPan == 0) LEDC_setSingleRedLED(left.power <= 9 ? left.power : 9);
        else if (selectedPan == 1) LEDC_setSingleRedLED(right.power <= 9 ? right.power : 9);
    }
}


void stateCookingHandler(Event event) {
    // --- Enforce current cap on entry (in case lb lowered it recently) ---
		if (g_params.lb_cap_until_epoch && device_time >= g_params.lb_cap_until_epoch) {
    g_params.max_power_w = DEFAULT_MAX_POWER_W;
    g_params.lb_cap_until_epoch = 0;
		}
		
    int cap = g_params.max_power_w; // 0..10 (10 often used as boost)
    bool changed = false;

    // If cap blocks boost, turn it off
    if (cap < 10) {
        if (left.boost)  { BoostMode(0, false); changed = true; }
        if (right.boost) { BoostMode(1, false); changed = true; }
    }
    // Clamp running power levels down to cap
    if (left.power  > cap)  { startSessionForSelectedPan(0, cap); changed = true; }
    if (right.power > cap)  { startSessionForSelectedPan(1, cap); changed = true; }

    if (changed) {
        writePowerLevel(left.power, right.power);
        if (selectedPan == 0) { LEDC_setSingleRedLED(left.power <= 9 ? left.power : 9); }
        else if (selectedPan == 1) { LEDC_setSingleRedLED(right.power <= 9 ? right.power : 9); }
        Emulated_EEPROM_Write_RuntimeParams();
    }
    // ----------------------------------------------------------------------
    switch (event) {
        case EVENT_ONOFF_KEY:
            right.power = 0;
            right.mins = 0;
            right.secs = 0;
            right.timer = false;
            right.boost = false;
            right.error = false;

            left.power = 0;
            left.mins = 0;
            left.secs = 0;
            left.timer = false;
            left.boost = false;
            left.error = false;

            I2C_Tx_Data[0] = eCooker.numberOfPlates == DOUBLE_STOVE ? 1 : 3;
            I2C_Tx_Data[1] = 0;

            I2C1_Rx_Data[0] = 2;
            I2C1_Tx_Data[1] = 0;

            ((epoch_time - main_onT > 20) && cook_done) ? (handleHdisplay(), HDelay()) : clearSevenSegDisplay();
            LEDC_redLED(POWER_KEY, false);
            handlePaygoLED(LED_OFF, 'g');
            handleSyncLED(LED_OFF, 'g');

            for (int i = 0; i < 4; i++) {
                left.timerDigits[i] = 0;
                right.timerDigits[i] = 0;
            }
            LEDC_turnOffAllRedLEDs();
            LEDC_redLED(L_PLATE, false);
            LEDC_redLED(R_PLATE, false);
            LEDC_redLED(TIMER_KEY, false);
            closeSession(&leftPanSession);
            closeSession(&rightPanSession);
            enterState(STATE_ONOFF);
            EVENT_OFF_STATUS = true;
            break;

        case EVENT_L_PLATE_SELECT_KEY:
            eCooker.numberOfPlates == DOUBLE_STOVE ? (
                (right.boost || left.boost) ? (void)0 : (
                    Buzzer_LongBeep(2000, 250),
                    selectedPan = 0,
                    LEDC_redLED(L_PLATE, true),
                    LEDC_redLED(R_PLATE, false),
                    left.timer ? LEDC_redLED(TIMER_KEY, true) : LEDC_redLED(TIMER_KEY, false),
                    writePowerLevel(left.power, right.power),
                    LEDC_setSingleRedLED(left.power)
                )
            ) : (void)0;
            break;

        case EVENT_R_PLATE_SELECT_KEY:
            eCooker.numberOfPlates == DOUBLE_STOVE ? (
                (right.boost || left.boost) ? (void)0 : (
                    Buzzer_LongBeep(2000, 250),
                    selectedPan = 1,
                    LEDC_redLED(R_PLATE, true),
                    LEDC_redLED(L_PLATE, false),
                    right.timer ? LEDC_redLED(TIMER_KEY, true) : LEDC_redLED(TIMER_KEY, false),
                    writePowerLevel(left.power, right.power),
                    LEDC_setSingleRedLED(right.power)
                )
            ) : (void)0;
            break;

        case EVENT_TIMER_KEY:
            Buzzer_LongBeep(2000, 250);
            LEDC_redLED(TIMER_KEY, true);
            LEDC_turnOffAllRedLEDs();
            if (selectedPan == 0) {
                left.timer ? (void)0 : writeTimeToSevenSeg(00, 00);
                left.timer ? enterState(STATE_COUNTDOWN) : enterState(STATE_TIMER);
                left.timer ? eventDelay() : (void)0;
            }
            if (selectedPan == 1) {
                right.timer ? (void)0 : writeTimeToSevenSeg(00, 00);
                right.timer ? enterState(STATE_COUNTDOWN) : enterState(STATE_TIMER);
                right.timer ? eventDelay() : (void)0;
            }
            break;

        case EVENT_LOCK_KEY_LONG:
            nlock++;
            LEDC_redLED(LOCK_KEY, true);
            enterState(STATE_LOCK);
            break;

        case EVENT_KEY_PRESS_0:
            deviceInteractionIdle();
            Buzzer_LongBeep(2000, 250);
            ((selectedPan == 0 && right.boost) || (selectedPan == 1 && left.boost)) ? (void)0 : (
                ((selectedPan == 0 && left.boost) || (selectedPan == 1 && right.boost)) ? BoostMode(selectedPan, false) : (void)0,

                // plateId select + power 0
                (selectedPan == 0) ? (eCooker.numberOfPlates == SINGLE_STOVE ? (I2C_Tx_Data[0] = 3) : (I2C_Tx_Data[0] = 1)) : (I2C1_Tx_Data[0] = 1),
                (selectedPan == 0) ? (I2C_Tx_Data[1] = 0) : (I2C1_Tx_Data[1] = 0),

                // CRC calc (left)
                (selectedPan == 0) ? (
                    CRC_calculation[0] = I2C_Tx_Data[0],
                    CRC_calculation[1] = I2C_Tx_Data[1],
                    CRC_calculation[2] = 0x00,
                    CRC_16_value = CRC_16(seed, CRC_calculation, sizeof(CRC_calculation)),
                    I2C_Tx_Data[3] = (CRC_16_value >> 8) & 0xFF,
                    I2C_Tx_Data[4] = CRC_16_value & 0xFF
                ) :
                // CRC calc (right)
                (
                    CRC_calculation[0] = I2C1_Tx_Data[0],
                    CRC_calculation[1] = I2C1_Tx_Data[1],
                    CRC_calculation[2] = 0x00,
                    CRC_16_value = CRC_16(seed, CRC_calculation, sizeof(CRC_calculation)),
                    I2C1_Tx_Data[3] = (CRC_16_value >> 8) & 0xFF,
                    I2C1_Tx_Data[4] = CRC_16_value & 0xFF
                ),

                // apply 0 power (always <= cap)
                selectedPan == 0 ? (left.power = 0, closeSession(&leftPanSession)) : (right.power = 0, closeSession(&rightPanSession)),
                writePowerLevel(left.power, right.power),
                LEDC_setSingleRedLED(0)
            );
            break;

        case EVENT_KEY_PRESS_1:
            ((selectedPan == 0 && right.boost) || (selectedPan == 1 && left.boost)) ? (void)0 : (
                ((selectedPan == 0 && left.boost) || (selectedPan == 1 && right.boost)) ? BoostMode(selectedPan, false) : (void)0,
                (1 <= cap) ? (startSessionForSelectedPan(selectedPan, 1), LEDC_setSingleRedLED(1)) : (buzz(3), (void)0)
            );
            break;

        case EVENT_KEY_PRESS_2:
            ((selectedPan == 0 && right.boost) || (selectedPan == 1 && left.boost)) ? (void)0 : (
                ((selectedPan == 0 && left.boost) || (selectedPan == 1 && right.boost)) ? BoostMode(selectedPan, false) : (void)0,
                (2 <= cap) ? (startSessionForSelectedPan(selectedPan, 2), LEDC_setSingleRedLED(2)) : (buzz(3), (void)0)
            );
            break;

        case EVENT_KEY_PRESS_3:
            ((selectedPan == 0 && right.boost) || (selectedPan == 1 && left.boost)) ? (void)0 : (
                ((selectedPan == 0 && left.boost) || (selectedPan == 1 && right.boost)) ? BoostMode(selectedPan, false) : (void)0,
                (3 <= cap) ? (startSessionForSelectedPan(selectedPan, 3), LEDC_setSingleRedLED(3)) : (buzz(3), (void)0)
            );
            break;

        case EVENT_KEY_PRESS_4:
            ((selectedPan == 0 && right.boost) || (selectedPan == 1 && left.boost)) ? (void)0 : (
                ((selectedPan == 0 && left.boost) || (selectedPan == 1 && right.boost)) ? BoostMode(selectedPan, false) : (void)0,
                (4 <= cap) ? (startSessionForSelectedPan(selectedPan, 4), LEDC_setSingleRedLED(4)) : (buzz(3), (void)0)
            );
            break;

        case EVENT_KEY_PRESS_5:
            ((selectedPan == 0 && right.boost) || (selectedPan == 1 && left.boost)) ? (void)0 : (
                ((selectedPan == 0 && left.boost) || (selectedPan == 1 && right.boost)) ? BoostMode(selectedPan, false) : (void)0,
                (5 <= cap) ? (startSessionForSelectedPan(selectedPan, 5), LEDC_setSingleRedLED(5)) : (buzz(3), (void)0)
            );
            break;

        case EVENT_KEY_PRESS_6:
            ((selectedPan == 0 && right.boost) || (selectedPan == 1 && left.boost)) ? (void)0 : (
                ((selectedPan == 0 && left.boost) || (selectedPan == 1 && right.boost)) ? BoostMode(selectedPan, false) : (void)0,
                (6 <= cap) ? (startSessionForSelectedPan(selectedPan, 6), LEDC_setSingleRedLED(6)) : (buzz(3), (void)0)
            );
            break;

        case EVENT_KEY_PRESS_7:
            ((selectedPan == 0 && right.boost) || (selectedPan == 1 && left.boost)) ? (void)0 : (
                ((selectedPan == 0 && left.boost) || (selectedPan == 1 && right.boost)) ? BoostMode(selectedPan, false) : (void)0,
                (7 <= cap) ? (startSessionForSelectedPan(selectedPan, 7), LEDC_setSingleRedLED(7)) : (buzz(3), (void)0)
            );
            break;

        case EVENT_KEY_PRESS_8:
            ((selectedPan == 0 && right.boost) || (selectedPan == 1 && left.boost)) ? (void)0 : (
                ((selectedPan == 0 && left.boost) || (selectedPan == 1 && right.boost)) ? BoostMode(selectedPan, false) : (void)0,
                (8 <= cap) ? (startSessionForSelectedPan(selectedPan, 8), LEDC_setSingleRedLED(8)) : (buzz(3), (void)0)
            );
            break;

        case EVENT_KEY_PRESS_9:
            ((selectedPan == 0 && right.boost) || (selectedPan == 1 && left.boost)) ? (void)0 : (
                ((selectedPan == 0 && left.boost) || (selectedPan == 1 && right.boost)) ? BoostMode(selectedPan, false) : (void)0,
                (9 <= cap) ? (startSessionForSelectedPan(selectedPan, 9), LEDC_setSingleRedLED(9)) : (buzz(3), (void)0)
            );
            break;

        case EVENT_BOOST_KEY:
            if (eCooker.numberOfPlates == 2) {
                // If the other pan is boosting, ignore per original logic
                if (!((selectedPan == 0 && right.boost) || (selectedPan == 1 && left.boost))) {
                    if ((selectedPan == 0 && left.boost) || (selectedPan == 1 && right.boost)) {
                        // Toggle OFF boost (always allowed)
                        Buzzer_LongBeep(2000, 250);
                        BoostMode(selectedPan, false);
                    } else {
                        // Enable boost only if cap allows
                        if (cap < 10) { buzz(3); break; }
                        Buzzer_LongBeep(2000, 250);
                        BoostMode(selectedPan, true);
                        closeSession(&leftPanSession);
                        closeSession(&rightPanSession);
                        startSessionForSelectedPan(selectedPan, 10); // 10 == boost level
                    }
                }
            }
            break;

        case EVENT_READ_ERROR:
            enterState(STATE_ERROR);
            break;

        case EVENT_SYNC_KEY_LONG:
                enterState(STATE_SYNC);
                Buzzer_LongBeep(2000, 250);
                CONNECT_COMMAND_STATUS = true;
            break;

        case EVENT_TIMER_KEY_LONG:
            if ((selectedPan == 0 && left.timer) || (selectedPan == 1 && right.timer)) {
                buzz(3);
                if (selectedPan == 0) {
                    left.mins = 0; left.secs = 0; left.timer = false;
                    for (int i = 0; i < 4; i++) left.timerDigits[i] = 0;
                    LEDC_redLED(TIMER_KEY, false);
                } else {
                    right.mins = 0; right.secs = 0; right.timer = false;
                    for (int i = 0; i < 4; i++) right.timerDigits[i] = 0;
                    LEDC_redLED(TIMER_KEY, false);
                }
            }
            break;

        default:
            break;
    }
}


void stateTimerHandler(Event event) {
    switch (event) {
        case EVENT_KEY_PRESS_0:
        case EVENT_KEY_PRESS_1:
        case EVENT_KEY_PRESS_2:
        case EVENT_KEY_PRESS_3:
        case EVENT_KEY_PRESS_4:
        case EVENT_KEY_PRESS_5:
        case EVENT_KEY_PRESS_6:
        case EVENT_KEY_PRESS_7:
        case EVENT_KEY_PRESS_8:
        case EVENT_KEY_PRESS_9: {
            Buzzer_LongBeep(2000, 250);
            int buttonValue = event - EVENT_KEY_PRESS_0; 

            if (selectedPan == 0) {
							((digitPosition == 2 && buttonValue > 6) || (left.timerDigits[2] == 6 && buttonValue !=0 && digitPosition==3) || (left.timerDigits[3] > 0 && buttonValue > 5 && digitPosition==2)) ? (int)0 : ((left.timerDigits[digitPosition] = buttonValue), digitPosition++);
								if(left.timerDigits[2] == 6) digitPosition++;
            } 
						else if (selectedPan == 1) {
              ((digitPosition == 2 && buttonValue >6) || (right.timerDigits[2] == 6 && buttonValue !=0 && digitPosition==3) || (right.timerDigits[3] > 0 && buttonValue > 5 && digitPosition==2)) ? (int)0 : ((right.timerDigits[digitPosition] = buttonValue), digitPosition++);
								if(right.timerDigits[2] == 6) digitPosition++;
            }
            if ((digitPosition > 3)) {
                digitPosition = 0; 
            }
            if (selectedPan == 0) {
                left.mins = left.timerDigits[0] * 10 + left.timerDigits[1];
                left.secs = left.timerDigits[2] * 10 + left.timerDigits[3];
                writeTimeToSevenSeg(left.mins, left.secs);
            } 
						else if (selectedPan == 1) {
                right.mins = right.timerDigits[0] * 10 + right.timerDigits[1];
                right.secs = right.timerDigits[2] * 10 + right.timerDigits[3];
                writeTimeToSevenSeg(right.mins, right.secs);
            }
            break;
        }

        case EVENT_TIMER_KEY:
						digitPosition = 0;
            if (selectedPan == 0) {
                left.mins = left.timerDigits[0] * 10 + left.timerDigits[1];
                left.secs = left.timerDigits[2] * 10 + left.timerDigits[3];

                if (left.mins == 0 && left.secs == 0) {
                    LEDC_setSingleRedLED(left.power);
										left.boost ? writeBoostLevel(selectedPan) : writePowerLevel(left.power, right.power);
                    LEDC_redLED(TIMER_KEY, false);
                    enterState(STATE_COOKING);
                } else {
										ntimer++;
                    enterState(STATE_COUNTDOWN);
                    left.timer = true;
										eventDelay();
                }
            } else if (selectedPan == 1) {
                right.mins = right.timerDigits[0] * 10 + right.timerDigits[1];
                right.secs = right.timerDigits[2] * 10 + right.timerDigits[3];

                if (right.mins == 0 && right.secs == 0) {
                    LEDC_setSingleRedLED(right.power);
                    right.boost ? writeBoostLevel(selectedPan) : writePowerLevel(left.power, right.power);
                    LEDC_redLED(TIMER_KEY, false);
                    enterState(STATE_COOKING);
                } else {
										ntimer++;
                    enterState(STATE_COUNTDOWN);
                    right.timer = true;
										eventDelay();
                }
            }
            break;
						
				case EVENT_ONOFF_KEY:
					LEDC_redLED(TIMER_KEY, false);
					digitPosition = 0;
					
					if (selectedPan == 0) {
								for(int i=0;i<4;i++)left.timerDigits[i]=0;
								LEDC_setSingleRedLED(left.power);
								left.boost ? writeBoostLevel(selectedPan) : writePowerLevel(left.power, right.power);
								enterState(STATE_COOKING);
            } else if (selectedPan == 1) {
								for(int i=0;i<4;i++)right.timerDigits[i]=0;
								LEDC_setSingleRedLED(right.power);
								right.boost ? writeBoostLevel(selectedPan) : writePowerLevel(left.power, right.power);
								enterState(STATE_COOKING);
            }
					break;
        default:
            break;
    }
}

void stateCountDownHandler(Event event) {
    switch (event) {
//        case EVENT_SYNC_KEY:
//						writePowerLevel(left.power, right.power);
//						selectedPan == 0 ? LEDC_setSingleRedLED(left.power) : LEDC_setSingleRedLED(right.power);
//            enterState(STATE_COOKING);
//            break;
				case EVENT_TIMER_KEY_LONG:
						buzz(3);
						if (selectedPan == 0) {
								left.mins = 0;
								left.secs = 0;
								left.timer = false;
								for (int i = 0; i < 4; i++) {
										left.timerDigits[i] = 0;
								}
								left.boost ? writeBoostLevel(selectedPan) : writePowerLevel(left.power, right.power);
								LEDC_setSingleRedLED(left.power);
								LEDC_redLED(TIMER_KEY, false);
						} else {
								right.mins = 0;
								right.secs = 0;
								right.timer = false;
								for (int i = 0; i < 4; i++) {
										right.timerDigits[i] = 0;
								}
								right.boost ? writeBoostLevel(selectedPan) : writePowerLevel(left.power, right.power);
								LEDC_redLED(TIMER_KEY, false);
								LEDC_setSingleRedLED(right.power);
						}
						enterState(STATE_COOKING);
						break;
        default:

            break;
    }
}

bool facttoggle = false;

void stateFactoryHandler(Event event) {
    switch (event) {
        case EVENT_ONOFF_KEY:			
            enterState(STATE_ONOFF);
			clearSevenSegDisplay();
            break;
				case EVENT_PAYGO_KEY:
						facttoggle ? (Buzzer_LongBeep(2000, 250),displayLast4DigitsOfSerial(), facttoggle = (bool)!facttoggle) : (Buzzer_LongBeep(2000, 250),writeATECToSevenSeg(),facttoggle = (bool)!facttoggle);
						break;
				case EVENT_KEY_PRESS_0_B:
					enterState(STATE_ONOFF);
					clearSevenSegDisplay();
				break;
				case EVENT_SYNC_KEY:
					Buzzer_LongBeep(2000, 250);
					writeSevenSegInt(nconn);
					break;
				case EVENT_KEY_PRESS_0:
					Buzzer_LongBeep(2000, 250);
					writeSevenSegInt(Emulated_EEPROM_Read_Cooking_Session_Num());
					break;
				case EVENT_TIMER_KEY:
					Buzzer_LongBeep(2000, 250);
					displayFirmwareVersion(globalPayload.devID.FW);
					break;
				case EVENT_L_PLATE_SELECT_KEY_LONG:
					Buzzer_LongBeep(2000, 250);
      			handlePaygoLED(LED_OFF, 'g');
						handleSyncLED(LED_OFF, 'g');
						clearSevenSegDisplay();
						LEDC_allRedLEDs(false);
					if (GSMreboot()) {
						TouchFunction_ResetFactoryLPanHold();
						NVIC_SystemReset();
					}
						writeATECToSevenSeg();
						ATECdispDelay();
						TouchFunction_ResetFactoryLPanHold();
						break;
				case EVENT_R_PLATE_SELECT_KEY_LONG: {
					u8 i;
					Buzzer_LongBeep(2000, 250);
					BFTM0_Delay_ms(250);
					Emulated_EEPROM_Erase_All_Cook_Sessions();
					params_reset_to_defaults();
					Emulated_EEPROM_Write_RuntimeParams();
					for (i = 0; i < 2; i++) {
						LEDC_allRedLEDs(true);
						BFTM0_Delay_ms(500);
						LEDC_allRedLEDs(false);
						BFTM0_Delay_ms(500);
					}
					NVIC_SystemReset();
					break;
				}
        default:
            break;
    }
}

void stateLockHandler(Event event) {
    switch (event) {
				case EVENT_LOCK_KEY_LONG:
						buzz(3);
						LEDC_redLED(LOCK_KEY, false);
						enterState(previousState);
						break;
//				case EVENT_ONOFF_KEY:
//						LEDC_redLED(LOCK_KEY, false);
//						enterState(previousState);
//						break;
        default:
            break;
    }
}

void stateSyncHandler(Event event) {	
    if (currentState != STATE_SYNC) {
			switch (event) {
					case EVENT_ONOFF_KEY:
							Buzzer_LongBeep(2000, 250);
							enterState(previousState);
							break;
					case EVENT_SYNC_KEY_LONG:
						Buzzer_LongBeep(2000, 250);
						gsmCfun0Then1();
						resetCGSNRetryCountOnGSMReset();
						resetEpochRetryStateOnGSMReset();
						mqtt_service_started = false;
						(void)gsmWaitForAtHandshake();
						handleConnect(CONNECT);
							handleSyncLED(LED_STEADY, 'g');
							enterState(previousState);
							break;
					case EVENT_SYNC_KEY:
	//            Buzzer_LongBeep(2000, 250);
	//            enterState(previousState);
	//            break;
					default:
							break;
			}
	}
}

void statePayGoHandler(Event event) {
	    switch (event) {
        case EVENT_PAYGO_KEY:
						writePowerLevel(0,0);
						handlePaygoLED(LED_OFF, 'r');
            enterState(STATE_IDLE);
            break;
				case EVENT_ONOFF_KEY:
						handlePaygoLED(LED_OFF, 'r');
						enterState(STATE_IDLE);
        default:
            break;
    }
}

void countdownTimeLeft(void) {
		static uint32_t last_bftm0_ct = 0;
		if (left.timer) {
        if ((bftm0_ct >= last_bftm0_ct + 1000) || (bftm0_ct < last_bftm0_ct)) {
            last_bftm0_ct = bftm0_ct;

            if (left.secs > 0) {
                left.secs--;
            } else if (left.mins > 0) {
                left.mins--;
                left.secs = 59;
            }
						(currentState == STATE_COUNTDOWN && selectedPan == 0) ? writeTimeToSevenSeg(left.mins, left.secs) : (void)0;
						
        if (!eventTriggered && click_bftm0 > 0) { 
            if ((bftm0_ct >= click_bftm0 + 5000) || (bftm0_ct < click_bftm0)) {
                eventTriggered = true;
								left.boost ? writeBoostLevel(selectedPan) : writePowerLevel(left.power, right.power);
								selectedPan == 0? LEDC_setSingleRedLED(left.power) : LEDC_setSingleRedLED(right.power);
								enterState(STATE_COOKING);
            }
        }
						
            if (left.mins == 0 && left.secs == 0) {
								printf("[SHUTOFF] Timer Expiration (LEFT): Stove shutoff triggered (timer expired, power was %d)\r\n", left.power);
								left.boost = false;
                left.timer = false; 
                buzz(4);
								left.power = 0;								
									I2C_Tx_Data[0] = eCooker.numberOfPlates == SINGLE_STOVE ? 3 : 1;
									I2C_Tx_Data[1] = 0;
							
				/**********************  CRC calculation for Double Stove Left /1 PAN ****************************************/
									CRC_calculation[0] = I2C_Tx_Data[0];
									CRC_calculation[1] = I2C_Tx_Data[1];
									CRC_calculation[2] = 0x00;		// temperature setting
	  							CRC_16_value = 	CRC_16(seed, CRC_calculation, sizeof(CRC_calculation));
											
									I2C_Tx_Data[3] = (CRC_16_value >> 8) & 0xFF; // Most significant byte
									I2C_Tx_Data[4] = CRC_16_value & 0xFF;        // Least significant byte	
				/*************************************************************************************************************/
							
								closeSession(&leftPanSession);
								writePowerLevel(left.power, right.power);
                for (int i = 0; i < 4; i++) left.timerDigits[i] = 0;
                digitPosition = 0;
								selectedPan == 0 ? LEDC_redLED(TIMER_KEY, false) : (void)0;
								selectedPan == 0? LEDC_setSingleRedLED(left.power) : LEDC_setSingleRedLED(right.power);
								deviceInteractionIdle();
								currentState == STATE_LOCK ? (void)0 : enterState(STATE_COOKING);
								
            }
        }
    }
}


void countdownTimeLeftRTC(void) {
    if (left.timer) {
        if (left.secs > 0) {
            left.secs--;
        } else if (left.mins > 0) {
            left.mins--;
            left.secs = 59;
        }

        if (currentState == STATE_COUNTDOWN && selectedPan == 0) {
            writeTimeToSevenSeg(left.mins, left.secs);
        }

        if (!eventTriggered && click_bftm0 > 0) {
            // click_bftm0 should now be stored as an RTC timestamp if needed
            static uint32_t click_counter = 0;
            click_counter++;

            if (click_counter >= 5) { // 5 seconds have passed
                eventTriggered = true;
                click_counter = 0;

                if (left.boost)
                    writeBoostLevel(selectedPan);
                else
                    writePowerLevel(left.power, right.power);

                if (selectedPan == 0)
                    LEDC_setSingleRedLED(left.power);
                else
                    LEDC_setSingleRedLED(right.power);

                enterState(STATE_COOKING);
            }
        }

        if (left.mins == 0 && left.secs == 0) {
            left.boost = false;
            left.timer = false;
            buzz(4);
            left.power = 0;

            I2C_Tx_Data[0] = eCooker.numberOfPlates == SINGLE_STOVE ? 3 : 1;
            I2C_Tx_Data[1] = 0;

            CRC_calculation[0] = I2C_Tx_Data[0];
            CRC_calculation[1] = I2C_Tx_Data[1];
            CRC_calculation[2] = 0x00;

            CRC_16_value = CRC_16(seed, CRC_calculation, sizeof(CRC_calculation));
            I2C_Tx_Data[3] = (CRC_16_value >> 8) & 0xFF;
            I2C_Tx_Data[4] = CRC_16_value & 0xFF;

            closeSession(&leftPanSession);
            writePowerLevel(left.power, right.power);

            for (int i = 0; i < 4; i++) left.timerDigits[i] = 0;
            digitPosition = 0;

            if (selectedPan == 0)
                LEDC_redLED(TIMER_KEY, false);

            if (selectedPan == 0)
                LEDC_setSingleRedLED(left.power);
            else
                LEDC_setSingleRedLED(right.power);

            deviceInteractionIdle();
            if (currentState != STATE_LOCK)
                enterState(STATE_COOKING);
        }
    }
}


void countdownTimeRight(void) {
		static uint32_t last_bftm0_ct = 0;
	
		    if (right.timer) {
        if ((bftm0_ct >= last_bftm0_ct + 1000) || (bftm0_ct < last_bftm0_ct)) {
            last_bftm0_ct = bftm0_ct;
            if (right.secs > 0) {
                right.secs--;
            } else if (right.mins > 0) {
                right.mins--;
                right.secs = 59;
            }
						(currentState == STATE_COUNTDOWN && selectedPan == 1) ? writeTimeToSevenSeg(right.mins, right.secs) : (void)0;
        
				if (!eventTriggered && click_bftm0 > 0) { 
            if ((bftm0_ct >= click_bftm0 + 5000) || (bftm0_ct < click_bftm0)) {
                eventTriggered = true;
								right.boost ? writeBoostLevel(selectedPan) : writePowerLevel(left.power, right.power);
								selectedPan == 0? LEDC_setSingleRedLED(left.power) : LEDC_setSingleRedLED(right.power);
								enterState(STATE_COOKING);
            }
        }
				
            if (right.mins == 0 && right.secs == 0) {
								printf("[SHUTOFF] Timer Expiration (RIGHT): Stove shutoff triggered (timer expired, power was %d)\r\n", right.power);
								right.boost = false;
                right.timer = false; 
                buzz(4);
								right.power = 0;
//change this to plateId 1 to have stove replacement
								I2C1_Tx_Data[0] = 1;							
							//I2C1_Tx_Data[0] = 2;
								I2C1_Tx_Data[1] = 0;
							
							
				/**********************  CRC calculation for Double Stove Right /2 PAN ****************************************/
						CRC_calculation[0] = I2C1_Tx_Data[0];
						CRC_calculation[1] = I2C1_Tx_Data[1];
						CRC_calculation[2] = 0x00;		// temperature setting
						CRC_16_value = 	CRC_16(seed, CRC_calculation, sizeof(CRC_calculation));
						I2C1_Tx_Data[3] = (CRC_16_value >> 8) & 0xFF; // Most significant byte
						I2C1_Tx_Data[4] = CRC_16_value & 0xFF;        // Least significant byte	
				/*************************************************************************************************************/
							
								closeSession(&rightPanSession); 
								writePowerLevel(left.power, right.power);
                for (int i = 0; i < 4; i++) right.timerDigits[i] = 0;
                digitPosition = 0;
								selectedPan == 1 ? LEDC_redLED(TIMER_KEY, false) : (void)0;
								selectedPan == 0? LEDC_setSingleRedLED(left.power) : LEDC_setSingleRedLED(right.power);
								deviceInteractionIdle();
								currentState == STATE_LOCK ? (void)0 : enterState(STATE_COOKING);
								
            }
        }
    }	
	
}


void countdownTimeRightRTC(void) {
    static uint32_t click_counter = 0;
    if (right.timer) {
        // Decrement the timer
        if (right.secs > 0) {
            right.secs--;
        } else if (right.mins > 0) {
            right.mins--;
            right.secs = 59;
        }

        // Update 7-segment display if applicable
        if (currentState == STATE_COUNTDOWN && selectedPan == 1) {
            writeTimeToSevenSeg(right.mins, right.secs);
        }

        // Event triggering after 5 seconds
        if (!eventTriggered && click_bftm0 > 0) {
            click_counter++;
            if (click_counter >= 5) {
                eventTriggered = true;
                click_counter = 0;

                if (right.boost)
                    writeBoostLevel(selectedPan);
                else
                    writePowerLevel(left.power, right.power);

                if (selectedPan == 0)
                    LEDC_setSingleRedLED(left.power);
                else
                    LEDC_setSingleRedLED(right.power);

                enterState(STATE_COOKING);
            }
        }

        // Timer expired
        if (right.mins == 0 && right.secs == 0) {
            right.boost = false;
            right.timer = false;
            buzz(4);
            right.power = 0;

            I2C1_Tx_Data[0] = 1;  // Or 2 if you're swapping stove IDs
            I2C1_Tx_Data[1] = 0;

            CRC_calculation[0] = I2C1_Tx_Data[0];
            CRC_calculation[1] = I2C1_Tx_Data[1];
            CRC_calculation[2] = 0x00;

            CRC_16_value = CRC_16(seed, CRC_calculation, sizeof(CRC_calculation));
            I2C1_Tx_Data[3] = (CRC_16_value >> 8) & 0xFF;
            I2C1_Tx_Data[4] = CRC_16_value & 0xFF;

            closeSession(&rightPanSession);
            writePowerLevel(left.power, right.power);

            for (int i = 0; i < 4; i++) right.timerDigits[i] = 0;
            digitPosition = 0;

            if (selectedPan == 1)
                LEDC_redLED(TIMER_KEY, false);

            if (selectedPan == 0)
                LEDC_setSingleRedLED(left.power);
            else
                LEDC_setSingleRedLED(right.power);

            deviceInteractionIdle();
            if (currentState != STATE_LOCK)
                enterState(STATE_COOKING);
        }
    }
}


void syncSwitch(void){
					if (!syncTriggered && sync_bftm0 > 0) { 
            if ((bftm0_ct >= sync_bftm0 + 2500) || (bftm0_ct < sync_bftm0)) {
							syncTriggered = true;
							Buzzer_LongBeep(2000, 250);
							(selectedPan == 0 && left.boost) || (selectedPan == 1 && right.boost) ? (writeBoostLevel(selectedPan)) : writePowerLevel(left.power,right.power);
							enterState(previousState);
            }
        }
}


void batlvlSwitch(void){
					if (!batlvlTriggered && batlvl_bftm0 > 0) { 
            if (((bftm0_ct >= batlvl_bftm0 + 5000) || (bftm0_ct < batlvl_bftm0)) && currentState == STATE_ONOFF) {
							batlvlTriggered = true;
							clearSevenSegDisplay();
            }
        }
}

void atecdispSwitch(void){
					if (!atecdispTriggered && atecdisp_bftm0 > 0) { 
            if ((bftm0_ct >= atecdisp_bftm0 + 4000) || (bftm0_ct < atecdisp_bftm0)) {
							atecdispTriggered = true;
							clearSevenSegDisplay();
            }
        }
}

void plateConfigSwitch(void){
					if (!plateConfigTriggered && plateConfig_bftm0 > 0 && currentState == STATE_ONOFF) {
						if(eCooker.numberOfPlates == SINGLE_STOVE || eCooker.numberOfPlates == DOUBLE_STOVE){
							if(isStoveI2cConnected() && !turnOff){
								plateConfigTriggered = true;
								if(currentState == STATE_ONOFF){
									sendToGSMAndPrintResponse(HT_USART1, "AT", "OK");
									globalPayload.stat.stvcnt++;
									deviceInteractionIdle();
									HTriggered = true;
									main_onT = getEpochTimeRTC();
									LEDC_redLED(POWER_KEY, true);
									writePowerLevel(left.power, right.power);
									eCooker.numberOfPlates == DOUBLE_STOVE ? (void)0 : LEDC_setSingleRedLED(left.power);
									eCooker.numberOfPlates == DOUBLE_STOVE ? enterState(STATE_IDLE) : (enterState(STATE_COOKING));
									EVENT_ON_STATUS = true;
									fwUpdate.success = false;
								} else {
									plateConfigTriggered = false;
								}
							}
						}
            }
}

void HdispSwitch(void){
					if (!HTriggered && H_bftm0 > 0) {
						if (eCooker.numberOfPlates > 0) {
							toggledisp ? handleHdisplay() : clearSevenSegDisplay();
						} else {
							clearSevenSegDisplay();
						}
            if ((bftm0_ct >= H_bftm0 + 60000) || (bftm0_ct < H_bftm0)) {
							HTriggered = true;
							clearSevenSegDisplay();
            }
        }
}


void disableHdispSwitch(void) {
		H_bftm0 = 0;
    clearSevenSegDisplay();
}
