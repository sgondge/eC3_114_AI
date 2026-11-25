#include <ctype.h>
#include "LEDC.h"

//-----------------------------------------------------------------------------
//LEDControl syncLED = {LED_OFF, 0, false, SYNC, false, false, false};
//LEDControl paygoLED = {LED_OFF, 0, false, PAYGO, false, false, false};

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

void LEDC_rgbLED (int ledId, bool red, bool green, bool blue) {
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

void writeLineToSevenSeg(void){
	  LEDC_SetData(sevenSegComms[0], 0x40); 
    LEDC_SetData(sevenSegComms[1], 0x40); 
    LEDC_SetData(sevenSegComms[2], 0x40);
    LEDC_SetData(sevenSegComms[3], 0x40); 
}

int currentFrame = 0;
uint32_t lastUpdateTime = 0;

void runLoadingAnimationNonBlocking(void) {
    int totalFrames = 13;
    uint32_t currentTime = bftm0_ct;

    // Progress the animation frame if enough time has passed
    if (currentTime - lastUpdateTime >= 150) {  // 150 ms per frame
        // Update the display with the current frame
        for (int j = 0; j < 4; j++) {
            LEDC_SetData(sevenSegComms[j], loadingFrames[currentFrame][j]);
        }

        currentFrame = (currentFrame + 1) % totalFrames;

        lastUpdateTime = currentTime;
    }
}

void writeATECToSevenSeg(void) {
    LEDC_SetData(sevenSegComms[0], LetterChar[5]); // 'A'
    LEDC_SetData(sevenSegComms[1], LetterChar[6]); // 'T'
    LEDC_SetData(sevenSegComms[2], LetterChar[3]); // 'E'
    LEDC_SetData(sevenSegComms[3], LetterChar[7]); // 'C'
}

//void displayFirmwareVersion(const char *fwVersion) {
//    char digits[5] = {0};
//    int count = 0;

//    for (int i = 0; fwVersion[i] != '\0' && count < 4; i++) {
//        if (isdigit((unsigned char)fwVersion[i])) {
//            digits[count++] = fwVersion[i];
//        }
//    }
//    if (count >= 3) {
//        writeSevenSegInt(atoi(digits));
//    } else {
//        writeSevenSegInt(0000);
//    }
//}

//void displayLast4DigitsOfSerial(void) {
//		//functionStatus = 168;
//    u32 serial_id = Emulated_EEPROM_Read_Serial_ID();
//    uint16_t last4digits = serial_id % 10000; // Get the last 4 digits of the serial number
//    // Display the last 4 digits on the 7-segment display
//    writeSevenSegInt(last4digits);
//}

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

/* CREATE NEW LED BLINKING CODE HERE*/