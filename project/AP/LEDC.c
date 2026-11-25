#include <ctype.h>
#include "LEDC.h"
#include "EEPROM.h"
#include "eCooker.h"
#include "GSM.h"
#include "FSM.h"
#include "utils.h"
//-----------------------------------------------------------------------------
LEDControl syncLED = {LED_OFF, 0, false, SYNC, false, false, false};
LEDControl paygoLED = {LED_OFF, 0, false, PAYGO, false, false, false};

//-----------------------------------------------------------------------------
const u8 NumberChar[]={
  0x3F, // '0' 
  0x06, // '1' 
  0x5B, // '2'
  0x4F, // '3' 
  0x66, // '4' 
  0x6D, // '5' 
  0x7D, // '6' 
  0x27, // '7' 
  0x7F, // '8' 
  0x6F, // '9'
  0x80  // ':'
};

const u8 LetterChar[]={
    0x5E, //'D' 0 
    0x3F, //'O' 1
    0x37, //'N' 2
    0x79, //'E' 3
		0x50, //'R' 4
		0x77, //'A' 5
    0x78, //'T' 6
    0x39, //'C' 7
		0x73, //'P' 8
		0x6E, //'Y' 9
		0x3D, //'G' 10
		0x6D, //'S' 11
		0x76, //'H' 12
};

//-----------------------------------------------------------------------------

typedef struct {
	uint8_t comm;
	uint8_t segBit;
} redLED_t;

typedef struct {
	uint8_t comm;
	uint8_t segBitRED;
	uint8_t segBitGREEN;
	uint8_t segBitBLUE;
} rgbLED_t;

//-----------------------------------------------------------------------------
redLED_t redLED [] = {
		{  4, 0}, // Power button 0  COM4 SEG0
		{  4, 1}, // Power button 1  COM4 SEG1
		{  4, 2}, // Power button 2  COM4 SEG2
		{  4, 3}, // Power button 3  COM4 SEG3
		{  4, 4}, // Power button 4  COM4 SEG4
		{  4, 5}, // Power button 5  COM4 SEG5
		{  4, 7}, // Power button 6  COM4 SEG7  !!
		{  4, 6}, // Power button 7  COM4 SEG6  !!
		{  6, 6}, // Power button 8  COM6 SEG6
		{  6, 1}, // Power button 9  COM6 SEG1
		{  6, 5}, // Power button Boost COM6 SEG5
		{  5, 0}, // select left button COM5 SEG0
		{  5, 1}, // on-off button COM5 SEG1
		{  5, 2}, // timer button  COM5 SEG2
		{  5, 3}, // lock button   COM5 SEG3
		{  5, 4}, // select right button COM5 SEG4
};
	
rgbLED_t rgbLED []= {
	{ 5, 5, 6, 7},
	{ 6, 2, 3, 4}
};
	
#define DIGIT1COM 9
#define DIGIT2COM 8
#define DIGIT3COM 3
#define DIGIT4COM 2

const uint8_t sevenSegComms[] = {9,8,3,2};
	
//-----------------------------------------------------------------------------
void LEDC_Configuration(void)
{	//functionStatus = 145;
  LEDC_InitTypeDef LEDC_InitStruct;
  CKCU_PeripClockConfig_TypeDef CKCUClock = {{ 0 }};

  /* Enable LED & AFIO APB clock     */   
  CKCUClock.Bit.AFIO             = 1;
  CKCUClock.Bit.LEDC             = 1;
  CKCU_PeripClockConfig(CKCUClock, ENABLE);
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_0, AFIO_FUN_LEDC);  // LED_SEG0  Config AFIO mode
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_1, AFIO_FUN_LEDC);  // LED_SEG1  Config AFIO mode
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_2, AFIO_FUN_LEDC);  // LED_SEG2  Config AFIO mode
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_3, AFIO_FUN_LEDC);  // LED_SEG3  Config AFIO mode
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_4, AFIO_FUN_LEDC);  // LED_SEG4  Config AFIO mode
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_5, AFIO_FUN_LEDC);  // LED_SEG5  Config AFIO mode
  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_6, AFIO_FUN_LEDC);  // LED_SEG6  Config AFIO mode
//  AFIO_GPxConfig(GPIO_PA, AFIO_PIN_7, AFIO_FUN_LEDC);  // LED_SEG7  Config AFIO mode
	AFIO_GPxConfig(GPIO_PD, AFIO_PIN_3, AFIO_FUN_LEDC);  // LED_SEG7  Config AFIO mode        -> changed from 1.0.7 to PD3

  AFIO_GPxConfig(GPIO_PB, AFIO_PIN_4, AFIO_FUN_LEDC);  // LED_COM2  Config AFIO mode
  AFIO_GPxConfig(GPIO_PB, AFIO_PIN_5, AFIO_FUN_LEDC);  // LED_COM3  Config AFIO mode
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_14, AFIO_FUN_LEDC);  // LED_COM8  Config AFIO mode
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_15, AFIO_FUN_LEDC);  // LED_COM9  Config AFIO mode
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_1, AFIO_FUN_LEDC);  // LED_COM4  Config AFIO mode
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_2, AFIO_FUN_LEDC);  // LED_COM5  Config AFIO mode
  AFIO_GPxConfig(GPIO_PC, AFIO_PIN_3, AFIO_FUN_LEDC);  // LED_COM6  Config AFIO mode

  LEDC_InitStruct.LEDC_ClockSource    = LEDC_SRC_LSI;  // LEDC_SRC_PCLK;
  LEDC_InitStruct.LEDC_DutyClockNumber= LEDC_DTYNUM_8;
  LEDC_InitStruct.LEDC_ClockPrescaler = 2;
  LEDC_InitStruct.LEDC_DeadTime       = 0;
  LEDC_InitStruct.LEDC_COMxEN = LEDC_COM2EN | LEDC_COM3EN | LEDC_COM8EN | LEDC_COM9EN | LEDC_COM4EN | LEDC_COM5EN | LEDC_COM6EN;
  LEDC_Init(&LEDC_InitStruct);

  LEDC_SetPolarityMode(LEDC_COM2POL | LEDC_COM3POL | LEDC_COM8POL | LEDC_COM9POL | LEDC_COM4POL | LEDC_COM5POL | LEDC_COM6POL, \
    LEDC_SEG0POL | LEDC_SEG1POL | LEDC_SEG2POL | LEDC_SEG3POL | LEDC_SEG4POL | LEDC_SEG5POL | LEDC_SEG6POL | LEDC_SEG7POL, \
    COMMON_ANODE_WITH_PNP );

  LEDC_IntConfig(ENABLE);    /* Enable frame interrupt. */
  NVIC_EnableIRQ(LEDC_IRQn); /* Enable LECD IRQ. */
  LEDC_Cmd(ENABLE);          /* LEDC Start. */
}

//-----------------------------------------------------------------------------
void LEDC_IRQHandler(void)
{	//functionStatus = 146;
  if (LEDC_GetFlagStatus())
    LEDC_ClearFlagStatus();

  //Add your code
}
//-----------------------------------------------------------------------------
void LEDC_redLED(uint8_t ledId, bool on) {
		//functionStatus = 147;
	redLED_t *thisRedLED;
	int16_t currentSegs;
	if (ledId > 15) return;
	thisRedLED = redLED + ledId;
	currentSegs = HT_LEDC->DR[thisRedLED->comm];
	if (on) {
		currentSegs |= 1<<thisRedLED->segBit;
	}
	else {
		currentSegs &= ~(1<<thisRedLED->segBit);
	}	
	HT_LEDC->DR[thisRedLED->comm] = currentSegs;
}

void LEDC_setSingleRedLED(uint8_t ledId) {
		//functionStatus = 148;
    if (ledId > 10) return;  // Ensure ledId is within 0-9 range

    redLED_t *thisRedLED;
    int16_t currentSegs;

    // Loop through the first 10 LEDs (0-9)
    for (uint8_t i = 0; i <= 10; i++) {
        thisRedLED = redLED + i;
        currentSegs = HT_LEDC->DR[thisRedLED->comm];

        if (i <= ledId) {
            // Turn on LEDs up to the specified index
            currentSegs |= (1 << thisRedLED->segBit);
        } else {
            // Turn off LEDs beyond the specified index
            currentSegs &= ~(1 << thisRedLED->segBit);
        }

        // Update the display register
        HT_LEDC->DR[thisRedLED->comm] = currentSegs;
    }
}


void LEDC_turnOffAllRedLEDs(void) {
		//functionStatus = 149;
    redLED_t *thisRedLED;
    int16_t currentSegs;

    // Loop through all 16 LEDs (0-15)
    for (uint8_t i = 0; i <= 10; i++) {
        thisRedLED = redLED + i;
        currentSegs = HT_LEDC->DR[thisRedLED->comm];

        // Turn off the LED by clearing the specific bit
        currentSegs &= ~(1 << thisRedLED->segBit);

        // Update the display register
        HT_LEDC->DR[thisRedLED->comm] = currentSegs;
    }
}

void LEDC_allRedLEDs(bool on)
{
    uint8_t i;
    for (i = 0; i <= 15; i++) {
        LEDC_redLED(i, on);
    }
    LEDC_rgbLED(SYNC, on, false, false);
    LEDC_rgbLED(PAYGO, on, false, false);
}

void LEDC_rgbLED (int ledId, bool red, bool green, bool blue) {
		//functionStatus = 150;
	rgbLED_t *thisRGBLED;
	int16_t currentSegs;
	if ((ledId != 0) && (ledId != 1)) return;
	thisRGBLED = rgbLED + ledId;
	currentSegs = HT_LEDC->DR[thisRGBLED->comm];
	if (red) {
		currentSegs |= 1<<thisRGBLED->segBitRED;
	}
	else {
		currentSegs &= ~(1<<thisRGBLED->segBitRED);
	}	
	if (red) {
		currentSegs |= 1<<thisRGBLED->segBitRED;
	}
	else {
		currentSegs &= ~(1<<thisRGBLED->segBitRED);
	}	
	if (green) {
		currentSegs |= 1<<thisRGBLED->segBitGREEN;
	}
	else {
		currentSegs &= ~(1<<thisRGBLED->segBitGREEN);
	}	
	if (blue) {
		currentSegs |= 1<<thisRGBLED->segBitBLUE;
	}
	else {
		currentSegs &= ~(1<<thisRGBLED->segBitBLUE);
	}	
	HT_LEDC->DR[thisRGBLED->comm] = currentSegs;
}
//-----------------------------------------------------------------------------

void writeBatLevel(float batV){
		//functionStatus = 151;
//	float perc = (batV/3.5)*100;
		float V_min = 2.6;  // Minimum battery voltage
    float V_max = 3.5;  // Maximum battery voltage
    float perc;

    // Ensure batV is within the valid range
    if (batV < V_min) {
        perc = 0.0;  // Battery voltage is below minimum, set percentage to 0%
    } else if (batV > V_max) {
        perc = 100.0;  // Battery voltage is above maximum, set percentage to 100%
    } else {
        // Calculate battery percentage
        perc = ((batV - V_min) / (V_max - V_min)) * 100;
    }
	
	if(perc<=100 && perc>=75){
		LEDC_SetData(sevenSegComms[0], NumberChar[0]); 
    LEDC_SetData(sevenSegComms[1], NumberChar[0]); 
    LEDC_SetData(sevenSegComms[2], NumberChar[0]);
    LEDC_SetData(sevenSegComms[3], NumberChar[0]); 
	}
	
	else if(perc<=75 && perc>=50){
		LEDC_SetData(sevenSegComms[0], 0x00); 
    LEDC_SetData(sevenSegComms[1], NumberChar[0]); 
    LEDC_SetData(sevenSegComms[2], NumberChar[0]);
    LEDC_SetData(sevenSegComms[3], NumberChar[0]); 		
	}
	
	else if(perc<=50 && perc>=25){
		LEDC_SetData(sevenSegComms[0], 0x00); 
    LEDC_SetData(sevenSegComms[1], 0x00); 
    LEDC_SetData(sevenSegComms[2], NumberChar[0]);
    LEDC_SetData(sevenSegComms[3], NumberChar[0]); 		
	}
	
	else if(perc<=25 && perc>=0){
		LEDC_SetData(sevenSegComms[0], 0x00); 
    LEDC_SetData(sevenSegComms[1], 0x00); 
    LEDC_SetData(sevenSegComms[2], 0x00);
    LEDC_SetData(sevenSegComms[3], NumberChar[0]); 		
	}
	else{
    LEDC_SetData(sevenSegComms[0], LetterChar[3]); // 'E'
    LEDC_SetData(sevenSegComms[1], LetterChar[4]); // 'R'
	  LEDC_SetData(sevenSegComms[2], LetterChar[4]); // 'R'
		LEDC_SetData(sevenSegComms[1], 0x00);
	}
}

void writeSevenSegAscii (char* segStr) {
		//functionStatus = 152;
	int i;
	int j=0;
	for (i=0; segStr[i];i++) {
		if (isdigit(segStr[i])) {
			LEDC_SetData (sevenSegComms[j++],NumberChar[segStr[i]-'0']);
		}
		if (j>3) break;
	}
}
void clearSevenSegDisplay(void) {
		//functionStatus = 153;
    for (int i = 0; i < 4; i++) {
        LEDC_SetData(sevenSegComms[i], 0x00);  // Set all segments of the digit off
    }
}
void writeSevenSegInt (uint16_t n) {
		//functionStatus = 154;
	int i;
	int j=1000;
	int k;
	for (i=0; i<4;i++) {
		k=n/j;
		if (k>9) break;
		LEDC_SetData (sevenSegComms[i],NumberChar[k]);
		n-=(k*j);
		j/=10;
	}
}

//void writeSevenSegDP (uint8_t dpx, bool on_off) {
//	uint8_t segs;
//	segs = LEDC_GetData(sevenSegComms[dpx]);
//	if (on_off) { //switch dp on
//		if ((segs & 0x80) == 0) {
//			segs |= 0x80;
//			LEDC_SetData(sevenSegComms[dpx], segs);
//		}
//	}
//	else { // switch dp off
//		if (segs & 0x80) {
//			segs &= 0x7F;
//			LEDC_SetData(sevenSegComms[dpx], segs);	
//		}
//	}
//	

//}

u8 loadingFrames[13][4] = {
{ 0x18, 0x00, 0x00, 0x00 },  // Frame 0
{ 0x30, 0x00, 0x00, 0x00 },  // Frame 1
{ 0x21, 0x00, 0x00, 0x00 },  // Frame 2
{ 0x01, 0x01, 0x00, 0x00 },  // Frame 3
{ 0x00, 0x01, 0x01, 0x00 },  // Frame 4
{ 0x00, 0x00, 0x01, 0x01 },  // Frame 5
{ 0x00, 0x00, 0x00, 0x03 },  // Frame 6
{ 0x00, 0x00, 0x00, 0x06 },  // Frame 7
{ 0x00, 0x00, 0x00, 0x0c },  // Frame 8
{ 0x00, 0x00, 0x08, 0x08 },  // Frame 9
{ 0x00, 0x08, 0x08, 0x00 },  // Frame 10
{ 0x08, 0x08, 0x00, 0x00 },  // Frame 11
{ 0x18, 0x00, 0x00, 0x00 },  // Frame 12
};

bool runsyncflag = false;

//void runLoadingAnimation() {
//    int totalFrames = 13;
//    uint32_t startTime = bftm0_ct;  // Capture the start time
//    
//    // Loop to run the animation for 5 seconds
//    while (bftm0_ct - startTime < 5000) {  // 5000 ms = 5 seconds
//        for (int i = 0; i < totalFrames; i++) {
//            // Show current loading frame on the 7-segment display
//            for (int j = 0; j < 4; j++) {
//                LEDC_SetData(sevenSegComms[j], loadingFrames[i][j]);
//            }

//            // Wait for a frame duration (e.g., wait until bftm0_ct reaches the next multiple of 150)
//            while ((bftm0_ct % 150 != 0) || (last_bftm0_ct == bftm0_ct)) {
//            }
//						
//            // Update the last_bftm0_ct for the next frame
//            last_bftm0_ct = bftm0_ct; 
//        }
//    }

//    // After the animation ends, reset the display
//    writeSYNCToSevenSeg();
//}

void writeLineToSevenSeg(void){
		//functionStatus = 157;
    if (currentState == STATE_FACTORY) {
        return;
    }
	  LEDC_SetData(sevenSegComms[0], 0x40); 
    LEDC_SetData(sevenSegComms[1], 0x40); 
    LEDC_SetData(sevenSegComms[2], 0x40);
    LEDC_SetData(sevenSegComms[3], 0x40); 
}

/* --- "Sync connected" animation: non-blocking state machine ------------- */
static const uint8_t syncAnimFrames[][4] = {
    { 0x6d, 0x6e, 0x37, 0x39 },
    { 0x6e, 0x37, 0x39, 0x00 },
    { 0x37, 0x39, 0x00, 0x5e },
    { 0x39, 0x00, 0x5e, 0x3f },
    { 0x00, 0x5e, 0x3f, 0x37 },
    { 0x5e, 0x3f, 0x37, 0x79 },
};
#define SYNC_ANIM_TOTAL_FRAMES 6
#define SYNC_ANIM_LOOPS        2
#define SYNC_ANIM_FRAME_MS     250

static int      syncAnim_frame = 0;
static int      syncAnim_loop  = 0;
static uint32_t syncAnim_lastTime = 0;
volatile bool   syncAnimActive = false;

void runBlockingAnimationStart(void) {
    syncAnim_frame = 0;
    syncAnim_loop  = 0;
    syncAnim_lastTime = bftm0_ct;
    syncAnimActive = true;
    for (int j = 0; j < 4; j++)
        LEDC_SetData(sevenSegComms[j], syncAnimFrames[0][j]);
}

bool runBlockingAnimationTick(void) {
    if (!syncAnimActive)
        return true;
    if ((bftm0_ct - syncAnim_lastTime) < SYNC_ANIM_FRAME_MS)
        return false;

    syncAnim_lastTime = bftm0_ct;
    syncAnim_frame++;
    if (syncAnim_frame >= SYNC_ANIM_TOTAL_FRAMES) {
        syncAnim_frame = 0;
        syncAnim_loop++;
        if (syncAnim_loop >= SYNC_ANIM_LOOPS) {
            syncAnimActive = false;
            return true;
        }
    }
    for (int j = 0; j < 4; j++)
        LEDC_SetData(sevenSegComms[j], syncAnimFrames[syncAnim_frame][j]);
    return false;
}

void runBlockingAnimation(void) {
    runBlockingAnimationStart();
    while (!runBlockingAnimationTick()) {
        extern void GPTM0_ProcessEvents(void);
        GPTM0_ProcessEvents();
    }
}

int currentFrame = 0;
uint32_t lastUpdateTime = 0;

void runLoadingAnimationNonBlocking(void) {
		//functionStatus = 159;
    int totalFrames = 13;
    uint32_t currentTime = bftm0_ct;

    // Progress the animation frame if enough time has passed
    if (currentTime - lastUpdateTime >= 150) {  // 150 ms per frame
        // Update the display with the current frame
        for (int j = 0; j < 4; j++) {
            LEDC_SetData(sevenSegComms[j], loadingFrames[currentFrame][j]);
        }

        // Move to the next frame
        currentFrame = (currentFrame + 1) % totalFrames;

        // Update the last frame update time
        lastUpdateTime = currentTime;
    }
}

void writeTimeToSevenSeg(uint16_t minutes, uint16_t seconds) {
		//functionStatus = 160;
    int digits[4];
    digits[0] = minutes / 10;
    digits[1] = minutes % 10;
    digits[2] = seconds / 10;
    digits[3] = seconds % 10;

    for (int i = 0; i < 4; i++) {
        LEDC_SetData(sevenSegComms[i], NumberChar[digits[i]]);
    }

    LEDC_SetData(sevenSegComms[1], NumberChar[digits[1]] | 0x80);
}

//void writeDoneToSevenSeg(void) {
//    LEDC_SetData(sevenSegComms[0], LetterChar[0]); // 'D'
//    LEDC_SetData(sevenSegComms[1], LetterChar[1]); // 'O'
//    LEDC_SetData(sevenSegComms[2], LetterChar[2]); // 'N'
//    LEDC_SetData(sevenSegComms[3], LetterChar[3]); // 'E'
//}


void writeATECToSevenSeg(void) {
		//functionStatus = 162;
    LEDC_SetData(sevenSegComms[0], LetterChar[5]); // 'A'
    LEDC_SetData(sevenSegComms[1], LetterChar[6]); // 'T'
    LEDC_SetData(sevenSegComms[2], LetterChar[3]); // 'E'
    LEDC_SetData(sevenSegComms[3], LetterChar[7]); // 'C'
}

void writeSYNCToSevenSeg(void) {
		//functionStatus = 163;
    LEDC_SetData(sevenSegComms[0], LetterChar[11]);  // 'S'
    LEDC_SetData(sevenSegComms[1], LetterChar[9]);   // 'Y'
    LEDC_SetData(sevenSegComms[2], LetterChar[2]);   // 'N'
    LEDC_SetData(sevenSegComms[3], LetterChar[7]);   // 'C'
}


//void writePAYGToSevenSeg(void) {
//    LEDC_SetData(sevenSegComms[0], LetterChar[8]);  // 'P'
//    LEDC_SetData(sevenSegComms[1], LetterChar[5]);  // 'A'
//    LEDC_SetData(sevenSegComms[2], LetterChar[9]);  // 'Y'
//    LEDC_SetData(sevenSegComms[3], LetterChar[10]); // 'G'
//}


void writeErrCodeToSevenSeg(uint8_t errCode) {
		//functionStatus = 165;
    LEDC_SetData(sevenSegComms[0],  0x00); 
    LEDC_SetData(sevenSegComms[1], LetterChar[3]); // 'E'
		LEDC_SetData(sevenSegComms[2], NumberChar[errCode]); 
	  LEDC_SetData(sevenSegComms[3], 0x00); 
}

void writePowerLevel(uint8_t lp, uint8_t rp){
			if (left.error) {
				LEDC_SetData(sevenSegComms[0], LetterChar[3]);
				LEDC_SetData(sevenSegComms[1], 0x00);
			} else if (lp == BOOST) {
				LEDC_SetData(sevenSegComms[0], NumberChar[1]);
				LEDC_SetData(sevenSegComms[1], NumberChar[0]);
			} else if (rp == BOOST) {
				LEDC_SetData(sevenSegComms[0], 0x00);
				LEDC_SetData(sevenSegComms[1], 0x00);
			} else {
				LEDC_SetData(sevenSegComms[0], NumberChar[lp]);
				LEDC_SetData(sevenSegComms[1], 0x00);
			}

			if (eCooker.numberOfPlates == 2) {
				if (right.error) {
					LEDC_SetData(sevenSegComms[2], 0x00);
					LEDC_SetData(sevenSegComms[3], LetterChar[3]);
				} else if (rp == BOOST) {
					LEDC_SetData(sevenSegComms[2], NumberChar[1]);
					LEDC_SetData(sevenSegComms[3], NumberChar[0]);
				} else if (lp == BOOST) {
					LEDC_SetData(sevenSegComms[2], 0x00);
					LEDC_SetData(sevenSegComms[3], 0x00);
				} else {
					LEDC_SetData(sevenSegComms[2], 0x00);
					LEDC_SetData(sevenSegComms[3], NumberChar[rp]);
				}
			} else {
				LEDC_SetData(sevenSegComms[2], 0x00);
				LEDC_SetData(sevenSegComms[3], 0x00);
			}
}

void writeBoostLevel(uint8_t pan){
		//functionStatus = 167;
	LEDC_setSingleRedLED(10);
	switch(pan){
		case 0:
			LEDC_SetData(sevenSegComms[0], NumberChar[1]);
			LEDC_SetData(sevenSegComms[1], NumberChar[0]);
			LEDC_SetData(sevenSegComms[2], 0x00);
			LEDC_SetData(sevenSegComms[3], 0x00);
			break;
		
		case 1:
			LEDC_SetData(sevenSegComms[0], 0x00);
			LEDC_SetData(sevenSegComms[1], 0x00);
			LEDC_SetData(sevenSegComms[2], NumberChar[1]);
			LEDC_SetData(sevenSegComms[3], NumberChar[0]);		
	}
			
}

void displayFirmwareVersion(const char *fwVersion) {
    char digits[5] = {0};
    int count = 0;

    for (int i = 0; fwVersion[i] != '\0' && count < 4; i++) {
        if (isdigit((unsigned char)fwVersion[i])) {
            digits[count++] = fwVersion[i];
        }
    }

    if (count >= 3) {
        writeSevenSegInt(atoi(digits));
    } else {
        writeSevenSegInt(0000);
    }
}

void displayLast4DigitsOfSerial(void) {
		//functionStatus = 168;
    u32 serial_id = Emulated_EEPROM_Read_Serial_ID();
    uint16_t last4digits = serial_id % 10000; // Get the last 4 digits of the serial number

    // Display the last 4 digits on the 7-segment display
    writeSevenSegInt(last4digits);
}

//-----------------------------------------------------------------------------
//void LEDC_test(void)
//{
//	static int count = 0;
//	if (count == 0) {
//#if 1		
//		LEDC_SetData(DIGIT1COM, NumberChar[0]);
//    LEDC_SetData(DIGIT2COM, NumberChar[1]);
//    LEDC_SetData(DIGIT3COM, NumberChar[2]);
//    LEDC_SetData(DIGIT4COM, NumberChar[3]);
//#else
//		 LEDC_SetData(DIGIT1COM, 0x00);
//		 LEDC_SetData(DIGIT2COM, 0x00);
//		 LEDC_SetData(DIGIT3COM, 0x00);
//		 LEDC_SetData(DIGIT4COM, 0x00);
//#endif
//		LEDC_SetData(4, 0x01);
//		LEDC_SetData(5, 0x01);
//		LEDC_SetData(6, 0x01);

//	}
//	else if (( count % 20) == 0) {
//	}
//}
//-----------------------------------------------------------------------------

void setLEDState(LEDControl *led, LEDState state, bool red, bool green, bool blue) {
		//functionStatus = 170;
    led->state = state;
    led->red = red;
    led->green = green;
    led->blue = blue;

    if (state == LED_OFF) {
        led->isOn = false;
        LEDC_rgbLED(led->lednum, false, false, false); 
    } else if (state == LED_STEADY) {
        led->isOn = true;
        LEDC_rgbLED(led->lednum, red, green, blue); 
    } else if (state == LED_BLINK) {
        led->lastToggleTime = bftm0_ct;  
        led->isOn = false; 
        LEDC_rgbLED(led->lednum, red, green, blue); 
    }
}

void setRedLEDState(LEDControl *led, LEDState state) {
		//functionStatus = 171;
    led->state = state;

    if (state == LED_OFF) {
        led->isOn = false;
        LEDC_redLED(led->lednum, false);  
    } else if (state == LED_STEADY) {
        led->isOn = true;
        LEDC_redLED(led->lednum, true);  
    } else if (state == LED_BLINK) {
        led->lastToggleTime = bftm0_ct; 
        led->isOn = false;  
        LEDC_redLED(led->lednum, true); 
    }
}


void updateLEDs(void) {
		//functionStatus = 172;
    uint32_t currentTime = bftm0_ct;

    if (syncLED.state == LED_BLINK) {
        if (currentTime - syncLED.lastToggleTime >= LED_BLINK_INTERVAL_MS) {
            syncLED.lastToggleTime = currentTime;
            syncLED.isOn = (bool)!syncLED.isOn;
            LEDC_rgbLED(syncLED.lednum, syncLED.isOn ? syncLED.red : false, syncLED.isOn ? syncLED.green : false, syncLED.isOn ? syncLED.blue : false);
        }
    } else if (syncLED.state == LED_STEADY) {
        LEDC_rgbLED(syncLED.lednum, syncLED.red, syncLED.green, syncLED.blue);
    }

    if (paygoLED.state == LED_BLINK) {
        if (currentTime - paygoLED.lastToggleTime >= LED_BLINK_INTERVAL_MS) {
            paygoLED.lastToggleTime = currentTime;
            paygoLED.isOn = (bool)!paygoLED.isOn;
            LEDC_rgbLED(paygoLED.lednum, paygoLED.isOn ? paygoLED.red : false, paygoLED.isOn ? paygoLED.green : false, paygoLED.isOn ? paygoLED.blue : false);
        }
    } else if (paygoLED.state == LED_STEADY) {
        LEDC_rgbLED(paygoLED.lednum, paygoLED.red, paygoLED.green, paygoLED.blue);
    }
}


void handleSyncLED(LEDState state, char c) {
		//functionStatus = 173;
    bool red = false, green = false, blue = false;

    switch(c) {
        case 'r': red = true; break;
        case 'g': green = true; break;
        case 'b': blue = true; break;
    }

    setLEDState(&syncLED, state, red, green, blue);
}

void handlePaygoLED(LEDState state, char c) {
		//functionStatus = 174;
    bool red = false, green = false, blue = false;

    switch(c) {
        case 'r': red = true; break;
        case 'g': green = true; break;
        case 'b': blue = true; break;
    }

    setLEDState(&paygoLED, state, red, green, blue);
}

void handleHdisplay(void){
		//functionStatus = 175;
	  LEDC_SetData(sevenSegComms[0], LetterChar[12]);
    LEDC_SetData(sevenSegComms[1], 0x00);
    LEDC_SetData(sevenSegComms[2], 0X00);
		(eCooker.numberOfPlates == 2) ? LEDC_SetData(sevenSegComms[3], LetterChar[12]) : (LEDC_SetData(sevenSegComms[2], 0X00));
}
