#ifndef __LEDC_H
#define __LEDC_H

//-----------------------------------------------------------------------------
#include "ht32.h"
#include "BFTM0.h"
//-----------------------------------------------------------------------------

#define POWER_KEY 12 												
#define TIMER_KEY 13
#define LOCK_KEY  14
#define R_PLATE		15
#define BOOST			10
#define L_PLATE		11
#define PAYGO			1
#define SYNC			0
#define LED_BLINK_INTERVAL_MS 300


//-----------------------------------------------------------------------------

typedef enum {
    LED_OFF,
    LED_STEADY,
    LED_BLINK
} LEDState;

typedef struct {
    LEDState state;
    uint32_t lastToggleTime;
    bool isOn;
    uint8_t lednum;
    bool red;
    bool green;
    bool blue;
} LEDControl;

//-----------------------------------------------------------------------------

void LEDC_Configuration(void);
void LEDC_redLED(uint8_t ledId, bool on);
void LEDC_rgbLED (int ledId, bool red, bool green, bool blue);
void writeSevenSegAscii (char* segStr);
void writeSevenSegInt (uint16_t n);
void writeSevenSegDP (uint8_t dpx, bool on_off);
//void LEDC_test(void);
void writeTimeToSevenSeg(uint16_t minutes, uint16_t seconds);
void writeErrCodeToSevenSeg(uint8_t errCode);
//void writeDoneToSevenSeg(void);
void displayLast4DigitsOfSerial(void);
void handleSyncLED(LEDState state, char c);
void handlePaygoLED(LEDState state, char c);
void updateLEDs(void);
void writeATECToSevenSeg(void);
void clearSevenSegDisplay(void);
void writePowerLevel(uint8_t lp, uint8_t rp);
void LEDC_setSingleRedLED(uint8_t ledId);
void LEDC_turnOffAllRedLEDs(void);
void LEDC_allRedLEDs(bool on);
//void writePAYGToSevenSeg(void);
void writeSYNCToSevenSeg(void);
void writeBoostLevel(uint8_t pan);
////void runLoadingAnimation(void);
void runLoadingAnimationNonBlocking(void);
void runBlockingAnimation(void);
void runBlockingAnimationStart(void);
bool runBlockingAnimationTick(void);
extern volatile bool syncAnimActive;
void writeLineToSevenSeg(void);
void writeBatLevel(float batV);
void handleHdisplay(void);
void displayFirmwareVersion(const char *fwVersion);
extern int currentFrame;
extern uint32_t lastUpdateTime;
//-----------------------------------------------------------------------------
#endif
