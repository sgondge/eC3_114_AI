#include "ht32.h"
#include "CRC.h"
#include "GPIO.h"
#include "BFTM0.h"
#include "GPTM0.h"
#include "UART0.h"
#include "USART1.h"
#include "I2C0.h"
#include "I2C1.h"
#include "LEDC.h"
#include "RTC.h"
#include "cli.h"
#include "eCooker.h"
#include "FSM.h"
#include "buzzer.h"
#include "ht32_board.h"
#include "ht32_board_config.h"
#include "ht32_touchkey.h"
#include "ht32_touchkey_conf.h"
#include "ht32_touchkey_BSconf.h"
#include "Payload.h"
#include "EEPROM.h"
#include "TouchFunction.h" 
#include "batVoltage.h"
#include "utils.h"
#include "GSM.h"
#include "iap_handler.h"
#include "string.h"
#include "cJSON.h"
#include "stove_stats.h"
#include <stdio.h>
#include <limits.h>
#include "ota_commands.h"
#include "common/memory_utils.h"

#define ERROR_STATE_THRESHOLD 500
#define ON_TIME  200  
#define OFF_TIME 20  
#define EVENT_OFF_PAYLOAD_DEBOUNCE_S 15
#define IDEAL_EPOCH_CHAR_SIZE 10
#define IDEAL_ICCID_CHAR_SIZE 20
#define MAX_JSON_SIZE 320
#define MAX_JSON_MESSAGES_PER_LOOP 3



static unsigned long last_event_off_payload_devt = 0;
static uint32_t last_bftm0_ct_i2c_mains;
static uint32_t last_bftm0_ct_err;
static uint32_t last_bftm0_ct_ecConfig;
static bool prev_left_error = false;
static bool prev_right_error = false;
static uint32_t last_gptm0_ct;
static unsigned long last_devt_epoch_sync = ULONG_MAX;
static unsigned long last_devt_iccid      = ULONG_MAX;
static unsigned long last_devt_clbs       = ULONG_MAX;
uint32_t update_bftm0;
uint32_t file_bftm0;


u8 PayloadOption = 0;
char* iccid = 0;
char temp[21];
char* temp2 = 0;

bool errorSent = false;
bool __init_sync_time__ = false;
bool conn = false;
bool __init_iccid__ = false;
volatile bool turnOff = false;
bool Deepsleep_Start_Flag = false;


char epochStr[20];
size_t iccidLength = 0;
size_t epochLength = 0;

FlagStatus Bat_CHG = RESET;

typedef enum {
    STATE_OFF,
    STATE_ON
} GPIO_State;

GPIO_State gpio_state = STATE_ON;  // Initial state for PBCI GPIO

bool updateAvailable = false;
bool updateAnimationActive = false;

u8 Rx_Data_Sum = 0;
u8 i2c0 = 0, i2c1 = 1;

unsigned char CRC_RX_I2C[11] = {0};
unsigned char CRC_RX_I2C1[11] = {0};

unsigned int CRC_RX_I2C_VAL = 0;
unsigned int CRC_RX_I2C1_VAL = 0;

u8 main_off_count = 0;            // Counter for consecutive zeros from i2c0 status
bool main_off_flag = false; 

FlagStatus i2c0SclGPIO, i2c0SdaGPIO, i2c1SclGPIO, i2c1SdaGPIO;

uint32_t gpioHighDuration = 0;

bool pb2_status = false;  // Global flag to monitor PB2 pin status

CLBSResult clbs;
bool updateTriggered = false;

void get_first_20_chars(const char* source, char* dest) {
    if (source == NULL || dest == NULL) {
        if (dest != NULL) {
            dest[0] = '\0';  // Set empty string if source is NULL
        }
        return;
    }
    strncpy(dest, source, 20);
    dest[20] = '\0';  /* safe for dest of size 21 */
}

bool are_all_digits(const char* str) {
    if (str == NULL) {
        return false;
    }
    for (u8 i = 0; i < 20; i++) {
        if (str[i] == '\0') {
            break;  // End of string reached
        }
        if (!isdigit((unsigned char)str[i])) {
            return false; // Not a digit
        }
    }
    return true; // All are digits
}

bool isGSM;
ParsedPayload fwUpdate;
ExtractResult result;

bool reset = false;
bool fileTriggered = false;

typedef struct {
	bool errorTimerActive;
	bool errorSent;
	int last_device_time_err;
} errorLog;

errorLog leftErr = {false, false, -1};
errorLog rightErr = {false, false, -1};

void fileDelay(void){
	fileTriggered = false;
	file_bftm0 = bftm0_ct;
}

void updateSwitch(void){
					if (!updateTriggered && update_bftm0 > 0) { 
            if ((bftm0_ct >= update_bftm0 + 25000) || (bftm0_ct < update_bftm0)) {
							updateTriggered = true;
								fwDownload(fwUpdate.data);
								fileDelay();
            }
        }
		}

void fileSwitch(void) {
    if (!fileTriggered && file_bftm0 > 0) { 
        if ((bftm0_ct >= file_bftm0 + 5000) || (bftm0_ct < file_bftm0)) {
            fileTriggered = true;
            handleSyncLED(LED_OFF, 'r');
            handleConnect(CONNECT);
        }
    }
		if(fwUpdate.success && fileTriggered && isGSM /* && (globalPayload.stat.BatV >= 3.2)*/){
					const char *filenamePtr = findHexFilenameInFSLS();
					if (filenamePtr) {
							updateAnimationActive = false;  // Stop animation before reboot
							clearSevenSegDisplay();
							handleSyncLED(LED_OFF, 'b');
							BOOT_MODE = BOOT_MODE_IAP;		// enter boot mode here
							NVIC_SystemReset();           /* Trigger reset */
					} else {
							updateAnimationActive = false;
							fwUpdate.success = false;
							fileTriggered = false;
							updateTriggered = false;
							update_bftm0 = 0;
							file_bftm0 = 0;
							handleSyncLED(LED_OFF, 'b');
							buzz(3);
							writeLineToSevenSeg();
							BFTM0_Delay_ms(2000);
							clearSevenSegDisplay();
							return;
					}
		}
}

void updateDelay(void){
	updateTriggered = false;
	update_bftm0 = bftm0_ct;
}


void fotaUpdate(void) {
	if (fotaAvailable && currentState == STATE_ONOFF) {
		/* Only clear flag when we actually sent the command; otherwise keep retrying */
		if (gsmFotaDownload(fwUpdate.data)) {
			fotaAvailable = false;
		}
	}
}

void update(void){
	if(updateAvailable && currentState==STATE_ONOFF){
		disableHdispSwitch();
		clearSevenSegDisplay();
		
		handleSyncLED(LED_BLINK, 'b');
		updateAnimationActive = true;
		currentFrame = 0;
		lastUpdateTime = bftm0_ct;
		sendToGSM("AT");
		sendToGSM("AT+CRESET");
		resetCGSNRetryCountOnGSMReset();
		resetEpochRetryStateOnGSMReset();
		mqtt_service_started = false;
		reset = true;
		updateAvailable = false;
	}
}

int last_cid = -1;

void process_uart_json_messages(void) {
    int messages_processed = 0;

    /* Commands stay queued in the UART0 message ring; dispatching would send AT commands during the cook ACK wait. */
    if (g_check_cook_publish) {
        return;
    }

    char json_buf[MAX_JSON_SIZE];
    while (messages_processed < MAX_JSON_MESSAGES_PER_LOOP &&
           UART0_MsgPop(json_buf, sizeof(json_buf)) > 0) {

        cJSON *root = cJSON_Parse(json_buf);
        if (!root) {
        } else {
            cJSON *ty   = cJSON_GetObjectItemCaseSensitive(root, "ty");
            cJSON *cid  = cJSON_GetObjectItemCaseSensitive(root, "cid");
            cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");

            if (ty != NULL && cJSON_IsString(ty) && ty->valuestring != NULL && 
                cid != NULL && cJSON_IsNumber(cid)) {
                const char *type = ty->valuestring;
                int corr_id = cid->valueint;

                if (corr_id != last_cid) {
                    last_cid = corr_id;
                    (void)dispatch_command(type, corr_id, root, data);
                } else {
                }
            } else {
            }
            cJSON_Delete(root);
        }
        messages_processed++;
    }
}


void Set_Interrupt_Priority()
{
	NVIC_SetPriority(UART0_IRQn, 0);
	
  NVIC_SetPriority(I2C0_IRQn, 1);
  NVIC_SetPriority(I2C1_IRQn, 1);
	NVIC_SetPriority(BFTM0_IRQn, 1);
  NVIC_SetPriority(BFTM1_IRQn, 1);
	
	NVIC_SetPriority(GPTM0_IRQn, 2);
  NVIC_SetPriority(USART1_IRQn, 2);
	NVIC_SetPriority(RTC_IRQn, 2);
  NVIC_SetPriority(ADC_IRQn, 2);
	
	NVIC_EnableIRQ(UART0_IRQn);
	NVIC_EnableIRQ(I2C0_IRQn);
	NVIC_EnableIRQ(I2C1_IRQn);
	NVIC_EnableIRQ(BFTM0_IRQn);
	NVIC_EnableIRQ(BFTM1_IRQn);
	NVIC_EnableIRQ(GPTM0_IRQn);
	NVIC_EnableIRQ(USART1_IRQn);
	NVIC_EnableIRQ(RTC_IRQn);
	NVIC_EnableIRQ(ADC_IRQn);	
}


int main() {

		initGlobalPayload();
		I2C0_Configuration();
		I2C1_Configuration();
		GPIO_Configuration();
	  BFTM0_Configuration();
		BFTM1_Configuration();
		GPTM0_Configuration();
		TKL_Configuration();
    RETARGET_Configuration();
    UART0_Configuration();
    USART1_Configuration();
    LEDC_Configuration();
		RTC_Configuration();
		RTC_Cmd(ENABLE);
    eCooker_Configuration();
		intializeCookNum();
		ADC_Configuration();
    CRC_Configuration();
		Systick_Configuration();
		Set_Interrupt_Priority();
	
 		Emulated_EEPROM_Read_RuntimeParams();
	
    GPIO_SetOutBits(HT_GPIOB, GPIO_PIN_13);
    GPIO_SetOutBits(HT_GPIOB, GPIO_PIN_14);
		
		globalPayload.devID.loc = "NA";
		globalPayload.devID.sid = "NA";
			
		printf("ATEC CLI\n");
		
    while (1) {	
				if(!ecConfig && ((bftm0_ct % 100 == 0 && last_bftm0_ct_ecConfig != bftm0_ct) || i2c1_master_connected || (I2C_Rx_Data[1] == 3) || (I2C1_Rx_Data[1] == 3))){
					if(bftm0_ct % 100 == 0 && last_bftm0_ct_ecConfig != bftm0_ct){
						last_bftm0_ct_ecConfig = bftm0_ct;
					}
					eCooker_Configuration();
				}

				GPTM0_ProcessEvents();

				if (currentState == STATE_FACTORY) {
						if (isGSMbridge) {
							while (isGSMbridge) {
								cli(HT_USART1);
							}
						} else {
								cli(HT_USART1);
						}
				}


				if(ecConfig){
					if (!left.error && !right.error) {
						HdispSwitch();
					}
					updateLEDs();
				}

				if (currentState == STATE_COOKING || currentState == STATE_LOCK) {
					if ((bftm0_ct % 200 == 0) && (last_bftm0_ct_err != bftm0_ct)) {
						last_bftm0_ct_err = bftm0_ct;
						if (left.error || right.error) {
							bool selectedHasError = (bool)((selectedPan == 0 && left.error) || (selectedPan == 1 && right.error));
							if (selectedHasError && eCooker.numberOfPlates == DOUBLE_STOVE) {
								uint8_t code = (selectedPan == 0) ? I2C_Rx_Data[2] : I2C1_Rx_Data[2];
								if (code != 8) writeErrCodeToSevenSeg(code);
							} else if (eCooker.numberOfPlates == DOUBLE_STOVE) {
								writePowerLevel(left.power, right.power);
								LEDC_setSingleRedLED(selectedPan == 0 ? left.power : right.power);
							} else {
								if (I2C_Rx_Data[2] != 8) writeErrCodeToSevenSeg(I2C_Rx_Data[2]);
							}
						}

						if (prev_left_error && !left.error) {
							writePowerLevel(left.power, right.power);
							LEDC_setSingleRedLED(selectedPan == 0 ? left.power : right.power);
							if (left.power != 0) startSessionForSelectedPan(0, left.power);
						}
						if (prev_right_error && !right.error) {
							writePowerLevel(left.power, right.power);
							LEDC_setSingleRedLED(selectedPan == 1 ? right.power : left.power);
							if (right.power != 0) startSessionForSelectedPan(1, right.power);
						}
						prev_left_error = left.error;
						prev_right_error = right.error;
					}
				}

				syncSwitch();
				batlvlSwitch();
				atecdispSwitch();
				plateConfigSwitch();	
				checkShutoff();
				updateSwitch();
				fileSwitch();
				update();
				GPTM0_ProcessEvents();

				if(updateAnimationActive && currentState == STATE_ONOFF){
					runLoadingAnimationNonBlocking();
				}

				isGSM = isGSMpoweredUp();
				sendPayload();
				payloadSend();
				
				if(HT_CKCU->APBCCR1 & 0x00000010){
					WDT_Restart();
				}
				
				/* Sleep gates the UART clock; the cook +CMQTTPUB result would be lost. */
				if (!cookPublishBusy()) {
					DeepSleepMode_Configuration();
				}
				process_uart_json_messages();
				checkMQTTConnection();
				networkInitCheck();
				connCheck();
				mqttcheckStart();
				GPTM0_ProcessEvents();

				//-----------------------------------------------------------------------
				/* I2C bus-idle detection only applies after a stove master was seen.
				 * Without I2C hardware, floating lines stay high and must not block
				 * factory mode or battery check in STATE_ONOFF. */
				if (isStoveI2cConnected() || ecConfig) {
					if (i2c0SclGPIO && i2c0SdaGPIO && i2c1SclGPIO && i2c1SdaGPIO) {
						if (gpioHighDuration < UINT32_MAX) {
							gpioHighDuration += 1;
						}
					} else {
						gpioHighDuration = 0;
					}

					turnOff = (gpioHighDuration >= 1000) ? true : false;
				} else {
					gpioHighDuration = 0;
					turnOff = false;
				}
				
				if( EVENT_ON_STATUS == true )
				{
					if (!g_check_cook_publish &&
					    currentConnState != CONNECTING && currentConnState != NETWORK_INIT) {
						sendToGSMAndPrintResponse(HT_USART1, "AT", "OK");
					}
					if (currentConnState == NETWORK_READY) {
						/* Cellular already up — start MQTT now that stove is on. */
						handleConnect(CONNECT);
					} else if (currentConnState == NETWORK_INIT) {
						/* Bring-up still in progress; do not restart NETWORK_INIT. */
					} else if (currentConnState == DISCONNECTED) {
						/* Only if cellular never came up (e.g. boot still pending). */
						connDelay(10);
					} else if (currentConnState == CONNECTED && currentState != STATE_ONOFF) {
						handleSyncLED(LED_STEADY, 'g');
					}
					EVENT_ON_STATUS = false;
				}
				
				if( EVENT_OFF_STATUS == true )
				{
						if (g_params.conn_strategy == CONN_ALWAYS_ON &&
								(!keep_alive_changed || !persistent_flag_changed))
						{
								// Equivalent to: (void)0;
						}
						else
						{
								if (Emulated_EEPROM_Read_Cooking_Session_Num() > 0)
								{
									switch(g_params.payload_mode)
									{
										case PAYLOAD_ALL:
											/* Don't restart send cycle while waiting for MQTT publish ack - prevents duplicate payloads */
											if (!g_check_cook_publish) {
												unsigned long elapsed = (last_event_off_payload_devt > 0) ? (device_time - last_event_off_payload_devt) : EVENT_OFF_PAYLOAD_DEBOUNCE_S;
												if (elapsed >= EVENT_OFF_PAYLOAD_DEBOUNCE_S) {
													last_event_off_payload_devt = device_time;
													payloadDelay();
												}
											}
											break;
										case PAYLOAD_ESSENTIAL:
											if (!g_check_cook_publish) {
												(void)sendCookingSessionsAggregated();
											}
											break;
									}
								}
							else
							{
								mqttFullTeardown();
							}
						}
					/* If a cook upload was queued, Payload.c sends CSCLK when it finishes. */
					if (!cookPublishBusy()) {
						Deepsleep_Start_Flag = sendToGSMAndPrintResponse(HT_USART1,"AT+CSCLK=2","OK");
					}
					EVENT_OFF_STATUS = false;
				}
				
				if( CONNECT_COMMAND_STATUS == true )
				{
					if (currentConnState == CONNECTED) {
						handleSyncLED(LED_STEADY, 'g');
						runBlockingAnimation();
						if (!cookPublishBusy()) {
							subscribeTopic();
						}
					} else if (currentState != STATE_FACTORY) {
						handleSyncLED(LED_BLINK, 'r');
						writeLineToSevenSeg();
						buzz(5);
					}
					syncDelay();
					CONNECT_COMMAND_STATUS = false;
				}
				GPTM0_ProcessEvents();

				/* Cellular registration once after MCU/GSM boot — not again on turn-off/DISCONNECTED.
				 * MQTT connect remains gated by mqttConnectAllowed() after NETWORK_READY. */
				{
					static bool gsm_was_powered = false;
					static bool gsm_boot_net_init_done = false;
					bool gsm_up_now = isGSMpoweredUp();
					if (!gsm_up_now) {
						gsm_was_powered = false;
						/* Allow NETWORK_INIT again only after a real GSM power loss. */
						gsm_boot_net_init_done = false;
					} else if (currentState != STATE_FACTORY &&
					           !gsm_was_powered &&
					           currentConnState == DISCONNECTED &&
					           !gsm_boot_net_init_done) {
						connDelay(5);
						gsm_boot_net_init_done = true;
					}
					gsm_was_powered = gsm_up_now;
				}

				if(reset && isGSM){
					updateDelay();
					reset=false;
				}
				
/*************** EPOCH Time Function - Sync and Validation *******************************************************************/				
				if(isGSMpoweredUp()){
					const unsigned long MIN_VALID_EPOCH = 1735689600UL; // Jan 1, 2025 00:00:00 UTC
					const unsigned long MAX_VALID_EPOCH = 4102444800UL; // Jan 1, 2100 00:00:00 UTC (reasonable upper bound)
					bool epoch_valid = false;
					
					if(g_clbs_epoch_valid && g_clbs_epoch >= MIN_VALID_EPOCH && g_clbs_epoch <= MAX_VALID_EPOCH){
						bool current_epoch_invalid = false;
						if(epoch_time <= 0 || epoch_time < MIN_VALID_EPOCH || epoch_time > MAX_VALID_EPOCH){
							current_epoch_invalid = true;
						} else {
							snprintf(epochStr, sizeof(epochStr), "%lu", epoch_time);
							epochLength = strlen(epochStr);
							if(epochLength < IDEAL_EPOCH_CHAR_SIZE){
								current_epoch_invalid = true;
							}
						}
						if(current_epoch_invalid){
							epoch_time = g_clbs_epoch;
						}
					}
					
					if(!g_check_cook_publish && !__init_sync_time__ && currentState != STATE_FACTORY &&
					   (currentConnState == NETWORK_READY || currentConnState == CONNECTED) &&
					   device_time%5 == 0 && device_time != last_devt_epoch_sync){
						last_devt_epoch_sync = device_time;
						/* Net-init CLBS may already have set epoch — avoid a parallel AT+CLBS. */
						if (g_clbs_epoch_valid && g_clbs_epoch >= MIN_VALID_EPOCH && g_clbs_epoch <= MAX_VALID_EPOCH) {
							epoch_time = g_clbs_epoch;
						} else {
							__init__RTCEpoch();
						}
					}
					
					if(epoch_time > 0){
						snprintf(epochStr, sizeof(epochStr), "%lu", epoch_time);
						epochLength = strlen(epochStr);
						
					if(epoch_time >= MIN_VALID_EPOCH && epoch_time <= MAX_VALID_EPOCH && epochLength >= IDEAL_EPOCH_CHAR_SIZE){
							epoch_valid = true;
						}
					}
					
					if(epoch_valid){
						if(!__init_sync_time__){
							synced_epoch_time = epoch_time;
							deviceInteractionIdle();
							__init_sync_time__ = true;
						} else {
							synced_epoch_time = epoch_time;
						}
					}
					else if(epoch_time > 0){
						epoch_time = device_time; // Reset to device_time as fallback
					}
				}				

/*************************** ICCID code *****************************************/				
				if(!g_check_cook_publish && !__init_iccid__ && currentState != STATE_FACTORY && currentConnState != CONNECTING && currentConnState != NETWORK_INIT && isGSMpoweredUp() && device_time%5 == 0 && device_time != last_devt_iccid){
					last_devt_iccid = device_time;
					iccid = geticcid();
					if(iccid == NULL){
						globalPayload.devID.sid = "NA";
						__init_iccid__ = false;
					} else {
						iccidLength = strlen(iccid);
						if(iccidLength != IDEAL_ICCID_CHAR_SIZE){
							__init_iccid__ = false;
						}
						else{
							get_first_20_chars(iccid, temp);						
							if(are_all_digits(temp)){
								__init_iccid__ = true;
								globalPayload.devID.sid = temp;
							}
							
							else{
								globalPayload.devID.sid = "NA";
								__init_iccid__ = false;
							}
						}
					}
				}
/************************************************************************************************/				
				
				//THIS IS THE CODE FOR THE GPS LOCATION
				if(!g_check_cook_publish && !clbs.success && currentState != STATE_FACTORY && currentConnState == CONNECTED && isGSMpoweredUp() && device_time%5 == 0 && device_time != last_devt_clbs && __init_iccid__){
					last_devt_clbs = device_time;
					clbs = extractCoordinatesFromCLBS();
					if (clbs.success && clbs.coordinates != NULL) {
						temp2 = clbs.coordinates;
						globalPayload.devID.loc = temp2;
					}
				}
				if(!clbs.success)
					globalPayload.devID.loc = "NA";
				
				/* Meta is owned by sendPayload() — piggybacks on existing MQTT only (no wake while OFF). */
//-----------------------------------------------------------------------------//
					
				if ((gptm0_ct != last_gptm0_ct) && (currentState != STATE_ONOFF)) {
					uint32_t elapsed_time = gptm0_ct - last_gptm0_ct;
					if (gpio_state == STATE_ON && elapsed_time >= ON_TIME) {		// Turn off GPIOs after ON_TIME has elapsed
						GPIO_ClearOutBits(HT_GPIOB, GPIO_PIN_13);
						GPIO_ClearOutBits(HT_GPIOB, GPIO_PIN_14);
						gpio_state = STATE_OFF;
						last_gptm0_ct = gptm0_ct; // Reset the timer
						
					} else if (gpio_state == STATE_OFF && elapsed_time >= OFF_TIME) { // Turn on GPIOs after OFF_TIME has elapsed			
						GPIO_SetOutBits(HT_GPIOB, GPIO_PIN_13);
						GPIO_SetOutBits(HT_GPIOB, GPIO_PIN_14);
						gpio_state = STATE_ON;
						last_gptm0_ct = gptm0_ct; // Reset the timer					
					}
				}
		
				/* Safety guard: zero power command when stove is off or power is 0.
				 * Done here (main loop) to avoid data-race with ISR writes. */
				if (currentState == STATE_ONOFF || left.power == 0) {
					I2C_Tx_Data[1] = 0;
				}
				if (currentState == STATE_ONOFF || right.power == 0) {
					I2C1_Tx_Data[1] = 0;
				}
				i2cSlaveSend2(I2C_Tx_Data, 8);
				i2c_1_SlaveSend2(I2C1_Tx_Data,8);
				/* i2cSlaveReceive / i2c_1_SlaveReceive moved to BFTM0_IRQHandler (1ms tick)
				 * so I2C_Rx_Data and I2C1_Rx_Data are refreshed every 1ms instead of
				 * once per main loop iteration, reducing display update latency. */

				 for(u8 i=0;i<11;i++){
						CRC_RX_I2C[i] = I2C_Rx_Data[i];
						CRC_RX_I2C1[i] = I2C1_Rx_Data[i];
				 }
				 
				if (((I2C_Rx_Data[15] == Rx_Data_Sum) || (Rx_Data_Sum==0)) && currentState != STATE_ONOFF  && currentState != STATE_FACTORY && !turnOff) {
					if ((currentState == STATE_COOKING || currentState == STATE_LOCK) && (I2C_Rx_Data[2] == 0 || I2C_Rx_Data[2] == 1 || I2C_Rx_Data[2] == 2 || I2C_Rx_Data[2] == 3 || I2C_Rx_Data[2] == 4 || I2C_Rx_Data[2] == 5 || I2C_Rx_Data[2] == 6 || I2C_Rx_Data[2] == 7)) {//Li's Changes
							if (selectedPan == 0 || eCooker.numberOfPlates == SINGLE_STOVE) {
								writeErrCodeToSevenSeg(I2C_Rx_Data[2]);
							} else {
								writePowerLevel(left.power, right.power);
							}
								if (!leftErr.errorTimerActive) {
										leftErr.last_device_time_err = device_time;
										leftErr.errorTimerActive = true;
								}
								else if (((device_time - leftErr.last_device_time_err) >= ERROR_STATE_THRESHOLD) && !leftErr.errorSent) {
										addErrC(epoch_time, I2C_Rx_Data[2]);
										leftErr.errorSent = true;
										leftErr.errorTimerActive=false;
								}
						} else {
								leftErr.last_device_time_err = -1;
								leftErr.errorTimerActive=false;
								leftErr.errorSent = false;
						}
				}
			
				if(eCooker.numberOfPlates == 2){
					Rx_Data_Sum = 0;
					for (int i_caculate = 0; i_caculate < 15; i_caculate++) {	//Li's Changes
							Rx_Data_Sum += I2C1_Rx_Data[i_caculate];
					}
				
			if (((I2C1_Rx_Data[15] == Rx_Data_Sum) || (Rx_Data_Sum==0)) && currentState != STATE_ONOFF && currentState != STATE_FACTORY && !turnOff) {	// Li's Change
					if ((currentState == STATE_COOKING || currentState == STATE_LOCK) && (I2C1_Rx_Data[2] == 0 || I2C1_Rx_Data[2] == 1 || I2C1_Rx_Data[2] == 2 || I2C1_Rx_Data[2] == 3 || I2C1_Rx_Data[2] == 4 || I2C1_Rx_Data[2] == 5 || I2C1_Rx_Data[2] == 6 || I2C1_Rx_Data[2] == 7)) { //Li's Changes
												if (selectedPan == 1) {
								writeErrCodeToSevenSeg(I2C1_Rx_Data[2]);
							} else {
								writePowerLevel(left.power, right.power);
							}
							addErrC(epoch_time, I2C1_Rx_Data[2]);
								if (!rightErr.errorTimerActive) {
										rightErr.last_device_time_err = device_time;
										rightErr.errorTimerActive = true;
								} else if (((device_time - rightErr.last_device_time_err) >= ERROR_STATE_THRESHOLD) && !rightErr.errorSent) {
										addErrC(epoch_time, I2C1_Rx_Data[2]);
										rightErr.errorSent = true;									
										rightErr.errorTimerActive=false;
								}
						} else {
								rightErr.last_device_time_err = -1;
								rightErr.errorTimerActive=false;
								rightErr.errorSent = false;
						}
				}
			}


/*********************************************************************************************************//**
 *  Below code is written to handle master i2c0 on/off condition, based on mains supply availability
 *  if mains power supply is abruptly removed / loadshedding / power supply cut off this will identify
 *  master i2c0 stove unavailability and turn off the display as well as other UI peripherals,
 *  to avoid this messy code use a hardware pin read to check if there is any input 5V DC supply is available 
 *************************************************************************************************************/	
			
			
			if ((bftm0_ct % 1000 == 0) && (last_bftm0_ct_i2c_mains != bftm0_ct)) {
				last_bftm0_ct_i2c_mains = bftm0_ct;
				i2c0_status();
				(eCooker.numberOfPlates == DOUBLE_STOVE) ? i2c1_status() : (void)0;
			}
			
			if ((bftm0_ct % 250 == 0) && (last_bftm0_ct_i2c_mains != bftm0_ct)) {
				last_bftm0_ct_i2c_mains = bftm0_ct;
				(main_off_count != i2c0_master_off) && (eCooker.numberOfPlates==DOUBLE_STOVE ? (main_off_count != i2c1_master_off):true) ? main_off_count++ : (int)0;
				main_off_flag = (main_off_count >= 3) ? (main_off_count = 0, true)  : false;
			}		
			
//				if(currentState == STATE_ONOFF &&  turnOff){
//						clearSevenSegDisplay();
//						HTriggered = true;
//				}
				if(currentState != STATE_ONOFF &&  turnOff/*(main_off_flag || main_off_count)*/ && currentState != STATE_FACTORY){//i2c0_master_off){
					handlePaygoLED(LED_OFF, 'g');
					handleSyncLED(LED_OFF, 'g');

					HTriggered = true;					
					clearSevenSegDisplay();
					
					LEDC_redLED(POWER_KEY, false);
					LEDC_redLED(R_PLATE, false);
					LEDC_redLED(L_PLATE, false);
					LEDC_redLED(TIMER_KEY, false);
					LEDC_redLED(LOCK_KEY, false);
					LEDC_turnOffAllRedLEDs();
				
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
					
					for(int i =0; i<4; i++){
							left.timerDigits[i] = 0;
							right.timerDigits[i] = 0;
					}
					
					closeSession(&leftPanSession);  
					closeSession(&rightPanSession);
					
				enterState(STATE_ONOFF);
				if (!cookPublishBusy()) {
					if (g_params.conn_strategy != CONN_ALWAYS_ON) {
						mqttFullTeardown();
					}
					Deepsleep_Start_Flag = sendToGSMAndPrintResponse(HT_USART1,"AT+CSCLK=2","OK");
				}
	}
//*************************************************************************************************************/
		
				if(leftPanSession.active || rightPanSession.active){
				 for(int i=0;i<11;i++){
						CRC_RX_I2C[i] = I2C_Rx_Data[i];
						CRC_RX_I2C1[i] = I2C1_Rx_Data[i];
				 }
				 CRC_RX_I2C_VAL = CRC_16(seed,CRC_RX_I2C, sizeof(CRC_RX_I2C));
				 CRC_RX_I2C1_VAL = CRC_16(seed,CRC_RX_I2C1, sizeof(CRC_RX_I2C1));
	
				// After CRC checks pass and you have raw fields in I2C_Rx_Data / I2C1_Rx_Data:
				if(((leftPanSession.active && (CRC_RX_I2C_VAL == ( I2C_Rx_Data[11] << 8 | I2C_Rx_Data[12] ))) || (rightPanSession.active && (CRC_RX_I2C1_VAL == ( I2C1_Rx_Data[11] << 8 | I2C1_Rx_Data[12] ))))){
				float ag_inst = 0.0f;
				if ((I2C_Rx_Data[5] != 0) || (I2C1_Rx_Data[5] != 0)) {
					if (eCooker.numberOfPlates == DOUBLE_STOVE) {
						float ag0 = (( (I2C_Rx_Data[13]  << 8) | I2C_Rx_Data[5]  ) / 256.0f);
						float ag1 = (( (I2C1_Rx_Data[13] << 8) | I2C1_Rx_Data[5] ) / 256.0f);
						ag_inst = ag0 + ag1;
					} else {
						ag_inst = (( (I2C_Rx_Data[13] << 8) | I2C_Rx_Data[5] ) / 256.0f);
					}
				}
				uint16_t vg_raw0 = ((I2C_Rx_Data[3]  << 8) | I2C_Rx_Data[4]);
				uint16_t vg_raw1 = ((I2C1_Rx_Data[3] << 8) | I2C1_Rx_Data[4]);
				float vg_inst = (vg_raw0 != 0) ? (float)vg_raw0 : (float)vg_raw1;
				// FEED the pair to the averager (this call updates globalPayload with averages)
				pg_accept_sample(ag_inst, vg_inst);
			}
		}
			 
		 i2c0SclGPIO = GPIO_ReadInBit(HT_GPIOC, GPIO_PIN_8);
		 i2c0SdaGPIO = GPIO_ReadInBit(HT_GPIOC, GPIO_PIN_9);
		 i2c1SclGPIO = GPIO_ReadInBit(HT_GPIOB, GPIO_PIN_7);
		 i2c1SdaGPIO = GPIO_ReadInBit(HT_GPIOB, GPIO_PIN_8);

		 pb2_status = (GPIO_ReadInBit(HT_GPIOB, GPIO_PIN_2) == SET) ? true : false;  /* stove on/off status from PB2 */
		
		
        if (HT_CKCU->APBCCR1 & (1 << 4)) {
            WDT_Restart();
        }
    }
}
