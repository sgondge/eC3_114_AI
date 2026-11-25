#ifndef __LEDC_H
#define __LEDC_H

//-----------------------------------------------------------------------------
#include "ht32.h"
#include "BFTM0.h"
//-----------------------------------------------------------------------------

extern const u8 NumberChar[];
extern const u8 LetterChar[];

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

//typedef enum {
//    LED_OFF,
//    LED_STEADY,
//    LED_BLINK
//} LEDState;

//typedef struct {
//    LEDState state;
//    uint32_t lastToggleTime;
//    bool isOn;
//    uint8_t lednum;
//    bool red;
//    bool green;
//    bool blue;
//} LEDControl;

//-----------------------------------------------------------------------------

void LEDC_Configuration(void);
void LEDC_redLED(uint8_t ledId, bool on);
void LEDC_rgbLED (int ledId, bool red, bool green, bool blue);



void writeATECToSevenSeg(void);
void clearSevenSegDisplay(void);
void runLoadingAnimationNonBlocking(void);

void writeLineToSevenSeg(void);
void LEDC_test(void);

//-----------------------------------------------------------------------------
#endif
