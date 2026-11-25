#include "ht32.h"
#include "GPIO.h"
#include "BFTM0.h"
#include "UART0.h"
#include "USART1.h"
#include "LEDC.h"
#include "I2C0.h"
#include "I2C1.h"
#include <gsm.h>
//#include "PBCI.h"
#include <string.h>
#include "ht32_touchkey.h"
#include "eCooker.h"
#include "EEPROM.h"
#include "Payload.h"
#include "Utils.h"
#include "RTC.h"

// Define maximum number of arguments
#define MAX_ARGS 12
#define MAX_ARG_LENGTH 20

//void TK_Configure(void);
// Function prototypes
void led_command(HT_USART_TypeDef *thisUsart, int commandIndex, int num_args, char *args[]);
void gsm_command(HT_USART_TypeDef *thisUsart, int commandIndex, int num_args, char *args[]);
void my_fprintf(HT_USART_TypeDef *thisUsart, const char *format, ...);
void my_fputs(const char *str, HT_USART_TypeDef *usart);


//uint32_t getAllKeys (void);

int my_getchar (HT_USART_TypeDef *usart);
int (*cliAbortableFunctionPtr)(uint8_t step, uint32_t cafTicks) = NULL; // this is pointer to function that can be aborted with Ctrl-C
//static int cafArg0 = 0; // these are the args passed to the backgraound functions

//static bool rtcDisplayEnable = false;
// Option structure
typedef struct {
    const char *optionLong;
    const char *description;
		const char optionShort;
} option_t;

// Command structure
typedef struct {
    const char *name;
	  const char *description;
    void (*function)(HT_USART_TypeDef *thisUsart, int commandIndex, int num_args, char *args[]);
    option_t options[MAX_ARGS];
} command_t;


static uint8_t cli_has_run = 0;
static char input[40];
static u8 index = 0;
static u8 cafStep;
static u8 cafTicks=0;
bool isGSMbridge = false;

// Command tablec
command_t command_table[] = {
		{ "led", "Manipulate LEDs and 7 Seg Display",		led_command, 		{
					{"red","-r<0..15> <-0|-1> red led -0 off -1 on",'r'},
					{"7seg","-s<[0..9][.][0..9][.|:][0..9][.][0..9][.]> Display number on 7 segment display",'s'},
					{"off","[switch led on",'0'},
					{"on","[switch led on",'1'},
					{"rgb","-c<0|1> [[r|g|b]] set RGB led colour",'c'},
					{"test","run led cycle test",'t'},
					{"bright","[-b<0..64>] set brightness level",'b'},
					{NULL, NULL, 0}		}},

				{ "gsm", "Communicate with GSM",		gsm_command, 		{
					{"power","-p<0|1>pulse the power pin. 0:2.5S to power off, 1:50mS to power on",'p'},
					{"status","read the status",'s'},
					{"bridge","bridge UART to CLI",'b'},
					{"test","TEST FUNC",'t'},
					{"cook","COOK FUNC",'c'},
					{"erase","ERASE FUNC",'e'},
					{NULL, NULL, 0}		}},

			{ "cook", "Cooking session flash storage commands", NULL, 		{
				{"max","-m  print max number of cooking sessions flash can store",'m'},
				{"num","-n  print current cooking session count stored in EEPROM",'n'},
				{"print","-p  print cooking blocks and confirm erase",'p'},
				{"dummy","-d <N>  write N realistic dummy cooking sessions to flash",'d'},
				{NULL, NULL, 0}		}} ,
};

char* skipWhitespace (char* p) {
		//functionStatus = 86;
		while (*p) {
			if (!isspace(*p)) return p; 
			p++;
		}
		return p;		
}

//**********************************************************************************//
// string parsing code here
//**********************************************************************************//
char* __asciiToBinary(int base, char* str, int* x) {
		//functionStatus = 87;
	int tempValue;
	char c;
	
	tempValue = 0;

	while (1) {
		c=*str;
		c=toupper(c);
		if (isdigit(c)) {
			tempValue*=base;
			tempValue += (c-'0');
		}
		else if ((base==16) && ((c>='A') && (c <= 'F')))  {
			tempValue*=base;
			tempValue += (c-'A'+10);
		}
		else {
			break;
		}
		str++;
	}
	*x = tempValue;
	return (str);

}

//**********************************************************************************//
// RGBasciiToBinary is used only to parse RGB led segment 
//**********************************************************************************//
char* RGBasciiToBinary(char* numericStr, int* value) {
		//functionStatus = 88;
	// valid formats nnnn decimal number, 0xnnnn hexdecimal number
	int base=10;
	int len;
	char* p;
		
	len=strlen(numericStr);
	base=10;
	if (!len) return numericStr;
	//determine base (decimal or hex)
	if (len > 2){ // room for 0xn so for 0x 
		if ((numericStr[0] == '0') &&((numericStr[1] == 'x') || (numericStr[1] == 'X'))) {
			base = 16; //its hex
			numericStr += 2;
		}
	}
	p = __asciiToBinary(base, numericStr, value);
	return p;
}

//**********************************************************************************//
// below implementation of ascii2binary causing LED to run from 0-9 but 
// cannot run RGB, 7seg displays, red led 10-15 implementation to be checked
//**********************************************************************************//
char* asciiToBinary(char* numericStr, int* value) {
		//functionStatus = 89;
	// valid formats nnnn decimal number, 0xnnnn hexdecimal number
	int base=10;
	int tempValue = 0;
	int len;
	char* p;
	char c;
		
	len=strlen(numericStr);
	if (!len) return NULL;
	//determine base (decimal or hex)
	if (len <= 3){ // no room for 0xn so must be decimal
		while (len--) {
			tempValue *= 10;
			if (isdigit(*numericStr)) tempValue += (*numericStr) - '0';
			numericStr++;	
		}
		*value = tempValue;
		return numericStr;
	}
	else { // test for 0x 
		if ((numericStr[0] == '0') &&((numericStr[0] == 'x') || (numericStr[0] == 'X'))) {
			base = 16; //its hex
			p=numericStr + 2;
		}
		else { //we will assume it is decimal
			p = numericStr;
		}
	}
	while (*p) {
		*value *= base;
		c = toupper(*p++);
		if (isdigit(c)) {
			*value += (c - '0');
		}
		else if ((((c >= 'A') && (c <= 'F'))) && (base == 16)) {
			*value += (c-'A' + 10);
		}			
		else return p; 
	}
	return NULL;
}

int my_getchar (HT_USART_TypeDef *usart){
		//functionStatus = 90;
		int c; // really only need this for debug but optimiser means it probably wont make any difference

	if (usart == HT_USART1) {	
		c = USART1_getchar();
		if (c == EOF) {
			return -1;
		}
		c &=0xFF;
		return c;
	}
	else if (usart == HT_UART0) {	
		c = UART0_getchar();
		if (c == EOF) {
			return -1;
		}
		c &=0xFF;
		return c;
	}
	
	return -1;
}


void my_putchar (HT_USART_TypeDef *usart, unsigned char c){
	int bufAvailable;
	if (usart == HT_USART1) {
		//check there is room is the tx fifo
		int guard = 10000; // bounded wait to prevent blocking forever
		while (1) {
			bufAvailable=USART1_getBufferSpace();
			if (bufAvailable > 0) break;
			if (--guard <= 0) {
				break;
			}
		}
		USART1_tx_data(&c, 1);
	}
	else 	if (usart == HT_UART0) {
		//check there is room is the tx fifo
		int guard = 10000; // bounded wait to prevent blocking forever
		while (1) {
			bufAvailable=UART0_getBufferSpace();
			if (bufAvailable > 0) break;
			if (--guard <= 0) {
				break;
			}
		}
		UART0_tx_data(&c, 1);
	}
}	

bool isValidShortOption (int commandIndex, char shortOption) {
		//functionStatus = 92;
	command_t* command = command_table + commandIndex;
	int i;
	
	for (i=0; i < MAX_ARGS; i++) {
		if (command->options[i].optionShort == shortOption) {
			return true;
		}
	}
	return false;
}
// Function to set unset gpio bits
void gpio_command(HT_USART_TypeDef *thisUsart,int commandIndex, int num_args, char *args[]) {
		//functionStatus = 93;
	HT_GPIO_TypeDef* portBase = NULL;
  char* p;

	int pin = -1;
	FlagStatus setOrReset;
	int value;
	char shortOption;
	bool output = false;
	bool input = false;
	char port;
	
  for (int i = 1; i < num_args; i++) {
		p = args[i];
		if (*p == '-') {
			if (*(p+1) == '-') { // check for long option
				my_fprintf(thisUsart, "Long option %s\n", p+2);
			}
			else { //short option
				shortOption = *(p+1);
				//find if short option is valid
				if (isValidShortOption(commandIndex, shortOption)) {
					my_fprintf(thisUsart, "\nUsing port %c\n", *(p+2));
					switch (shortOption) {
						case 'p':
								if (!*(p+2)) { //no value supplied
								  my_fprintf(thisUsart, "ERROR: no ouput value specified for option -o\n");
									return;
							  }

							port = *(p+2);
							switch (*(p+2)){
								case 'A':
									portBase = (HT_GPIO_TypeDef*) HT_GPIOA_BASE;
								break;
								case 'B':
									portBase = (HT_GPIO_TypeDef*) HT_GPIOB_BASE;
								break;

								case 'C':
									portBase = (HT_GPIO_TypeDef*) HT_GPIOC_BASE;
								break;
								case 'D':
									portBase = (HT_GPIO_TypeDef*) HT_GPIOD_BASE;
								break;							}
							break;
						case 'o': //-o<value>
							output=true;
							if (!*(p+2)) { //no value supplied
								my_fprintf(thisUsart, "ERROR: no ouput value specified for option -o\n");
								return;
							}
							asciiToBinary(p+2, &value);
							break;
						case 'i':
							input=true;
							break;
						case 'b': //syntax -b[0xbit][=[0x]nnnn]
							value=0;
							p = skipWhitespace(p+2);
							p=asciiToBinary(p, &pin);
							if ((p = skipWhitespace(p)) != NULL ) {
								if (*p != '=') {// read the bit
									input=true;
								}
								else {// get set or reset 
									if (*(p+1) == '1') {
										output=true;
										value=1;
										setOrReset = SET;
									}
									else if (*(p+1) == '0') {
										output=true;
										value=1;
										setOrReset = RESET;
									}
									else { //syntax error
									}
								}
							}
							break;
						
					}
				}
				else {// invalid short option
					my_fprintf(thisUsart, "ERROR: Invalid short option -%c\n", shortOption);
					return;
				}
			}
			if (portBase == NULL) {
				my_fprintf(thisUsart, "Unspecified or invalid gpio port\n");
			}
		}
  }
	if (output) {
		if (pin == -1) {
			portBase->DOUTR = (unsigned int) value;
		  my_fprintf(thisUsart, "wrote %d to port %c \n", value, port);
		}
		else { //output value to single port bit
			if (value) {
				GPIO_WriteOutBits(portBase, 1<<pin, setOrReset);
				my_fprintf(thisUsart, "\nSET bit %d of gpio port %c\n", pin, port);
			}
		}
	}
	if (input) {
		if (pin == -1) {
			my_fprintf (thisUsart, "\n port %c = output bits 0x%x input bits 0x%x\n",port, portBase->DOUTR, portBase->DINR);
		}
		else {
				char* inOutStr;
				//read the bit
				if ((portBase->DIRCR & (1<<pin)) != 0) { //It is an output pin
					inOutStr = "OUT";
					setOrReset=GPIO_ReadOutBit(portBase, 1<<pin);
				}
				else { // It is an input pin
					inOutStr = "IN";
					setOrReset=GPIO_ReadInBit(portBase, 1<<pin);
				}
				my_fprintf(thisUsart, "\nport %c %sPUT bit %d is %s\n", port, inOutStr, pin, setOrReset==SET?"SET":"RESET");			
		}
	}
  my_fprintf(thisUsart, "!\n");
}


void led_command(HT_USART_TypeDef *thisUsart,int commandIndex, int num_args, char *args[]) {
		//functionStatus = 94;
  char* p;

	int8_t redLED = -1;
	//bool rgbLED;
	int8_t rgbLED = -1;
	int sevenSegValue;
	int pin = -1;
	int value;
	char shortOption;
	bool red = false;
	bool green = false;
	bool blue = false;
	int on_off = 0; // 0: not speficied 1: switch off 2: switch on
	
  for (int i = 1; i < num_args; i++) {
		p = args[i];
		if (*p == '-') {
			if (*(p+1) == '-') { // check for long option
				my_fprintf(thisUsart, "Long option %s\n", p+2);
			}
			else { //short option
				shortOption = *(p+1);
				//find if short option is valid
				if (isValidShortOption(commandIndex, shortOption)) {
					switch (shortOption) {
						case 'r': //red led specifified
								if (!*(p+2)) { //no value supplied
								  my_fprintf(thisUsart, "ERROR: no red led value specified for option -r\n");
									return;
							  }
					      asciiToBinary(p+2, &value);
								redLED = (uint8_t) value;
							break;
						case 'c': //set RGB
								if (!*(p+2)) { //no value supplied
								  my_fprintf(thisUsart, "ERROR: no RGB led number specified for option -c\n");
									return;
							  }
					      RGBasciiToBinary(p+2, &value);
								rgbLED = (uint8_t) value;
							break;
						case 's':
							RGBasciiToBinary(p+2, &sevenSegValue);
							//writeSevenSegInt((uint16_t) sevenSegValue);
						
							writeSevenSegInt(8888); //for test and debug 
						  
							break;
						case '0': //-0 OFF
							on_off = 1;
							break;
						case '1': //-1 ON
							on_off = 2;
							break;
						case 'b': //syntax -b[0xbit][=[0x]nnnn]
							p = skipWhitespace(p);
							p=asciiToBinary(p, &pin);
							if ((p = skipWhitespace(p)) != NULL ) {
								if (*p != '=') {// syntax error
									
								}
								else {// get set or reset 
								}
							}
							break;
					}
				}
				else {// invalid short option
					my_fprintf(thisUsart, "ERROR: Invalid short option -%c\n", shortOption);
					return;
				}
			}
		}
  }
	if (redLED != -1) {
		if ((redLED >= 0) && (redLED < 16)){
			if (on_off == 1) {
				 LEDC_redLED(redLED, false);
			}
			else if (on_off == 2){
				 LEDC_redLED (redLED, true);
			}
			else {
				my_fprintf(thisUsart, "ERROR: must specify -0 (off) or -1 (on) for -r option\n");
			}
		}
		else {
				my_fprintf(thisUsart, "ERROR: Invalid LED number for -r option\n");			
		}
	}
	
	if (rgbLED >= 0){
		// no code to turn off RGB LEDs?
		if (rgbLED > 1) {
			my_fprintf(thisUsart, "ERROR: Invalid RGB LED  for -c option, use 0 or 1\n");
		}
		else {
			p = skipWhitespace(p+=3);
			while (*p) {
				if (toupper(*p) == 'R') red = true;
				else if (toupper(*p) == 'G') green = true;
				else if (toupper(*p) == 'B') blue = true;
				else break;
				p++;
			}
			LEDC_rgbLED (rgbLED, red, green, blue);
	 }
	}
  my_fprintf(thisUsart, "!\n");
}


//GSM command
void gsm_command(HT_USART_TypeDef *thisUsart,int commandIndex, int num_args, char *args[]) {
		//functionStatus = 95;
  char* p;

	char shortOption;
	
  for (int i = 1; i < num_args; i++) {
		p = args[i];
		if (*p == '-') {
			if (*(p+1) == '-') { // check for long option
				my_fprintf(thisUsart, "Long option %s\n", p+2);
			}
			else { //short option
				shortOption = *(p+1);
				//find if short option is valid
				if (isValidShortOption(commandIndex, shortOption)) {
					switch (shortOption) {
						case 'p': //strope the power pin
						{
							p= skipWhitespace(p+2);
							if (*p == '0') { //2.5 Second pulse 								
							GSMpoweron(2500);
							}
							else if (*p == '1') { //50 milliSecond pulse 								
							GSMpoweron(50);
							}
							else {
								my_fprintf(thisUsart, "ERROR: -p option specify 0 or 1\n");
							}
						}
							break;
						case 's': // read the GSM status

								my_fprintf(thisUsart, "GSM powered %s\n", isGSMpoweredUp()? "UP":"DOWN");

							break;
						
						case 'b': //enable UART to UART bridge
//							if (isGSMpoweredUp()) {
//								isGSMbridge = true;
//							}
//							else {
//									my_fprintf(thisUsart, "ERROR: Can not enable GSM bridge when GSM not powered up\n");
//							}
							{
							p= skipWhitespace(p+2);
							if (*p == '0') { 								
							isGSMbridge = false;
							}
							else if (*p == '1') {						
							isGSMbridge = true;
							}
							else {
								my_fprintf(thisUsart, "ERROR: -b option specify 0 or 1\n");
							}
						}
							break;
						case 't': //enable UART to UART bridge
							printf("%d\n",Emulated_EEPROM_Read_Serial_ID());
							//printf("time: %ld\n", getEpochTime());

							//printCookingSessionsFromEEPROM();
							break;
						case 'c': //enable UART to UART bridge
							break;
						case 'e':
							Emulated_EEPROM_Erase_All_Cook_Sessions();
//							printf("ncook eeprom: %d\n",Emulated_EEPROM_Read_Cooking_Session_Num());
							break;
					}
				}
				else {// invalid short option
					my_fprintf(thisUsart, "ERROR: Invalid short option -%c\n", shortOption);
					return;
				}
			}
		}
  }
  my_fprintf(thisUsart, "!\n");
}


int getNextLongOption (int commandIndex, char** args, int num_args, int* lastOptionIndex) {
		//functionStatus = 97;
	    // handle any options
		command_t *command = command_table+commandIndex;
    for (int i = 1; i < num_args; i++) {
        if (args[i][0] == '-') {
            // options are space delimited. options can be -s [value] --long[value]
            for (int j = 0;  ; j++) {
                if ((strcmp(args[i], command->options[j].optionLong) == 0) || (command->options[j].optionShort != 0)) {
                    break;
                }
            }
        }
    }
	return -1;
}	
// Function to execute the command

void my_fputs(const char *str, HT_USART_TypeDef *usart) {
		//functionStatus = 98;
    while (*str != '\0') {
        my_putchar(usart, *str);
        str++;
    }
}

// Basic implementation of printf
void my_fprintf(HT_USART_TypeDef *thisUsart, const char *format, ...) {
		//functionStatus = 99;
    va_list args;
    va_start(args, format);

    while (*format != '\0') {
        if (*format == '%') {
            format++;
            if (*format == 's') {
                char *s = va_arg(args, char *);
                while (*s != '\0') {
                    my_putchar(thisUsart, *s);
                    s++;
                }
								
            }
            else if (*format == 'c') {
                int c = va_arg(args, int);
                    my_putchar(thisUsart, c);
								
            }	else if (*format == 'd') {
                int d = va_arg(args, int);
                int temp = d;
                int num_digits = 0;
                if (temp == 0) {
                    num_digits = 1; // Special case for 0
                } else {
                    while (temp != 0) {
                        num_digits++;
                        temp /= 10;
                    }
                }
                char num_buffer[MAX_ARG_LENGTH];
                for (int i = num_digits - 1; i >= 0; i--) {
                    num_buffer[i] = '0' + (d % 10);
                    d /= 10;
                }
                num_buffer[num_digits] = '\0';
                my_fputs(num_buffer, thisUsart);
            } else if (*format == 'x') {
                int num_digits = 8;
                int d = va_arg(args, int);
                char num_buffer[MAX_ARG_LENGTH];
                for (int i = num_digits - 1; i >= 0; i--) {
                    num_buffer[i] = '0' + (d & 0x0F);
										if (num_buffer[i] > '9') num_buffer[i]+=7;
                    d >>=4;
                }
                num_buffer[num_digits] = '\0';
                my_fputs(num_buffer, thisUsart);
						 } else if (*format == 'h') {
                int num_digits = 2;
                int d = va_arg(args, int);
                char num_buffer[MAX_ARG_LENGTH];
                for (int i = num_digits - 1; i >= 0; i--) {
                    num_buffer[i] = '0' + (d & 0x0F);
										if (num_buffer[i] > '9') num_buffer[i]+=7;
                    d >>=4;
                }
                num_buffer[num_digits] = '\0';
                my_fputs(num_buffer, thisUsart);
							}


        } else {
            my_putchar(thisUsart, *format);
        }
        format++;
    }

    va_end(args);
}

// Called perodically
struct lastCommand_struc {
    void (*function)(HT_USART_TypeDef *thisUsart, int commandIndex, int num_args, char *args[]);
		int commandIndex;
		int num_args;
		char** args;
} lastCommand = {0};


//int cli(HT_USART_TypeDef *thisUsart) {// called from ain every 10mS
//  int c;
//	int l;
//	int commandIndex = -1;
//	command_t *command = NULL;
//	char* p;
//	char* q;

//		if (isGSMbridge) { // check for any data from the GSM UART and display it
//			int gsmChar = UART0_getchar();
//			if (gsmChar != -1) {
//					my_putchar(thisUsart, (uint8_t) (gsmChar & 0xFF));
//			}
//		}
//    if (!cli_has_run) {
////DEBUG TEMP			my_fprintf(thisUsart, "> %c", 'C');
//			cli_has_run = 1;
//			my_fprintf(thisUsart, "CLI v6\n>");
//		}
//		if (cliAbortableFunctionPtr) {
//			 l = (*cliAbortableFunctionPtr) (cafStep, cafTicks++);
//			if (l<0) {
//				if (l == -1) {
//						my_fprintf(thisUsart, "\nBackground command completed successfuly\n");
//				}
//				else {
//					my_fprintf(thisUsart, "\nBackground failed: error code %d\n", l);
//				}
//				cliAbortableFunctionPtr = NULL;
//			}
//			 writeSevenSegInt((uint16_t) cafTicks);
//		}

//// just for the RTC test ------------------------------------------------------------		
//		if (rtcDisplayEnable) { 
//			uint32_t tempRTCcount;
//			uint16_t hhmm;
//			tempRTCcount = RTC_GetCounter();
//			hhmm = tempRTCcount/(1024); // convert temp count 32.25ms into minutes;		
//			writeSevenSegInt ((((hhmm/60)%24)*100) + (hhmm%60));
//				if (((tempRTCcount % 1024) >512)) {
//					writeSevenSegDP (1, true);
//				}
//				else {
//					writeSevenSegDP (1, false);
//				}
//		}
//// end RTC test ----------------------------------------------------------------------------
//     if ((c = my_getchar(thisUsart)) != EOF) {
////DEBUG TEMP			 my_fprintf(thisUsart,"=%c %d", c,c);
//			 if (c == 0x03) { //CTRL-C entered
//				 if (isGSMbridge) isGSMbridge = false;
//				 if (cliAbortableFunctionPtr) { //if we are executing an abortable function
//					 cliAbortableFunctionPtr = NULL;
//					 my_fprintf(thisUsart, "!!ABORTED!!\n");
//				 }
//			 }
//			 
//			 // THIS CODE EFFECTIVELY HIJACKS the CLI Usart and forms a back-to-back bridge to the GSM UART
//			 // Only CTL-C (ABOVE) breaks out of this
//				if (isGSMbridge) {
//					 uint8_t ch = (uint8_t) (c&0xFF);
//					 //UART0_tx_data(&ch, 1);
//						UART0_SEND_DATA(&ch,sizeof(ch));
//					return 0;
//				}
//			 // ===========================================================================================
//			 if (c == 0x12) {//ctrl r
//				 if (lastCommand.function) {
//					 lastCommand.function(thisUsart, lastCommand.commandIndex, lastCommand.num_args,lastCommand.args);
//					 my_fprintf(thisUsart, "\n>");
//					 return 0;
//				 }
//			 }
//			 if (index == 0) {
//				 	memset (input, 0, sizeof(input));
//			 }

//       if ((c == '\n') ||  (c=='\r')) { //input string conatins the whole command line
//					  my_putchar(thisUsart,'\n');
//            input[index] = '\0'; // Null-terminate the input string
//						if (index) {
//							// Tokenise input, options start with - (short name) or -- (long name). The name may be followed by white space and a =
//							num_args = 0;
//							p = skipWhitespace(input); // skip ay leading whitspace before command name
//							q = p; //save start of command name
//							if (*p) {// check this not an empty line
//								while ((*p) && (*p != ' ')) p++;
//								*p = '\0';
//								// Find the command in the command table
//								for (int i = 0; i < sizeof(command_table) / sizeof(command_table[0]); i++) {
//										if (strcmp(q, command_table[i].name) == 0) {
//												command = &command_table[i];
//												commandIndex = i;
//												break;
//										}
//								}
//								if (commandIndex < 0) {
//									index = 0;
//									my_fprintf(thisUsart, "\nERROR: Unrecognised command\n");
//									return -1; //ERROR comamand not found
//								}
//								args[0] = q; //first arg is always command name
//								num_args=1;
//							}
//							else {
//								return 0;
//							}
//							p += 1; //point past the the null terminator of the command name
//							// now deal with options and their arguments
//							while (*p && num_args < MAX_ARGS) {
//									p = skipWhitespace(p); // skip past any space before the first option 
//									if (!*p) break;
//									l = strlen(p);
//									if (p[0] == '-') {
//										//its an option
//										if ((l>3) && (p[1] == '-')) { // -- means long option
//											//its a long option
//										}
//										else {
//											//its short option
//											if (!isValidShortOption(commandIndex, p[1])) {
//												index = 0;
//												num_args = 0;
//												my_fprintf(thisUsart, "ERROR: invalid option %c\n>", p[1]);
//												return -4; //not a valid oftion for this command
//											}
//											// the option may have arguments, capture the whole option string
//											q=p+1; //point to next
//											while ((*q) && (*q != '-')) q++; // look for end of input or next '-' which is start of next option (if any)
//											if((*q) && (*q == '-')) { //check if this is the start of the next option
//												//its the beinning of the next option
//												*(q-1) = '\0';
//												args[num_args++] = p;
//												p=q;
//											}
//											else { //last argument in input
//												args[num_args++] = p;
//												p=q;
//											}
//										}							
//									}
//									else {// its a manadtory argument
//												args[num_args++] = p;
//												while ((*p) && (*p != ' ')) p++;	
//												*p = 0;
//											  p++;
//									}
//							}
//							//execute command
//							lastCommand.function = command->function;
//							lastCommand.commandIndex = commandIndex;
//							lastCommand.num_args = num_args;
//							lastCommand.args = args;
//							command->function(thisUsart, commandIndex, num_args, args);
//							num_args = 0;
//					  }
//            // Reset indices for the next command
//            index = 0;
//            num_args = 0;
//            my_fprintf(thisUsart, "\n>");
//					
//        }
//				else if ((c == '\b') || (c == 0x7F)) { // do the backspace stuff
//					if (index) {
//						--index;
//						my_fprintf (thisUsart, "\b \b");
//					}
//					return 0;
//				}
//				else if (index < sizeof(input) - 1) {
//            input[index++] = c; // add the character to the current command line
//						my_putchar (thisUsart, c); //and echo it
//        }
//    }
//		return 0;
//}

//*********************************************************************************

//int cli(HT_USART_TypeDef *thisUsart) {
//    int c;
//    int l;
////    int commandIndex = -1;
////   command_t *command = NULL;
////    char* p;
////    char* q;
//    
//    // Certificate streaming state
//    static enum {
//        CLI_NORMAL,
//        CLI_CERT_HEADER,
//        CLI_KEY_HEADER,
//        CLI_STREAMING
//    } cli_state = CLI_NORMAL;
//    
//    static int header_pos = 0;
//    const char cert_header[] = "-----BEGIN CERTIFICATE-----";
//    const char key_header[] = "-----BEGIN RSA PRIVATE KEY-----";
//    const char end_marker[] = "-----END";

//    // Process incoming characters
//    while ((c = my_getchar(thisUsart)) != EOF) {
//        // Handle GSM bridge mode first
//        if (isGSMbridge) {
//            uint8_t ch = (uint8_t)(c & 0xFF);
//            UART0_SEND_DATA(&ch, sizeof(ch));
//            
//            if (c == 0x03) { // CTRL-C to exit bridge mode
//                isGSMbridge = false;
//                my_fprintf(thisUsart, "\nGSM bridge deactivated\n>");
//            }
//            continue;
//        }

////        // Certificate/key streaming detection
////        if (cli_state == CLI_NORMAL) {
////            // Check for certificate or key header start
////            if (c == '-') {
////                if (cert_header[header_pos] == c) {
////                    if (++header_pos == strlen(cert_header)) {
////                        cli_state = CLI_CERT_HEADER;
////                        my_fprintf(thisUsart, "\nCertificate detected\n");
////                        // Immediately send the header
////                        UART0_SEND_DATA((uint8_t*)cert_header, strlen(cert_header));
////                        UART0_SEND_DATA((uint8_t*)"\n", 1);
////                        header_pos = 0;
////                        continue;
////                    }
////                } 
////                else if (key_header[header_pos] == c) {
////                    if (++header_pos == strlen(key_header)) {
////                        cli_state = CLI_KEY_HEADER;
////                        my_fprintf(thisUsart, "\nPrivate key detected\n");
////                        // Immediately send the header
////                        UART0_SEND_DATA((uint8_t*)key_header, strlen(key_header));
////                        UART0_SEND_DATA((uint8_t*)"\n", 1);
////                        header_pos = 0;
////                        continue;
////                    }
////                } else {
////                    header_pos = 0;
////                }
////            } else {
////                header_pos = 0;
////            }
////        }

////        // Handle streaming mode
////        if (cli_state == CLI_CERT_HEADER || cli_state == CLI_KEY_HEADER) {
////            cli_state = CLI_STREAMING;
////        }

////        if (cli_state == CLI_STREAMING) {
////            // Stream data to UART0
////            uint8_t ch = (uint8_t)c;
////            UART0_SEND_DATA(&ch, 1);
////            
////            // Check for end marker
////            if (c == '-') {
////                if (++header_pos >= strlen(end_marker)) {
////                    cli_state = CLI_NORMAL;
////                    header_pos = 0;
////                    my_fprintf(thisUsart, "\nStreaming complete\n>");
////                    continue;
////                }
////            } else {
////                header_pos = 0;
////            }
////            
////            if (c == 0x03) { // CTRL-C to abort
////                cli_state = CLI_NORMAL;
////                my_fprintf(thisUsart, "\nStreaming aborted\n>");
////            }
////            continue;
////        }

//        // Normal CLI processing
//        if (c == 0x03) { // CTRL-C
//            if (cliAbortableFunctionPtr) {
//                cliAbortableFunctionPtr = NULL;
//                my_fprintf(thisUsart, "!!ABORTED!!\n");
//            }
//        }
//        
//        if (c == 0x12) { // CTRL-R
//            if (lastCommand.function) {
//                lastCommand.function(thisUsart, lastCommand.commandIndex, 
//                                   lastCommand.num_args, lastCommand.args);
//                my_fprintf(thisUsart, "\n>");
//                continue;
//            }
//        }
//        
//        if ((c == '\n') || (c == '\r')) {
//            input[index] = '\0';
//            
//            // Process commands
//            if (index > 0) {
//                if (strcmp(input, "gsm -b 1") == 0) {
//                    isGSMbridge = true;
//                    my_fprintf(thisUsart, "\nGSM bridge activated\n>");
//                }
//                else if (strcmp(input, "gsm -b 0") == 0) {
//                    isGSMbridge = false;
//                    my_fprintf(thisUsart, "\nGSM bridge deactivated\n>");
//                }
//                // ... other command processing ...
//            }
//            
//            index = 0;
//            my_fprintf(thisUsart, "\n>");
//        } 
//        else if ((c == '\b') || (c == 0x7F)) {
//            if (index > 0) {
//                --index;
//                my_fprintf(thisUsart, "\b \b");
//            }
//        }
//        else if (index < sizeof(input) - 1) {
//            input[index++] = c;
//            my_putchar(thisUsart, c);
//        }
//    }

//    // Background processing
//    if (cliAbortableFunctionPtr) {
//        l = (*cliAbortableFunctionPtr)(cafStep, cafTicks++);
//        if (l < 0) {
//            cliAbortableFunctionPtr = NULL;
//            my_fprintf(thisUsart, "\nBackground operation completed\n>");
//        }
//    }

//    return 0;
//}
int on;
int idx;
char col;
static bool cookEraseConfirmationPending = false;
#define MAX_SESSION_DURATION_SEC 7200U

static void printCookingBlocks(HT_USART_TypeDef *thisUsart) {
	int32_t sessionCount = Emulated_EEPROM_Read_Cooking_Session_Num();
	int blockSize = g_params.num_cooking_blocks;
	int blockCount;
	int plateIndex = (eCooker.numberOfPlates == SINGLE_STOVE) ? 0 : 1;
	int block;

	if (sessionCount < 0) {
		sessionCount = 0;
	}
	if (blockSize <= 0) {
		my_fprintf(thisUsart, "\nERROR: invalid cooking block size\n");
		return;
	}

	blockCount = (sessionCount == 0) ? 0 : (sessionCount + blockSize - 1) / blockSize;
	my_fprintf(thisUsart, "\nNumber of cooking blocks : %d\nCooking Blocks :\n", blockCount);

	for (block = 0; block < blockCount; block++) {
		int offset = block * blockSize;
		int chunkSize = sessionCount - offset;
		int written;
		int i;
		char buf[1024];

		if (chunkSize > blockSize) {
			chunkSize = blockSize;
		}

		written = snprintf(buf, sizeof(buf),
			"{\"devID\":{\"sn\":\"%u\",\"FW\":\"%s\"},\"ts\":%lu,\"Cook\":[",
			Emulated_EEPROM_Read_Serial_ID(),
			globalPayload.devID.FW ? globalPayload.devID.FW : "",
			(unsigned long)getEpochTimeRTC());
		if (written <= 0 || written >= (int)sizeof(buf)) {
			my_fprintf(thisUsart, "\nERROR: cooking block is too large to print\n");
			return;
		}

		for (i = 0; i < chunkSize; i++) {
			CookSession session = Emulated_EEPROM_Read_Cook_Session((uint32_t)(offset + i));
			uint32_t duration = (session.offT > session.onT) ?
				(uint32_t)(((uint64_t)session.offT - (uint64_t)session.onT > MAX_SESSION_DURATION_SEC) ?
					MAX_SESSION_DURATION_SEC : (session.offT - session.onT)) : 0U;
			float pwrC = ((float)duration * POWER_LEVEL[plateIndex][session.pwr]) / 3600.0f;
			double pg = ((double)session.Ag * (double)session.Vg * (double)duration) / 3600.0;
			int remaining = (int)sizeof(buf) - written;
			int n;

			if (pwrC > ((eCooker.numberOfPlates == SINGLE_STOVE) ? 3200.0f : 3300.0f)) {
				pwrC = (eCooker.numberOfPlates == SINGLE_STOVE) ? 3200.0f : 3300.0f;
			}
			n = snprintf(buf + written, remaining,
				"{\"pan\":\"%c\",\"onT\":%ld,\"offT\":%ld,\"pwr\":%u,\"pwrC\":%.2f,\"Pg\":%.2f}%s",
				session.pan,
				session.onT,
				session.offT,
				POWER_LEVEL[plateIndex][session.pwr],
				pwrC,
				pg,
				(i < chunkSize - 1) ? "," : "");
			if (n <= 0 || n >= remaining) {
				my_fprintf(thisUsart, "\nERROR: cooking block is too large to print\n");
				return;
			}
			written += n;
		}

		if (written + 2 >= (int)sizeof(buf)) {
			my_fprintf(thisUsart, "\nERROR: cooking block is too large to print\n");
			return;
		}
		buf[written++] = ']';
		buf[written++] = '}';
		buf[written] = '\0';
		my_fputs(buf, thisUsart);
		my_putchar(thisUsart, '\n');
		BFTM0_Delay_ms(100);
	}
}

int cli(HT_USART_TypeDef *thisUsart) {
		//functionStatus = 99;
    int c;
    int l;
    
    // 1. First check for any incoming data from UART0 (GSM) and display on USART1 (CLI)
	
	if(cli_has_run==0){
	my_fprintf(thisUsart, "ATEC CLI v1.0 > \n");
	cli_has_run=1;
	}
    while ((c = UART0_getchar()) != -1) {
        my_putchar(thisUsart, (uint8_t)(c & 0xFF)); // Display on CLI
    }

    // 2. Process CLI input
    while ((c = my_getchar(thisUsart)) != EOF) {
        // Handle GSM bridge mode
        if (isGSMbridge) {
            uint8_t ch = (uint8_t)(c & 0xFF);
            UART0_SEND_DATA((const char *)&ch, sizeof(ch)); // Send to GSM
            
            if (c == 0x03) { // CTRL-C to exit bridge mode
                isGSMbridge = false;
                my_fprintf(thisUsart, "\nGSM bridge deactivated\n>");
            }
            continue;
        }

        // Normal CLI processing
        if (c == 0x03) { // CTRL-C
            if (cliAbortableFunctionPtr) {
                cliAbortableFunctionPtr = NULL;
                my_fprintf(thisUsart, "!!ABORTED!!\n");
            }
        }
        
        if (c == 0x12) { // CTRL-R
            if (lastCommand.function) {
                /* Validate function pointer before calling to prevent hardfault */
                uint32_t funcAddr = (uint32_t)lastCommand.function;
                if (funcAddr >= 0x1000 && (funcAddr & 0x1) != 0) {
                    // Additional check: ensure args pointer is valid if num_args > 0
                    if (lastCommand.num_args > 0 && lastCommand.args == NULL) {
                        lastCommand.function = NULL;
                        my_fprintf(thisUsart, "\nInvalid args pointer detected\n>");
                    } else {
                        lastCommand.function(thisUsart, lastCommand.commandIndex, 
                                           lastCommand.num_args, lastCommand.args);
                        my_fprintf(thisUsart, "\n>");
                    }
                } else {
                    /* Invalid function pointer - clear it */
                    lastCommand.function = NULL;
                    my_fprintf(thisUsart, "\nInvalid function pointer detected\n>");
                }
            }
        }
        
        if ((c == '\n') || (c == '\r')) {
            input[index] = '\0';
            
            // Process commands
            if (index > 0) {
                if (cookEraseConfirmationPending &&
                    strcmp(input, "y") != 0 && strcmp(input, "n") != 0) {
                    cookEraseConfirmationPending = false;
                }
                if (cookEraseConfirmationPending &&
                    (strcmp(input, "y") == 0 || strcmp(input, "n") == 0)) {
                    if (strcmp(input, "y") == 0) {
                        if (Emulated_EEPROM_Erase_All_Cook_Sessions() == SUCCESS) {
                            freeCookSessions();
                            my_fprintf(thisUsart, "\nAll cooking sessions erased");
                        } else {
                            my_fprintf(thisUsart, "\nERROR: failed to erase cooking sessions");
                        }
                        cookEraseConfirmationPending = false;
                    } else if (strcmp(input, "n") == 0) {
                        cookEraseConfirmationPending = false;
                        my_fprintf(thisUsart, "\nCooking data was not erased");
                    }
                }
                else if (strcmp(input, "gsm -b 1") == 0) {
                    isGSMbridge = true;
                    my_fprintf(thisUsart, "\nGSM bridge activated\n>");
                }
                else if (strcmp(input, "gsm -b 0") == 0) {
                    isGSMbridge = false;
                    my_fprintf(thisUsart, "\nGSM bridge deactivated\n>");
                }
								else if (strcmp(input, "meta -s") == 0){
									my_fprintf(thisUsart, "\n%d\n",Emulated_EEPROM_Read_Serial_ID());
								}
								else if (strcmp(input, "meta -i") == 0){
									my_fprintf(thisUsart, "\n%s\n",globalPayload.devID.sid);
								}
								else if (strcmp(input, "meta -g") == 0){
									my_fprintf(thisUsart, "\n%s\n",globalPayload.devID.loc);
								}
								else if (sscanf(input, "led -r%d %d", &idx, &on) == 2) {
										LEDC_redLED(idx, (bool)on);
								}
								
								else if (sscanf(input, "led -s%d", &on) == 1){
									switch(on){
										case 0:
											clearSevenSegDisplay();
											break;
										case 1:
											writeSevenSegInt(8888);
											break;
									}										
								}
								
								else if (sscanf(input, "led -c%d %s", &idx, &col) == 2){
									switch(col){
										case 'r':
											LEDC_rgbLED(idx, true, false, false);
											break;
										case 'g':
											LEDC_rgbLED(idx, false, true, false);
											break;
										case 'b':
											LEDC_rgbLED(idx, false, false, true);
											break;
										default:
											LEDC_rgbLED(idx, false, false, false);
											break;
									}
										
								}
								else if (strcmp(input, "cook -m") == 0) {
									my_fprintf(thisUsart, "\nMax cooking sessions flash can store: %d\n", COOK_SESSION_MAX_COUNT);
								}
								else if (strcmp(input, "cook -n") == 0) {
									int32_t sessionCount = Emulated_EEPROM_Read_Cooking_Session_Num();
									my_fprintf(thisUsart, "\nCooking sessions stored in EEPROM: %d\n", sessionCount);
								}
								else if (strcmp(input, "cook -p") == 0) {
									printCookingBlocks(thisUsart);
									my_fprintf(thisUsart, "Erase ALL cooking data? Type y to confirm, n to cancel\n");
									cookEraseConfirmationPending = true;
								}
								else if (sscanf(input, "cook -d %d", &on) == 1) {
									int32_t num_d = on;
									int32_t idx_d = Emulated_EEPROM_Read_Cooking_Session_Num();
									int32_t written = 0;
									int32_t i_d;
									if (num_d <= 0) {
										my_fprintf(thisUsart, "\nERROR: count must be > 0\n");
									} else {
										uint32_t seed = getEpochTimeRTC();
										const uint16_t pwr_table[9] = {300, 400, 600, 700, 900, 1100, 1300, 1500, 1600};
										for (i_d = 0; i_d < num_d; i_d++) {
											if (idx_d >= (int32_t)COOK_SESSION_MAX_COUNT) {
												my_fprintf(thisUsart, "\nFlash full at %d/%d, stopped early\n",
												           idx_d, COOK_SESSION_MAX_COUNT);
												break;
											}
											seed = seed * 1664525UL + 1013904223UL;
											char pan_d = ((seed & 0x1) == 0) ? 'L' : 'R';
											uint16_t pwr_d = pwr_table[(seed >> 4) % 9];
											long onT_d = (long)getEpochTimeRTC() - (long)((num_d - i_d) * 21600UL);
											uint32_t dur_d = 300UL + ((seed >> 8) % 6901UL);
											long offT_d = onT_d + (long)dur_d;
											float ag_d = 3.0f + (float)((seed >> 12) % 50) * 0.1f;
											float vg_d = 210.0f + (float)((seed >> 16) % 21);
											ErrStatus res_d = Emulated_EEPROM_Write_Cook_Session(
												(uint32_t)idx_d, pan_d, onT_d, offT_d, pwr_d, ag_d, vg_d);
											if (res_d == SUCCESS) {
												idx_d++;
												written++;
											} else {
												my_fprintf(thisUsart, "\nERROR: write failed at index %d\n", idx_d);
												break;
											}
										}
										if (written > 0) {
											Emulated_EEPROM_Write_Cooking_Session_Num(idx_d);
											my_fprintf(thisUsart, "\n%d dummy session(s) written (total: %d)\n", written, idx_d);
										}
									}
								}
            }
            
            index = 0;
            my_fprintf(thisUsart, "\n>");
        } 
        else if ((c == '\b') || (c == 0x7F)) {
            if (index > 0) {
                --index;
                my_fprintf(thisUsart, "\b \b");
            }
        }
        else if (index < sizeof(input) - 1) {
            input[index++] = c;
            my_putchar(thisUsart, c);
        }
    }

    // Background processing
    if (cliAbortableFunctionPtr) {
        /* Validate function pointer before calling to prevent hardfault */
        /* For ARM Cortex-M, valid function pointers should have LSB set (Thumb mode) */
        /* and be in valid memory range */
        uint32_t funcAddr = (uint32_t)cliAbortableFunctionPtr;
        if (funcAddr >= 0x1000 && (funcAddr & 0x1) != 0) {
            l = (*cliAbortableFunctionPtr)(cafStep, cafTicks++);
            if (l < 0) {
                cliAbortableFunctionPtr = NULL;
                my_fprintf(thisUsart, "\nBackground operation completed\n>");
            }
        } else {
            /* Invalid function pointer detected - clear it to prevent future hardfaults */
            cliAbortableFunctionPtr = NULL;
            my_fprintf(thisUsart, "\nInvalid function pointer detected and cleared\n>");
        }
    }

    return 0;
}
