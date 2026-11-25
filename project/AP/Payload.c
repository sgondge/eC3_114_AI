#include "Payload.h"
#include "UART0.h"
#include <string.h>
#include <stdlib.h>
#include "Utils.h"
#include "EEPROM.h"
#include "BFTM0.h"
#include "cli.h"
#include "LEDC.h"
#include "eCooker.h"
#include "RTC.h"
#include "FSM.h"
#include "CRC.h"
#include "stove_stats.h"
#include "ota_commands.h"
#include "buzzer.h"
#include "gsm.h"
#include <stdio.h>

#define EVERY_10_SEC 10
#define EVERY_MIN 60
#define EVERY_15_MIN (60*15)
#define EVERY_30_MIN (60*30)
#define EVERY_6_HRS (60*60*6)
#define EVERY_12_HRS (60*60*12)
#define EVERY_24_HRS (60*60*24)

#define MAX_RETRY 5
#define CHECK_INTERVAL 20
#define MAX_SESSION_DURATION_SEC 7200U
#define MAX_MQTT_PUBLISH_RETRIES 5

#define GSM_APN_ONOMONDO "onomondo"
#define GSM_APN_AERIS "iot-eu.aer.net"
#define ONOMONDO_ICCID_PREFIX "894573"
#define NETWORK_INIT_STEP_RETRY_MAX 5
#define NETWORK_INIT_STEP_DELAY_S 2
#define NETWORK_INIT_ATTACH_BACKOFF_S 30
#define NETWORK_RADIO_RESET_MAX 2

extern uint16_t failpub;
void storeFailedEraseRest(void);
void MQTT_CheckPublishCook(void);
static void cookPublishFailed(void);
static void cookPublishAcked(void);

/* One cook attempt: do not republish if OK is missing. */
static bool publish_cook_once = false;
static uint16_t g_cook_chunk_size = 0;
extern uint32_t g_cook_publish_check_start_time;
/* gsm_mqtt_pub_seq just before the cook AT+CMQTTPUB; any later +CMQTTPUB line is this publish's result. */
static uint16_t cook_pub_seq = 0;

/* After MAX_RETRY_PUBLISH_RESP misses, leave the head in flash and wait before trying again. */
#define COOK_UPLOAD_BACKOFF_S 600UL
static bool cook_upload_stopped = false;
static unsigned long cook_upload_stop_devt = 0;

static bool publishMQTTWithRetry(const char* payload, int payload_len) {
    int retry_count = 0;
    bool success = false;
    int max_tries = publish_cook_once ? 1 : MAX_MQTT_PUBLISH_RETRIES;
    bool cook = publish_cook_once;
    
    while (retry_count < max_tries && !success) {
        UART0_ClearRxBuffer();
        if (!sendToGSMAndPrintResponse(HT_USART1, "AT+CMQTTTOPIC=0,25", ">")) {
            retry_count++;
            continue;
        }
        if (!sendToGSMAndPrintResponse(HT_USART1, "usage-raw-data-topic-prod", "OK")) {
            retry_count++;
            continue;
        }
        char command[30];
        snprintf(command, sizeof(command), "AT+CMQTTPAYLOAD=0,%d", payload_len);
        if (!sendToGSMAndPrintResponse(HT_USART1, command, ">")) {
            retry_count++;
            continue;
        }
        if (!sendToGSMAndPrintResponse(HT_USART1, payload, "OK")) {
            retry_count++;
            continue;
        }
        if (cook) {
            cook_pub_seq = gsm_mqtt_pub_seq;
        }
        if (!sendToGSMAndPrintResponse(HT_USART1, "AT+CMQTTPUB=0,1,60", "OK")) {
            if (cook) {
                /* The publish command was written. Wait for +CMQTTPUB rather than sending it again. */
                return true;
            }
            retry_count++;
            continue;
        }
        success = true;
    }
    
    return success;
}

static bool publishCookMQTT(const char* payload, int payload_len) {
    bool ok;
    publish_cook_once = true;
    ok = publishMQTTWithRetry(payload, payload_len);
    publish_cook_once = false;
    /* The modem's 60 s publish timer starts at AT+CMQTTPUB, not at chunk build. */
    g_cook_publish_check_start_time = device_time;
    return ok;
}

char fw_url_buf[256] = "";
const char* fw_url = fw_url_buf;

#define STL_BUF_SIZE 32
static char global_payload_stl_buf[STL_BUF_SIZE];

const u16 POWER_LEVEL[2][11] = {{0, 300, 400, 600, 700, 900, 1100, 1300, 1500, 1600, 0},
													{0, 300, 400, 500, 600, 700, 800, 860, 940, 1000, 1650}};

uint32_t ncook = 0;
uint32_t nerr = 0;
Payload globalPayload;

bool payloadTriggered = false;
static uint32_t payload_bftm0 = 0;
int payloadOffset=0;
bool payloadSent = false;
																										
unsigned long last_rtc_minute = 0;
unsigned long last_rtc_6hrs = 0;
unsigned long last_rtc_24hrs = 0;

unsigned long send_last_rtc_30minute = 0;
unsigned long send_last_rtc_30minute_err = 0;

unsigned long send_last_rtc_6hrs = 0;
unsigned long send_last_rtc_12hrs = 0;
unsigned long send_last_rtc_meta = 0;

int connect_bftm0_ct = 0;
void connect(void);
void freeCookSessions(void);
void freeErr(void);
unsigned char CRC_RX[11] = {0};
unsigned int CRC_RX_value = 0;

enum PayloadFSMState {
    PAYLOAD_IDLE,
    PAYLOAD_SEND_24HRS,
    PAYLOAD_SEND_ERRORS,
    PAYLOAD_SEND_SESSIONS,
    PAYLOAD_DONE
};

enum PayloadFSMState payloadState = PAYLOAD_IDLE;

bool connTriggered = false;
//static uint32_t conn_bftm0 = 0;

bool isSubscribedTopic = false;

bool mqtt_service_started = false;

bool downloadInProgress = false;

uint32_t conn_devt = 0;
uint32_t conn_devt_delay = 5;
uint32_t connStartTime = 0;
uint32_t connRetryCount = 0;

bool resetmodem = false;

/* Set by OTA "mr" to force an immediate meta send (does not wait for interval). */
static bool meta_force_request = false;
static unsigned long last_meta_attempt_devt = 0;

static bool metaIntervalElapsed(void)
{
	/* 0 means never sent this boot — first meta is due immediately. */
	if (send_last_rtc_meta == 0UL) {
		return true;
	}
	return ((device_time - send_last_rtc_meta) >= (unsigned long)g_params.meta_payload_interval_s) ? TRUE : FALSE;
}

static bool metaPayloadDue(void)
{
	/* OTA "mr" forces a send even if sendMeta was disabled. */
	if (meta_force_request) {
		return true;
	}
	if (!g_params.send_meta_payloads) {
		return false;
	}
	/* Interval is always device_time-based (works with or without epoch sync). */
	return metaIntervalElapsed();
}

/** Advance meta timer only after a successful publish. */
static void metaMarkSent(void)
{
	send_last_rtc_meta = device_time;
	meta_force_request = false;
}

bool mqttConnectAllowed(void)
{
	/* Never start MQTT while stove is off or in factory — meta must piggyback. */
	return (bool)(currentState != STATE_ONOFF && currentState != STATE_FACTORY);
}

void factoryConnHold(void)
{
	/* Local freeze only — do not send CMQTT/CFUN teardown ATs in factory. */
	connTriggered = true;
	conn_devt = 0;
	connRetryCount = 0;
	mqtt_service_started = false;
	isSubscribedTopic = false;
	payloadTriggered = true;
	meta_force_request = false;
	networkInitRestart();
	enterConnState(DISCONNECTED);
}

void connDelay(uint32_t delay){
	if (currentState == STATE_FACTORY) {
		return;
	}
	connTriggered = false;
	conn_devt = device_time;
	conn_devt_delay = delay;
}

void payloadDelay(void){
	payloadSent = true;
	payloadTriggered = false;
	payload_bftm0 = bftm0_ct;
}

/* A cook chunk is queued or waiting for +CMQTTPUB. No other publish/AT may run. */
bool cookPublishBusy(void){
	return (bool)(g_check_cook_publish || (!payloadTriggered && payload_bftm0 > 0));
}

/* Stove-off AT that main.c skipped while a cook publish was in flight. */
static void cookUploadIdleOff(bool teardown){
	if (currentState != STATE_ONOFF) {
		return;
	}
	if (teardown) {
		mqttFullTeardown();
	}
	Deepsleep_Start_Flag = sendToGSMAndPrintResponse(HT_USART1, "AT+CSCLK=2", "OK");
}


// Initialize globalPayload structure to prevent hardfaults
void initGlobalPayload(void) {
    globalPayload.errC = NULL;
    globalPayload.Cook = NULL;
    globalPayload.status = NULL;
    globalPayload.devID.sn = 0;
    globalPayload.devID.pt = 0;
    globalPayload.devID.mc = 0;
    globalPayload.devID.sid = NULL;
    globalPayload.devID.loc = NULL;
    globalPayload.devID.FW = NULL;
    globalPayload.ts = 0;
    globalPayload.stat.Vg = 0;
    globalPayload.stat.Ag = 0.0f;
    globalPayload.stat.Pg = 0.0f;
    globalPayload.stat.BatV = 0.0f;
    memset(global_payload_stl_buf, 0, sizeof(global_payload_stl_buf));
    globalPayload.stat.STL = global_payload_stl_buf;
    globalPayload.stat.stvcnt = 0;
    globalPayload.GSMsignalStrength = 0;
}

//void connCheck(void){
//					if (!connTriggered && conn_bftm0 > 0) { 
//            if ((bftm0_ct >= conn_bftm0 + 6500)) {
//							connTriggered = true;
//							if(PayloadOption == 0)
//								sendToGSMAndPrintResponse(HT_USART1, "AT+CMQTTCONNECT?","tcp://a3b6socfqdtpq4-ats.iot.ap-southeast-1.amazonaws.com:8883") ? (currentState==STATE_ONOFF?(void)0: (handleSyncLED(LED_STEADY, 'g'), subscribeTopic())) : ((currentState==STATE_ONOFF?(void)0: handleSyncLED(LED_BLINK, 'r')),handleConnect(CONNECT));
//							else
//								sendToGSMAndPrintResponse(HT_USART1, "AT+CMQTTCONNECT?","tcp://a3b6socfqdtpq4-ats.iot.ap-southeast-1.amazonaws.com:8883") ? (currentState==STATE_ONOFF?(void)0: (handleSyncLED(LED_STEADY, 'g'), subscribeTopic())) : ((currentState==STATE_ONOFF?(void)0: handleSyncLED(LED_STEADY, 'g')),handleConnect(CONNECT));
//            }
//        }
//}


void connCheck(void){
					if (currentState == STATE_FACTORY) {
						return;
					}
					if (!connTriggered && conn_devt > 0) { 
            uint32_t elapsed = device_time - conn_devt;
            if (elapsed >= conn_devt_delay) {
									connTriggered = true;
							    connRetryCount = 0;
									connStartTime = device_time;
									handleConnect(CONNECT);
							}
						}
			/* CONNECTING is completed by checkMQTTConnection(); do not CFUN or restart NETWORK_INIT here. */
}

typedef enum {
	NET_STEP_BOOT_QUIET,
	NET_STEP_AT_PROBE,
	NET_STEP_CPIN,
	NET_STEP_ICCID,
	NET_STEP_FPLMN, /* every network init after ICCID */
	NET_STEP_CSQ,
	NET_STEP_APN,
	NET_STEP_CGATT_SET, /* skipped: AT+CGATT=1 blocks the AT channel */
	NET_STEP_CGATT_CHECK,
	NET_STEP_CREG,
	NET_STEP_CPSI,
	NET_STEP_PDP_ACTIVE,
	NET_STEP_PDP_VERIFY,
	NET_STEP_NETOPEN,
	/* Best-effort CLBS after NETOPEN — not in parallel with registration AT cmds. */
	NET_STEP_CLBS
} NetworkInitStep;

static NetworkInitStep network_init_step = NET_STEP_BOOT_QUIET;
static uint8_t network_init_retry = 0;
static uint32_t network_init_last_attempt = 0;
static uint32_t network_init_step_delay_s = NETWORK_INIT_STEP_DELAY_S;
static uint8_t network_radio_reset_count = 0;
static bool network_boot_quiet_done = false;
static char gsm_selected_apn[16] = GSM_APN_ONOMONDO;

static bool network_step_is_attach_failure(NetworkInitStep step) {
	/* Attach is polled with CGATT? / CREG. Do not treat CGATT=1 as attach
	 * failure — that command holds the AT channel for ~75s and blocked CREG. */
	return (bool)(step == NET_STEP_CGATT_CHECK ||
	              step == NET_STEP_CREG);
}

static void network_radio_reset(void) {
	gsmCfun0Then1();
	resetmodem = true;
	mqtt_service_started = false;
	networkInitRestart();
}

static void network_attach_backoff(void) {
	network_init_retry = 0;
	network_init_step_delay_s = NETWORK_INIT_ATTACH_BACKOFF_S;
}

/* After a failed attach/reg sequence: CFUN and restart bring-up. */
static void network_on_attach_failed(void) {
	network_radio_reset();
}

void networkInitRestart(void) {
	network_init_step = NET_STEP_BOOT_QUIET;
	network_init_retry = 0;
	network_init_last_attempt = 0;
	network_init_step_delay_s = NETWORK_INIT_STEP_DELAY_S;
	network_boot_quiet_done = false;
	strlcpy0(gsm_selected_apn, GSM_APN_ONOMONDO, sizeof(gsm_selected_apn));
}

void networkAllowFplmnAfterStoveOn(void) {
	/* Allow another EF_FPLMN CRSM write this GSM cycle if needed. */
	gsmAllowFplmnWriteAfterGsmReboot();
}

static bool network_parse_csq(const char *resp, uint32_t *rssi_out) {
	const char *p = strstr(resp, "+CSQ:");
	if (!p) {
		return false;
	}
	int rssi = 99;
	int ber = 99;
	if (sscanf(p, "+CSQ: %d,%d", &rssi, &ber) != 2) {
		return false;
	}
	if (rssi_out) {
		*rssi_out = (uint32_t)rssi;
	}
	return (rssi != 99) ? true : false;
}

static bool network_parse_reg_stat(const char *resp, int *stat_out) {
	const char *p = strstr(resp, "+CREG:");
	if (!p) {
		p = strstr(resp, "+CGREG:");
	}
	if (!p) {
		return false;
	}
	int n = 0;
	int stat = -1;
	if (sscanf(p, "+CREG: %d,%d", &n, &stat) != 2 &&
	    sscanf(p, "+CGREG: %d,%d", &n, &stat) != 2) {
		return false;
	}
	if (stat_out) {
		*stat_out = stat;
	}
	return (stat >= 0) ? true : false;
}

static bool network_parse_reg(const char *resp) {
	int stat = -1;
	if (!network_parse_reg_stat(resp, &stat)) {
		return false;
	}
	return (stat == 1 || stat == 5) ? true : false;
}

static bool network_parse_cgatt(const char *resp) {
	const char *p = strstr(resp, "+CGATT:");
	if (!p) {
		return false;
	}
	int att = 0;
	return ((sscanf(p, "+CGATT: %d", &att) == 1) && att == 1) ? true : false;
}

static bool network_run_init_step(void) {
	GSMResponse res = {.success = false, .response = NULL};
	bool step_ok = false;

	switch (network_init_step) {
		case NET_STEP_BOOT_QUIET:
			if (!network_boot_quiet_done) {
				/* After CFUN/power-on: wait for a real AT OK, not a cached probe. */
				if (gsmWaitForAtHandshake()) {
					network_boot_quiet_done = true;
					step_ok = true;
				}
			} else {
				step_ok = true;
			}
			break;

		case NET_STEP_AT_PROBE:
			res = GSMsendATCommand("AT");
			step_ok = res.success;
			break;

		case NET_STEP_CPIN:
			res = GSMsendATCommand("AT+CPIN?");
			step_ok = (res.success && res.response &&
			          strstr(res.response, "READY") != NULL) ? true : false;
			break;

		case NET_STEP_ICCID: {
			/* Wait until SIM ICCID is readable, then pick APN by issuer prefix. */
			char *iccid = geticcid();
			if (iccid == NULL || iccid[0] == '\0') {
				step_ok = false;
				break;
			}
			if (strncmp(iccid, ONOMONDO_ICCID_PREFIX, strlen(ONOMONDO_ICCID_PREFIX)) == 0) {
				strlcpy0(gsm_selected_apn, GSM_APN_ONOMONDO, sizeof(gsm_selected_apn));
			} else {
				strlcpy0(gsm_selected_apn, GSM_APN_AERIS, sizeof(gsm_selected_apn));
			}
			step_ok = true;
			break;
		}

		case NET_STEP_FPLMN:
			/* Handled in networkInitCheck before this switch. */
			step_ok = true;
			break;

		case NET_STEP_CSQ: {
			uint32_t rssi = 99;
			res = GSMsendATCommand("AT+CSQ");
			step_ok = (res.success && network_parse_csq(res.response, &rssi)) ? true : false;
			if (step_ok) {
				globalPayload.GSMsignalStrength = rssi;
			}
			break;
		}

		case NET_STEP_APN: {
			/* AT+CGDCONT defines PDP context / APN before attach & activation. */
			char apn_cmd[48];
			snprintf(apn_cmd, sizeof(apn_cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", gsm_selected_apn);
			res = GSMsendATCommand(apn_cmd);
			step_ok = res.success;
			break;
		}

		case NET_STEP_CGATT_SET:
			/* Skip blocking AT+CGATT=1 (modem holds AT until attach, ~75s).
			 * LTE auto-attaches after CFUN/SIM/APN; poll with CGATT?. */
			step_ok = true;
			break;

		case NET_STEP_CGATT_CHECK:
			res = GSMsendATCommand("AT+CGATT?");
			step_ok = (res.success && network_parse_cgatt(res.response)) ? true : false;
			break;

		case NET_STEP_CREG:
			res = GSMsendATCommand("AT+CREG?");
			step_ok = (res.success && network_parse_reg(res.response)) ? true : false;
			if (!step_ok) {
				GSMResponse cgreg = GSMsendATCommand("AT+CGREG?");
				step_ok = (cgreg.success && network_parse_reg(cgreg.response)) ? true : false;
			}
			break;

		case NET_STEP_CPSI:
			res = GSMsendATCommand("AT+CPSI?");
			/* Best-effort: CGACT/NETOPEN verify the data path if CPSI is slow. */
			step_ok = res.success;
			break;

		case NET_STEP_PDP_ACTIVE:
			/*
			 * A76XX manual: in LTE mode, activating/deactivating default bearer
			 * (cid 1) is prohibited — Aeris returns CME #33 ("not subscribed").
			 * After CGATT the default bearer is already up; AT+NETOPEN starts
			 * the TCP/IP service and activates PDP as needed. Skip CGACT write.
			 */
			step_ok = true;
			break;

		case NET_STEP_PDP_VERIFY:
			res = GSMsendATCommand("AT+CGACT?");
			/* If already active, great; otherwise continue to NETOPEN. */
			step_ok = true;
			(void)res;
			break;

		case NET_STEP_NETOPEN: {
			/* AT+NETOPEN starts TCP/IP socket service (activates PDP if needed). */
			GSMResponse q = GSMsendATCommand("AT+NETOPEN?");
			if (q.success && q.response && strstr(q.response, "+NETOPEN: 1") != NULL) {
				step_ok = true;
				break;
			}
			res = GSMsendATCommand("AT+NETOPEN");
			/* Simcom: already-open returns +IP ERROR then ERROR; treat as success. */
			step_ok = (res.success ||
			          (res.response && strstr(res.response, "already opened") != NULL)) ? true : false;
			break;
		}

		case NET_STEP_CLBS: {
			/* Up to CLBS_MAX_MODEM_ATTEMPTS via network_init_retry; failure does not block bring-up. */
			bool ok = fetchCLBSEpochTime();
			CLBSResult r = extractCoordinatesFromCLBS();
			if (r.success && r.coordinates != NULL) {
				globalPayload.devID.loc = r.coordinates;
			}
			if (ok && g_clbs_epoch_valid && g_clbs_epoch > 0) {
				epoch_time = g_clbs_epoch;
			}
			step_ok = ok;
			break;
		}
	}

	return step_ok;
}

void networkInitCheck(void) {
	/* Cellular bring-up is independent of stove state (ONOFF/IDLE/COOKING).
	 * Only factory holds the modem; MQTT remains gated by mqttConnectAllowed(). */
	if (currentState == STATE_FACTORY) {
		return;
	}
	if (currentConnState != NETWORK_INIT) {
		return;
	}
	if (!isGSMpoweredUp()) {
		return;
	}

	if (network_init_last_attempt != 0 &&
	    (device_time - network_init_last_attempt) < network_init_step_delay_s) {
		return;
	}
	network_init_last_attempt = device_time;
	network_init_step_delay_s = NETWORK_INIT_STEP_DELAY_S;

	/* FPLMN list check on every network init (after ICCID). */
	if (network_init_step == NET_STEP_FPLMN) {
		GsmFplmnStatus fplmn = gsm_clear_fplmn();
		if (fplmn == GSM_FPLMN_CLEARED) {
			/* gsm_clear_fplmn already did CFUN=0/1 after writing FPLMN. */
			network_boot_quiet_done = false;
			network_init_step = NET_STEP_BOOT_QUIET;
			network_init_retry = 0;
			network_init_step_delay_s = NETWORK_INIT_STEP_DELAY_S;
			return;
		}
		/* Empty, skipped, or failed write: continue to CSQ without CFUN. */
		network_init_retry = 0;
		network_init_step = NET_STEP_CSQ;
		return;
	}

	if (network_run_init_step()) {
		network_init_retry = 0;
		if (network_init_step >= NET_STEP_CLBS) {
			network_radio_reset_count = 0;
			networkInitRestart();
			/* Network ready regardless of stove state; MQTT only if allowed. */
			if (!mqttConnectAllowed()) {
				enterConnState(NETWORK_READY);
			} else {
				enterConnState(CONNECTING);
				connect();
			}
			return;
		}
		if (network_init_step == NET_STEP_BOOT_QUIET && !network_boot_quiet_done) {
			return;
		}
		/* ICCID → FPLMN → CSQ … ; skip CGATT=1 (APN → CGATT?). */
		if (network_init_step == NET_STEP_APN) {
			network_init_step = NET_STEP_CGATT_CHECK;
		} else {
			network_init_step = (NetworkInitStep)(network_init_step + 1);
		}
		return;
	}

	network_init_retry++;
	{
		/* One pass of CGATT?/CREG per sequence, then CFUN. Other steps keep 5 tries. */
		uint8_t retry_max = network_step_is_attach_failure(network_init_step)
		                    ? (uint8_t)1
		                    : (uint8_t)NETWORK_INIT_STEP_RETRY_MAX;
		if (network_init_retry >= retry_max) {
			network_init_retry = 0;
			if (network_init_step <= NET_STEP_AT_PROBE) {
				network_boot_quiet_done = false;
				network_init_step = NET_STEP_BOOT_QUIET;
			} else if (network_init_step == NET_STEP_CLBS) {
				/* Best-effort: finish bring-up even if CLBS failed after max retries. */
				network_radio_reset_count = 0;
				networkInitRestart();
				if (!mqttConnectAllowed()) {
					enterConnState(NETWORK_READY);
				} else {
					enterConnState(CONNECTING);
					connect();
				}
				return;
			} else if (network_step_is_attach_failure(network_init_step)) {
				network_on_attach_failed();
			} else if (network_init_step == NET_STEP_CSQ) {
				/* CSQ 99 is "RF not ready yet" after CFUN; do not radio-reset on that. */
				network_init_step = NET_STEP_AT_PROBE;
				network_boot_quiet_done = true;
			} else if (network_radio_reset_count < NETWORK_RADIO_RESET_MAX) {
				network_radio_reset_count++;
				network_radio_reset();
			} else {
				network_init_step = NET_STEP_AT_PROBE;
				network_boot_quiet_done = true;
				network_attach_backoff();
			}
		}
	}
}

static uint32_t mqttcheck_bftm0 = 0;
bool mqttcheckTriggered = false;

void mqttcheckDelay(void){
	mqttcheckTriggered = false;
	mqttcheck_bftm0 = bftm0_ct;
}

void payloadSend(void){
					if (currentState == STATE_FACTORY) {
						return;
					}
					if (!payloadTriggered && payload_bftm0 > 0) { 
            if ((bftm0_ct >= payload_bftm0 + 3000)) {
							payloadTriggered = true;
							if(payloadOffset>=Emulated_EEPROM_Read_Cooking_Session_Num()){
							int32_t left;
							payloadOffset=0;
							if (failpub > 0) {
								storeFailedEraseRest();
							}
							left = Emulated_EEPROM_Read_Cooking_Session_Num();
							payloadState = PAYLOAD_IDLE;
							if (left <= 0) {
								payloadSent = false;
								cookUploadIdleOff(true);
							}
								return;
							}
							else{
								sendCookingSessionsChunk(payloadOffset);
								if (!g_check_cook_publish) {
									cookPublishFailed();
								}
							}
            }
        }
}

void mqttcheckStart(void){
					if (currentState == STATE_FACTORY) {
						return;
					}
					if (!mqttcheckTriggered && mqttcheck_bftm0 > 0) { 
            if ((bftm0_ct >= mqttcheck_bftm0 + 6000)) {
							MQTT_CheckPublishCook();
							mqttcheckTriggered = true;
            }
        }
}

void update6Hrs(void) {
		//functionStatus = 62;
    globalPayload.devID.sn = Emulated_EEPROM_Read_Serial_ID();
    globalPayload.devID.pt = eCooker.numberOfPlates;
		/* Do not clear __init_sync_time__ here — that blocked periodic meta sends. */
		
		for(int i=0;i<11;i++)
			CRC_RX[i] = I2C_Rx_Data[i];
		
//		CRC_RX_value = CRC_GetModbus16(CRC_RX, sizeof(CRC_RX));
		CRC_RX_value = CRC_16(seed, CRC_RX, sizeof(CRC_RX));
		if( (((CRC_RX_value >> 8) & 0xFF) == I2C_Rx_Data[11]) && ((CRC_RX_value & 0xFF) == I2C_Rx_Data[12]))
			globalPayload.devID.mc = I2C_Rx_Data[0];

}

void update1Min(void) {
    snprintf(globalPayload.stat.STL, STL_BUF_SIZE, "%dX%dX%d", nconn, ntimer, nlock);
}

void updatePayload(void) {
    if (device_time - last_rtc_minute >= EVERY_MIN) {
        last_rtc_minute = device_time;  
        update1Min();  
    }

    if (device_time - last_rtc_6hrs >= EVERY_6_HRS) {
        last_rtc_6hrs = device_time; 
        update6Hrs();  
    }
}

bool send24Hrs(void) {
	const unsigned long MIN_VALID_EPOCH = 1735689600UL; // Jan 1, 2025 00:00:00 UTC
	const unsigned long MAX_VALID_EPOCH = 4102444800UL; // Jan 1, 2100 00:00:00 UTC
	unsigned long ts;

	/* Prefer CLBS-synced epoch; otherwise send with device_time (seconds since boot). */
	if (__init_sync_time__ &&
			epoch_time >= MIN_VALID_EPOCH && epoch_time <= MAX_VALID_EPOCH) {
		ts = epoch_time;
	} else {
		ts = device_time;
	}

		UART0_reset_tx_buffer(true);
		char batvStr[16];
    char stlStr[32];
    char stvcntStr[16];
	
		globalPayload.devID.sn = Emulated_EEPROM_Read_Serial_ID();
		globalPayload.devID.mc = I2C_Rx_Data[0];
	
		getBatV();
		Bat_CHG = GPIO_ReadInBit(HT_GPIOD, GPIO_PIN_0);							// check battery charging status
				 
    if (globalPayload.stat.BatV >= 0.0f && globalPayload.stat.BatV <= 4.2f) {
        snprintf(batvStr, sizeof(batvStr), "%.2fX%d", globalPayload.stat.BatV, Bat_CHG);
    } else {
        strncpy(batvStr, "NA", sizeof(batvStr));
				batvStr[sizeof(batvStr)-1] = '\0';
    }
		
		snprintf(stlStr, sizeof(stlStr), "%dX%dX%d", nconn, ntimer, nlock);
    snprintf(stvcntStr, sizeof(stvcntStr), "%d", globalPayload.stat.stvcnt);

    // Build JSON without heap allocation (heap corruption was causing hardfaults)
    char payload[512];
    int written = snprintf(payload, sizeof(payload),
        "{\"devID\":{\"sn\":\"%u\",\"FW\":\"%s\",\"pt\":\"%u\",\"mc\":\"%u\",\"sid\":\"%s\",\"loc\":\"%s\"},"
        "\"ts\":%lu,"
        "\"stat\":{\"BatV\":\"%s\",\"STL\":\"%s\",\"stvcnt\":\"%s\"}}",
        (unsigned int)globalPayload.devID.sn,
        globalPayload.devID.FW ? globalPayload.devID.FW : "",
        (unsigned int)globalPayload.devID.pt,
        (unsigned int)globalPayload.devID.mc,
        globalPayload.devID.sid ? globalPayload.devID.sid : "",
        globalPayload.devID.loc ? globalPayload.devID.loc : "",
        ts,
        batvStr,
        stlStr,
        stvcntStr);
    if (written <= 0 || written >= (int)sizeof(payload)) {
        return false;
    }
	
    sendToGSMAndPrintResponse(HT_USART1, "AT", "OK");
    
    if (!publishMQTTWithRetry(payload, written)) {
        return false; // Abort if all retries fail
    }
    
    Deepsleep_Start_Flag = sendToGSMAndPrintResponse(HT_USART1, "AT+CSCLK=2", "OK");

    nconn = 0;
    ntimer = 0;
    nlock = 0;
    globalPayload.stat.stvcnt = 0;
		return true;
}

/** OTA "mr": mark meta due; sendPayload sends on next existing MQTT session (no new connect). */
void metaForceRequest(void)
{
	meta_force_request = true;
}

//bool sendCookingSessions(void){
//    size_t totalCookingTime = 0;
//    for (int i = 0; i < Emulated_EEPROM_Read_Cooking_Session_Num(); i++) {
//        CookSession session = Emulated_EEPROM_Read_Cook_Session(i);
//        totalCookingTime += (session.offT - session.onT);
//    }

//    float Pg = globalPayload.stat.Ag * globalPayload.stat.Vg * totalCookingTime;
//		
//		char batvStr[5];

//		if (globalPayload.stat.BatV >= 0.0f && globalPayload.stat.BatV <= 4.2f) {
//				snprintf(batvStr, sizeof(batvStr), "%.2f", globalPayload.stat.BatV);
//		} else {
//				strncpy(batvStr, "\"NA\"", sizeof(batvStr));
//		}

//    size_t size = snprintf(NULL, 0, 
//        "{\"devID\":{\"sn\":\"%u\",\"FW\":\"%s\"},\"ts\":%d,\"stat\":{\"Vg\":%u,\"Ag\":%.1f,\"BatV\":%s,\"Pg\":%.2f}",
//        Emulated_EEPROM_Read_Serial_ID(),     
//        globalPayload.devID.FW, 
//        getEpochTimeRTC(),         
//        globalPayload.stat.Vg,
//        globalPayload.stat.Ag,
//        batvStr,
//        Pg/3600
//    );

//    size += snprintf(NULL, 0, ",\"Cook\":[");
//    for (int i = 0; i < Emulated_EEPROM_Read_Cooking_Session_Num(); i++) {
//        CookSession session = Emulated_EEPROM_Read_Cook_Session(i);
//        size += snprintf(NULL, 0, 
//            "{\"pan\":\"%c\",\"onT\":%ld,\"offT\":%ld,\"pwr\":%u,\"pwrC\":%.2f}%s", 
//            session.pan,  
//            session.onT,  
//            session.offT, 
//            POWER_LEVEL[eCooker.numberOfPlates-1][session.pwr],
//            ((float)(session.offT - session.onT) * POWER_LEVEL[eCooker.numberOfPlates-1][session.pwr]) / 3600, 
//            (i < Emulated_EEPROM_Read_Cooking_Session_Num() - 1) ? "," : "" 
//        );
//    }

//    size += snprintf(NULL, 0, "]}");

//    char *payload = (char *)malloc(size + 1);

//    if (payload == NULL) {
//        return false;
//    }

//    int offset = snprintf(payload, size + 1, 
//        "{\"devID\":{\"sn\":\"%u\",\"FW\":\"%s\"},\"ts\":%d,\"stat\":{\"Vg\":%u,\"Ag\":%.1f,\"BatV\":%s,\"Pg\":%.2f}",
//        Emulated_EEPROM_Read_Serial_ID(),    
//        globalPayload.devID.FW, 
//        getEpochTimeRTC(),          
//        globalPayload.stat.Vg,   
//        globalPayload.stat.Ag,   
//        batvStr, 
//        Pg/3600
//    );

//    offset += snprintf(payload + offset, size + 1 - offset, ",\"Cook\":[");
//    for (int i = 0; i < Emulated_EEPROM_Read_Cooking_Session_Num(); i++) {
//        CookSession session = Emulated_EEPROM_Read_Cook_Session(i);
//        offset += snprintf(payload + offset, size + 1 - offset, 
//            "{\"pan\":\"%c\",\"onT\":%ld,\"offT\":%ld,\"pwr\":%u,\"pwrC\":%.2f}%s", 
//            session.pan, 
//            session.onT, 
//            session.offT, 
//            POWER_LEVEL[eCooker.numberOfPlates-1][session.pwr],   
//            ((float)(session.offT - session.onT) * POWER_LEVEL[eCooker.numberOfPlates-1][session.pwr]) / 3600, 
//            (i < Emulated_EEPROM_Read_Cooking_Session_Num() - 1) ? "," : ""  
//        );
//    }

//    snprintf(payload + offset, size + 1 - offset, "]}\r\n");
//    
//    char command[30];
//    snprintf(command, sizeof(command), "AT+CMQTTPAYLOAD=0,%d", size);
//                        
//    sendToGSMAndPrintResponse(HT_USART1, "AT+CMQTTTOPIC=0,25",">");
//    sendToGSMAndPrintResponse(HT_USART1, "usage-raw-data-topic-prod","OK");
//    sendToGSMAndPrintResponse(HT_USART1, command,">");
//    sendToGSMAndPrintResponse(HT_USART1, payload,"OK");
//    sendToGSMAndPrintResponse(HT_USART1, "AT+CMQTTPUB=0,1,60","OK");
//    
//    free(payload);
//    Emulated_EEPROM_Erase_All_Cook_Sessions();
//		
//		return true;
//}

//void sendCookingSessionsChunk(int offset) {
//    int totalSessions = Emulated_EEPROM_Read_Cooking_Session_Num();
//    if (offset >= totalSessions) return;

//    int chunkSize = (totalSessions - offset >= 2) ? 2 : (totalSessions - offset);
//    size_t totalCookingTime = 0;

//    for (int i = 0; i < chunkSize; i++) {
//        CookSession session = Emulated_EEPROM_Read_Cook_Session(offset + i);
//        totalCookingTime += (session.offT - session.onT);
//    }

//    float Pg = globalPayload.stat.Ag * globalPayload.stat.Vg * totalCookingTime;

//    char batvStr[6]; // Enough for "4.20\0" or "\"NA\"\0"
//    if (globalPayload.stat.BatV >= 0.0f && globalPayload.stat.BatV <= 4.2f) {
//        snprintf(batvStr, sizeof(batvStr), "%.2f", globalPayload.stat.BatV);
//    } else {
//        strncpy(batvStr, "\"NA\"", sizeof(batvStr));
//        batvStr[sizeof(batvStr) - 1] = '\0';
//    }

//    char cookPayload[256];  // Adjust size based on real worst-case estimates
//    int payloadOffset = snprintf(cookPayload, sizeof(cookPayload),
//        "{\"devID\":{\"sn\":\"%u\",\"FW\":\"%s\"},\"ts\":%d,\"stat\":{\"Vg\":%u,\"Ag\":%.1f,\"BatV\":%s,\"Pg\":%.2f},\"Cook\":[",
//        Emulated_EEPROM_Read_Serial_ID(),
//        globalPayload.devID.FW,
//        getEpochTimeRTC(),
//        globalPayload.stat.Vg,
//        globalPayload.stat.Ag,
//        batvStr,
//        Pg / 3600.0f
//    );

//    for (int i = 0; i < chunkSize; i++) {
//        CookSession session = Emulated_EEPROM_Read_Cook_Session(offset + i);
//        payloadOffset += snprintf(cookPayload + payloadOffset, sizeof(cookPayload) - payloadOffset,
//            "{\"pan\":\"%c\",\"onT\":%ld,\"offT\":%ld,\"pwr\":%u,\"pwrC\":%.2f}%s",
//            session.pan,
//            session.onT,
//            session.offT,
//            POWER_LEVEL[eCooker.numberOfPlates - 1][session.pwr],
//            ((float)(session.offT - session.onT) * POWER_LEVEL[eCooker.numberOfPlates - 1][session.pwr]) / 3600.0f,
//            (i < chunkSize - 1) ? "," : ""
//        );
//    }

//    snprintf(cookPayload + payloadOffset, sizeof(cookPayload) - payloadOffset, "]}\r\n");

//    char command[40];
//    snprintf(command, sizeof(command), "AT+CMQTTPAYLOAD=0,%d", strlen(cookPayload));

//    sendToGSMAndPrintResponse(HT_USART1, "AT+CMQTTTOPIC=0,25", ">");
//    sendToGSMAndPrintResponse(HT_USART1, "usage-raw-data-topic-prod", "OK");
//    sendToGSMAndPrintResponse(HT_USART1, command, ">");
//    sendToGSMAndPrintResponse(HT_USART1, cookPayload, "OK");
//    sendToGSMAndPrintResponse(HT_USART1, "AT+CMQTTPUB=0,1,60", "OK");
//}


//*******************************************************************************************************************************

// Accurate energy using averaged Ag/Vg over a duration (Wh).
//static inline uint64_t session_seconds(long onT, long offT) {
//    return (offT > onT) ? (uint64_t)(offT - onT) : 0ULL;
//}
//static inline double energy_wh_from_avg(double ag_avg, double vg_avg, uint64_t secs) {
//    return (ag_avg * vg_avg) * ((double)secs / 3600.0);
//}

// Accurate energy using measured average power (Wh).
static inline uint64_t secs_between(long onT, long offT) {
  return (offT > onT) ? (uint64_t)(offT - onT) : 0ULL;
}

static inline uint32_t clamped_session_secs(long onT, long offT) {
  uint64_t span = secs_between(onT, offT);
  return (span > MAX_SESSION_DURATION_SEC) ? MAX_SESSION_DURATION_SEC : (uint32_t)span;
}
//static inline double energy_wh_from_avgP(double avgP_watts, uint64_t secs) {
//  return avgP_watts * ((double)secs / 3600.0);
//}

// Union of two intervals (for 2-session chunks).
//static inline uint64_t union_secs2(long on1, long off1, long on2, long off2) {
//  uint64_t s1 = secs_between(on1, off1);
//  uint64_t s2 = secs_between(on2, off2);
//  long start_max = (on1 > on2) ? on1 : on2;
//  long end_min   = (off1 < off2) ? off1 : off2;
//  uint64_t overlap = (end_min > start_max) ? (uint64_t)(end_min - start_max) : 0ULL;
//  return s1 + s2 - overlap;
//}

// Union of N (chronological) sessions across EEPROM.
static uint64_t union_seconds_all_sessions(void) {
  int n = Emulated_EEPROM_Read_Cooking_Session_Num();
  if (n <= 0) return 0ULL;

  CookSession first = Emulated_EEPROM_Read_Cook_Session(0);
  long cur_on  = first.onT;
  long cur_off = first.offT;
  uint64_t total = 0ULL;

  for (int i = 1; i < n; i++) {
    CookSession s = Emulated_EEPROM_Read_Cook_Session(i);
    if (s.onT <= cur_off) {
      // Overlaps / touches current union range: extend it
      if (s.offT > cur_off) cur_off = s.offT;
    } else {
      // Disjoint: close current range and start a new one
      total += secs_between(cur_on, cur_off);
      cur_on  = s.onT;
      cur_off = s.offT;
    }
  }
  total += secs_between(cur_on, cur_off);
  return total;
}

bool sendRetryDetails(void) {
    UART0_reset_tx_buffer(true);
    uint32_t serial = Emulated_EEPROM_Read_Serial_ID();

    char payload[160];
    int written = snprintf(payload, sizeof(payload),
        "{\"devID\":{\"sn\":\"%u\"},\"ts\":%lu,\"retry\":%u}",
        (unsigned int)serial,
        (unsigned long)getEpochTimeRTC(),
        (unsigned int)connRetryCount);
    if (written <= 0 || written >= (int)sizeof(payload)) {
        return false;
    }

    char command[40];
    snprintf(command, sizeof(command), "AT+CMQTTPAYLOAD=0,%d", written);

    sendToGSM("AT+CMQTTTOPIC=0,20");
    sendToGSM("device-retry-topic");
    sendToGSM(command);
    sendToGSM(payload);
    sendToGSM("AT+CMQTTPUB=0,1,60");

    return true;
}

bool sendCookingSessions(void){
    // Use the chunked sender (heap-free) to avoid allocator hardfaults.
    if (Emulated_EEPROM_Read_Cooking_Session_Num() == 0) {
        return false;
    }
    payloadOffset = 0;
    payloadDelay();
    return true;
}


bool sendCookingSessionsAggregated(void){
    UART0_reset_tx_buffer(true);
    UART0_ClearRxBuffer();
    (void)union_seconds_all_sessions(); // retained call; result not used but kept for parity

    int count = Emulated_EEPROM_Read_Cooking_Session_Num();
    uint64_t sum_secs = 0;
    double sum_pwr_w = 0.0;
    double sum_pwrC_Wh = 0.0;
    double PgWh_total = 0.0;

    int included_count = 0;

    for (int i = 0; i < count; i++) {
        CookSession s = Emulated_EEPROM_Read_Cook_Session(i);

        unsigned int pwr_w = POWER_LEVEL[eCooker.numberOfPlates - 1][s.pwr];
        uint32_t session_secs = clamped_session_secs(s.onT, s.offT);
        double session_pwrC_Wh = ((double)session_secs * (double)pwr_w) / 3600.0;

        if (session_pwrC_Wh > (eCooker.numberOfPlates == SINGLE_STOVE ? 3200 : 3300)) {
            continue;
        }

        sum_secs    += (uint64_t)session_secs;
        sum_pwr_w   += (double)pwr_w;
        sum_pwrC_Wh += session_pwrC_Wh;
        PgWh_total  += ((double)s.Ag * (double)s.Vg * (double)session_secs) / 3600.0;

        included_count++;
    }

    double avg_pwr_w = (included_count > 0) ? (sum_pwr_w / (double)included_count) : 0.0;

    char payload[256];
    int written = snprintf(payload, sizeof(payload),
        "{\"devID\":{\"sn\":\"%u\",\"FW\":\"%s\"},\"ts\":%lu,"
        "\"CookAgg\":{\"count\":%d,\"avgPwrW\":%.2f,\"totalSecs\":%llu,\"pwrC\":%.2f,\"PgWh\":%.2f}}",
        Emulated_EEPROM_Read_Serial_ID(),
        globalPayload.devID.FW ? globalPayload.devID.FW : "",
        (unsigned long)getEpochTimeRTC(),
        included_count,
        avg_pwr_w,
        (unsigned long long)sum_secs,
        sum_pwrC_Wh,
        PgWh_total);
    if (written <= 0 || written >= (int)sizeof(payload)) {
        return false;
    }

    MQTT_CheckPublishCook();
    if (!publishCookMQTT(payload, written)) {
        g_check_cook_publish = false;
        g_cook_chunk_size = 0;
        return false;
    }

    return true;
}


void sendCookingSessionsChunk(int offset) {
    // Build payload without heap allocations to avoid allocator hardfaults.
    UART0_reset_tx_buffer(true);
    int totalSessions = Emulated_EEPROM_Read_Cooking_Session_Num();
		if (offset >= totalSessions) return;

    int chunkSize = (totalSessions - offset >= g_params.num_cooking_blocks)
                        ? g_params.num_cooking_blocks
                        : (totalSessions - offset);

    char buf[1024];
    int written = snprintf(buf, sizeof(buf),
        "{\"devID\":{\"sn\":\"%u\",\"FW\":\"%s\"},\"ts\":%lu,\"Cook\":[",
        Emulated_EEPROM_Read_Serial_ID(),
        globalPayload.devID.FW ? globalPayload.devID.FW : "",
        (unsigned long)getEpochTimeRTC());
    if (written <= 0 || written >= (int)sizeof(buf)) return;

    for (int i = 0; i < chunkSize; i++) {
        CookSession s = Emulated_EEPROM_Read_Cook_Session(offset + i);

        uint32_t session_secs = clamped_session_secs(s.onT, s.offT);

        double Pg = ((double)s.Ag * (double)s.Vg * (double)session_secs) / 3600.0;

        float pwr_wh = ((float)session_secs *
                        POWER_LEVEL[eCooker.numberOfPlates - 1][s.pwr]) / 3600.0f;
        float max_pwr_wh = (eCooker.numberOfPlates == SINGLE_STOVE) ? 3200.0f : 3300.0f;
        if (pwr_wh > max_pwr_wh) {
            pwr_wh = max_pwr_wh;
        }
        
        int remain = (int)sizeof(buf) - written;
        if (remain <= 0) return;

        int n = snprintf(buf + written, remain,
            "{\"pan\":\"%c\",\"onT\":%ld,\"offT\":%ld,\"pwr\":%u,\"pwrC\":%.2f,\"Pg\":%.2f}%s",
            s.pan,
            s.onT,
            s.offT,
            POWER_LEVEL[eCooker.numberOfPlates - 1][s.pwr],
            pwr_wh,
            Pg,
            (i < chunkSize - 1) ? "," : "");
        if (n <= 0 || n >= remain) return;
        written += n;
    }

    if (written + 2 >= (int)sizeof(buf)) return;
    buf[written++] = ']';
    buf[written++] = '}';
    buf[written] = '\0';

    g_cook_chunk_size = (uint16_t)chunkSize;
    MQTT_CheckPublishCook();
    if (!publishCookMQTT(buf, written)) {
        g_check_cook_publish = false;
        g_cook_chunk_size = 0;
    }
}




//*******************************************************************************************************************************

bool sendMultipleErrorPayload(void) {
	  UART0_reset_tx_buffer(true);
	  __disable_irq();
    if (globalPayload.errC == NULL || nerr == 0) {
        __enable_irq();
        return false;
    }
		uint32_t serial = Emulated_EEPROM_Read_Serial_ID();
    // Take exclusive ownership: detach the array so addErrC() cannot realloc it
    size_t local_nerr = nerr;
    errC *err_ptr = globalPayload.errC;
    globalPayload.errC = NULL;
    nerr = 0;
    __enable_irq();
		
    bool result = false;
    char payload[768];
    int written = snprintf(payload, sizeof(payload),
        "{\"devID\":{\"sn\":\"%u\"},\"errC\":[",
        (unsigned int)serial);
    if (written <= 0 || written >= (int)sizeof(payload)) {
        goto cleanup;
    }

    for (size_t i = 0; i < local_nerr; i++) {
        int remain = (int)sizeof(payload) - written;
        if (remain <= 0) goto cleanup;
        int n = snprintf(payload + written, remain,
            "{\"ts\":\"%ld\",\"err\":\"%d\"}%s",
            err_ptr[i].ts,
            err_ptr[i].err,
            (i < local_nerr - 1) ? "," : "");
        if (n <= 0 || n >= remain) {
            goto cleanup;
        }
        written += n;
    }

    if (written + 2 >= (int)sizeof(payload)) {
        goto cleanup;
    }
    payload[written++] = ']';
    payload[written++] = '}';
    payload[written] = '\0';

    sendToGSMAndPrintResponse(HT_USART1,"AT", "OK");
    if (!publishMQTTWithRetry(payload, written)) {
        goto cleanup; // Abort if all retries fail
    }

		Deepsleep_Start_Flag = sendToGSMAndPrintResponse(HT_USART1, "AT+CSCLK=2", "OK");
		result = true;

cleanup:
		free(err_ptr);
		return result;
}

bool connRun = false;
bool startUpPending = true;

void sendPayload(void){
	if (currentState == STATE_FACTORY) {
		return;
	}
	// Guard: Cap cooking time monitor values at 7200 seconds before sending payload
	if (cookingTimeMonitor.left_seconds > 7200U) {
		cookingTimeMonitor.left_seconds = 7200U;
	}
	if (cookingTimeMonitor.right_seconds > 7200U) {
		cookingTimeMonitor.right_seconds = 7200U;
	}
	
	if (cook_upload_stopped && !cookPublishBusy() &&
			(device_time - cook_upload_stop_devt) >= COOK_UPLOAD_BACKOFF_S) {
		cook_upload_stopped = false;
		payloadSent = false;
	}

	switch(g_params.conn_strategy){
		case CONN_ALWAYS_ON:
		case CONN_ON_DEMAND:
				if (g_params.send_cooking_sessions &&
					 (g_params.payload_mode == PAYLOAD_ALL	? Emulated_EEPROM_Read_Cooking_Session_Num() >= g_params.num_cooking_blocks : (currentState == STATE_ONOFF && Emulated_EEPROM_Read_Cooking_Session_Num()>0 )) &&
						!payloadSent && currentConnState == CONNECTED) {
						payloadSent = true;
						switch(g_params.payload_mode)
						{
							case PAYLOAD_ALL:
								payloadDelay();
								break;
							case PAYLOAD_ESSENTIAL:
								if (!sendCookingSessionsAggregated()) {
									cookPublishFailed();
								}
								break;
						}
						
						
				}

				if (!cookPublishBusy() &&
						g_params.send_error_payloads &&
						nerr >= 3 && currentConnState == CONNECTED) {
						sendMultipleErrorPayload();
				}

				/* First meta / every metaIntervalS — piggyback only; never open MQTT for meta alone.
				 * If due while stove off / disconnected, wait until next natural session. */
				if (metaPayloadDue() && currentConnState == CONNECTED) {
						if (g_check_cook_publish ||
								(!payloadTriggered && payload_bftm0 > 0)) {
								/* Cooking MQTT in flight — wait so AT/MQTT is not shared. */
						} else if ((device_time - last_meta_attempt_devt) >= 5UL ||
											 last_meta_attempt_devt == 0UL) {
								last_meta_attempt_devt = device_time;
								if (send24Hrs()) {
										metaMarkSent();
								}
								/* On failure leave send_last_rtc_meta unchanged and retry next session. */
						}
				}
		break;
		case CONN_ONCE:
				switch (payloadState) {
						case PAYLOAD_IDLE:
								/* Do not connect solely for meta — wait until already CONNECTED. */
								if (metaPayloadDue() && currentConnState == CONNECTED) {
											payloadState = PAYLOAD_SEND_24HRS;
								}
								break;

						case PAYLOAD_SEND_24HRS:
								if (g_check_cook_publish ||
										(!payloadTriggered && payload_bftm0 > 0)) {
										break;
								}
								if ((device_time - last_meta_attempt_devt) < 5UL &&
										last_meta_attempt_devt != 0UL) {
										break;
								}
								last_meta_attempt_devt = device_time;
								if (send24Hrs()) {
										metaMarkSent();
										startUpPending = false;
										payloadState = PAYLOAD_SEND_ERRORS;
								} else if (currentConnState != CONNECTED) {
										/* Lost link — reconnect and retry; do not advance interval. */
										payloadState = PAYLOAD_IDLE;
										connRun = false;
								}
								/* Epoch/publish not ready: stay and retry (throttled). */
								break;

						case PAYLOAD_SEND_ERRORS:
								if (cookPublishBusy()) {
									break;
								}
								if(nerr>0)
									sendMultipleErrorPayload();
								payloadState = PAYLOAD_SEND_SESSIONS;
								payloadSent = false;
								break;

						case PAYLOAD_SEND_SESSIONS:
								if(Emulated_EEPROM_Read_Cooking_Session_Num()>0 && !cook_upload_stopped){
									
									!payloadSent ? payloadDelay() : (void)0;
								}
								else
									payloadState = PAYLOAD_DONE;
								connRun = false;
								break;

						case PAYLOAD_DONE:
								if (g_check_cook_publish) {
									break;
								}
								if ((g_params.conn_strategy == CONN_ALWAYS_ON) && (!keep_alive_changed || !persistent_flag_changed)) {
									/* stay connected */
								} else {
									mqttFullTeardown();
								}
								payloadState = PAYLOAD_IDLE;
								break;
				}
				break;

	}
}
uint32_t nconn = 0;
static uint32_t mqtt_connect_wait_devt = 0;

ConnState currentConnState = DISCONNECTED;

void enterConnState(ConnState state) {
    currentConnState = state;
    switch (state) {
        case NETWORK_INIT:
            networkInitRestart();
            break;
        case NETWORK_READY:
            break;
        case CONNECTED:
					connRetryCount = 0;
					mqtt_connect_wait_devt = 0;
					if (!mqttConnectAllowed()) {
						mqtt_service_started = false;
						isSubscribedTopic = false;
						currentConnState = (currentState == STATE_FACTORY) ? DISCONNECTED : NETWORK_READY;
						break;
					}
					handleSyncLED(LED_STEADY, 'g');
					subscribeTopic();
            break;
        case DISCONNECTED:
						isSubscribedTopic = false;
						mqtt_connect_wait_devt = 0;
            break;
				case CONNECTING:
						mqtt_connect_wait_devt = device_time;
            break;
        case DISCONNECTING:
            break;
    }
}

void handleConnect(ConnEvent event) {
    if (currentState == STATE_FACTORY) {
        return;
    }
    switch (currentConnState) {
        case DISCONNECTED:
            /* Cellular NETWORK_INIT is independent of stove state. */
            if (event == CONNECT && isGSMpoweredUp()) {
                enterConnState(NETWORK_INIT);
            }
            break;

        case NETWORK_INIT:
            if (event == DISCONNECT) {
                networkInitRestart();
                enterConnState(DISCONNECTED);
            }
            break;

        case NETWORK_READY:
            if (event == CONNECT && mqttConnectAllowed()) {
                enterConnState(CONNECTING);
                connect();
            } else if (event == DISCONNECT) {
                enterConnState(DISCONNECTED);
            }
            break;

        case CONNECTING:
            if (event == CONNECT) {
            } else if (event == DISCONNECT) {
                enterConnState(DISCONNECTED);
            }
            break;

        case CONNECTED:
            if (event == DISCONNECT) {
                enterConnState(DISCONNECTING);
                gracefulDisconnect();
            }
            break;

        case DISCONNECTING:
            if (event == DISCONNECT) {
            } else if (event == CONNECT) {
            }
            break;
    }
}

void connect(void){
    /* Hard gate: never start MQTT while stove is off or in factory. */
    if (currentState == STATE_ONOFF) {
        enterConnState(NETWORK_READY);
        return;
    }
    if (currentState == STATE_FACTORY) {
        enterConnState(DISCONNECTED);
        return;
    }

    isSubscribedTopic = false;
    nconn++;
    char accqCmd[64];
    sprintf(accqCmd, "AT+CMQTTACCQ=0,\"%d\",1", Emulated_EEPROM_Read_Serial_ID());

    int ka = g_params.keep_alive_s;
    if (ka < 10)      ka = 10;
    if (ka > 1200)   ka = 1200;
	
		int ps = g_params.persistent_flag;
    if (ps <= 0)   ps = 0;
    if (ps >= 1)   ps = 1;

    char connectCmd[128];
    snprintf(connectCmd, sizeof(connectCmd),
             "AT+CMQTTCONNECT=0,\"tcp://a3b6socfqdtpq4-ats.iot.ap-southeast-1.amazonaws.com:8883\",%d,%d",
             ka, ps);
		keep_alive_changed = false;
		persistent_flag_changed = false;

	/* Drop stale URCs so checkMQTTConnection cannot re-enter NETWORK_INIT mid-connect. */
		UART0_ClearRxBuffer();

	/* SMS ? UART: direct +CMT URC (text mode). Idempotent each connect(). */
		sendToGSM("AT+CMGF=1");
		sendToGSM("AT+CNMI=2,2,0,0,0");

    if (!mqtt_service_started) {
        sendToGSM("AT+CMQTTDISC=0");
        sendToGSM("AT+CMQTTREL=0");
        sendToGSM("AT+CMQTTSTOP");
				Systick_Delay_us(100000);  /* 100ms for cleanup */
	
				gsmApplyA7682eUpstreamTcpMssClamp();
			
        sendToGSM("AT+CSSLCFG=\"sslversion\",0,4");
        sendToGSM("AT+CSSLCFG=\"authmode\",0,2");
        sendToGSM("AT+CSSLCFG=\"cacert\",0,\"cacert.pem\"");
        sendToGSM("AT+CSSLCFG=\"clientcert\",0,\"clientcert.pem\"");
        sendToGSM("AT+CSSLCFG=\"clientkey\",0,\"clientkey.pem\"");
			
				sendToGSM("AT+CSSLCFG=\"ignorertctime\",0,1");
				sendToGSM("AT+CSSLCFG=\"enableSNI\",0,1");
				Systick_Delay_us(50000);  /* 50ms for TLS config */

        sendToGSM("AT+CMQTTSTART");
				Systick_Delay_us(100000);  /* 100ms for MQTT init */

        sendToGSM(accqCmd);
        sendToGSM("AT+CMQTTCFG=\"checkUTF8\",0,0");
        sendToGSM("AT+CMQTTCFG=\"version\",0,4");
        sendToGSM("AT+CMQTTSSLCFG=0,0");
				Systick_Delay_us(50000);  /* 50ms before connect */
        mqtt_service_started = true;
    }

	/* Drop setup OK spam so +CMQTTCONNECT URC is not lost when the RX ring fills. */
		UART0_ClearRxBuffer();
    sendToGSM(connectCmd);

    // connDelay(); // if you still use this
}


void addCookSession(char pan, long onT, long offT, uint16_t pwr) {
		//functionStatus = 71;
    Cook *tmp = (Cook *)realloc(globalPayload.Cook, (ncook + 1) * sizeof(Cook));
    if (tmp == NULL) {
        return;
    }
    globalPayload.Cook = tmp;

    globalPayload.Cook[ncook].pan = pan;
    globalPayload.Cook[ncook].onT = onT;
    globalPayload.Cook[ncook].offT = offT;
    globalPayload.Cook[ncook].pwr = pwr;
		
    globalPayload.Cook[ncook].PwrC = ((float)(offT - onT) * pwr) / 3600;
	//	printf("Added cooking session: pan=%c, onT=%ld, offT=%ld, pwr=%d, PwrC=%f\nnCook: %d\n",
    //       pan, onT, offT, pwr, globalPayload.Cook[ncook].PwrC,ncook);
    ncook++;
}

void freeCookSessions() {
    if (globalPayload.Cook != NULL) {
        free(globalPayload.Cook); 
        globalPayload.Cook = NULL; 
        ncook = 0;            
//        my_fprintf(HT_USART1,"All cooking sessions freed.\n");
    } else {
//        my_fprintf(HT_USART1,"No cooking sessions to free.\n");
    }
}

void freeErr(){
		if (globalPayload.errC != NULL) {
        free(globalPayload.errC);  
        globalPayload.errC = NULL; 
        nerr = 0;            
//        my_fprintf(HT_USART1,"All errors freed.\n");
    } else {
//        my_fprintf(HT_USART1,"No errors to free.\n");
    }
	
}

#define MAX_ERRC 8

void addErrC(long ts, uint8_t err){
	    if (nerr >= MAX_ERRC) {
	        return;
	    }
	    errC *tmp = (errC *)realloc(globalPayload.errC, (nerr + 1) * sizeof(errC));
    if (tmp == NULL) {
        return;
    }
    globalPayload.errC = tmp;
    globalPayload.errC[nerr].ts = ts;
    globalPayload.errC[nerr].err = err;		
//		my_fprintf(HT_USART1,"Added error: ts= %l, err= %d\n",ts, err);
    nerr++;
}

#define MAX_FAILED_OFFSETS 42
#define MAX_RETRY_PUBLISH_RESP 5
#define COOK_PUBLISH_ACK_TIMEOUT_S 60U
bool g_check_cook_publish = false;
uint32_t g_cook_publish_check_start_time = 0;
uint16_t failedOffsets[MAX_FAILED_OFFSETS];
uint16_t failpub = 0;
uint16_t failed_send_count = 0;

void MQTT_CheckPublishCook(void) {
    if (!g_check_cook_publish) {
        g_cook_publish_check_start_time = device_time;
        cook_pub_seq = gsm_mqtt_pub_seq;
    }
    g_check_cook_publish = true;
    /* The 6s mqttcheck timer must not restart this wait or wipe the start time. */
    mqttcheckTriggered = true;
    mqttcheck_bftm0 = 0;
}

/* 1 = PUBACK, 2 = publish error, 0 = no +CMQTTPUB since this cook AT+CMQTTPUB yet. */
static int cookPubResult(void) {
    if (gsm_mqtt_pub_seq == cook_pub_seq) {
        return 0;
    }
    return gsm_mqtt_pub_ok ? 1 : 2;
}

static void cookPublishStop(void) {
    g_check_cook_publish = false;
    g_cook_chunk_size = 0;
    failed_send_count = 0;
    payloadTriggered = true;
    payload_bftm0 = 0;
    payloadSent = true;
    cook_upload_stopped = true;
    cook_upload_stop_devt = device_time;
    cookUploadIdleOff((g_params.conn_strategy == CONN_ON_DEMAND || g_params.conn_strategy == CONN_ONCE) ? TRUE : FALSE);
}

static void cookPublishFailed(void) {
    g_check_cook_publish = false;
    failed_send_count++;
    if (failed_send_count >= MAX_RETRY_PUBLISH_RESP) {
        cookPublishStop();
        return;
    }
    if (g_params.payload_mode == PAYLOAD_ALL) {
        payloadDelay();
    } else if (g_params.payload_mode == PAYLOAD_ESSENTIAL) {
        payloadSent = false;
    }
}

static void cookPublishAcked(void) {
    failed_send_count = 0;
    if (g_params.payload_mode == PAYLOAD_ALL) {
        uint16_t n = g_cook_chunk_size;
        int32_t left;
        if (n == 0 ||
            Emulated_EEPROM_Delete_Cook_Sessions_Range((uint32_t)payloadOffset, n) != SUCCESS) {
            cookPublishStop();
            return;
        }
        g_cook_chunk_size = 0;
        g_check_cook_publish = false;
        left = Emulated_EEPROM_Read_Cooking_Session_Num();
        if (left > payloadOffset) {
            payloadDelay();
        } else {
            payloadOffset = 0;
            payloadSent = false;
            payloadTriggered = true;
            payload_bftm0 = 0;
            payloadState = PAYLOAD_IDLE;
            if (left <= 0) {
                cookUploadIdleOff(true);
            }
        }
    } else if (g_params.payload_mode == PAYLOAD_ESSENTIAL) {
        Emulated_EEPROM_Erase_All_Cook_Sessions();
        g_check_cook_publish = false;
        g_cook_chunk_size = 0;
        payloadSent = false;
        cookUploadIdleOff((g_params.conn_strategy == CONN_ON_DEMAND || g_params.conn_strategy == CONN_ONCE) ? TRUE : FALSE);
    } else {
        g_check_cook_publish = false;
    }
}

void checkMQTTConnection(void) {
    if (currentState == STATE_FACTORY) {
        return;
    }
    if (g_check_cook_publish) {
        int code = cookPubResult();
        if (code == 1) {
            cookPublishAcked();
            return;
        }
        if (code == 2 ||
            (device_time - g_cook_publish_check_start_time) > COOK_PUBLISH_ACK_TIMEOUT_S) {
            cookPublishFailed();
            return;
        }
    }
    /* Snapshot RX ring buffer into a NUL-terminated buffer; strstr() requires it. */
    if (rxd_comm0.cnt == 0) {
        /* URC may have been lost; confirm over AT after connect has had time. */
        if (!g_check_cook_publish &&
            currentConnState == CONNECTING &&
            mqtt_connect_wait_devt != 0 &&
            (device_time - mqtt_connect_wait_devt) >= 5U) {
            mqtt_connect_wait_devt = device_time;
            GSMResponse q = GSMsendATCommand("AT+CMQTTCONNECT?");
            if (q.success && q.response != NULL &&
                strstr(q.response, "tcp://") != NULL) {
                enterConnState(CONNECTED);
            }
        }
        return;
    }
    static char rx_snap[UART0_BUF_SIZE + 1];
    uint16_t n = rxd_comm0.cnt;
    if (n > UART0_BUF_SIZE) n = UART0_BUF_SIZE;
    for (uint16_t i = 0; i < n; i++) {
        uint16_t idx = (rxd_comm0.read_pt + i) % UART0_BUF_SIZE;
        rx_snap[i] = (char)rxd_comm0.buffer[idx];
    }
    rx_snap[n] = '\0';
    const char *rxStr = rx_snap;

    if (strstr(rxStr, "+CMQTTCONNECT: 0,0") != NULL) {
        enterConnState(CONNECTED);
        UART0_ClearRxBuffer();
        return;
    }

    /*
     * Fail URC is +CMQTTCONNECT: 0,<err> with err != 0.
     * Do NOT treat AT+CMQTTCONNECT? query lines (+CMQTTCONNECT: 0,"tcp://...")
     * as failures — the old "+CMQTTCONNECT: 0," match falsely re-entered NETWORK_INIT
     * while MQTT connect was still in progress.
     */
    if (currentConnState == CONNECTING) {
        const char *p = rxStr;
        while ((p = strstr(p, "+CMQTTCONNECT: 0,")) != NULL) {
            const char *code = p + 17; /* strlen("+CMQTTCONNECT: 0,") */
            if (code[0] >= '1' && code[0] <= '9') {
                mqtt_service_started = false;
                enterConnState(NETWORK_INIT);
                UART0_ClearRxBuffer();
                return;
            }
            p = code;
        }
        /* Make room for a late success URC if setup responses filled the ring. */
        if (rxd_comm0.cnt > (UART0_BUF_SIZE * 3U / 4U)) {
            UART0_ClearRxBuffer();
        }
    }
}

int totalBlocks = 0;

void storeFailedEraseRest(void){
	totalBlocks = 0;
	if (failpub == 0) {
		return;
	}
	CookSession *sessions = (CookSession *)malloc(failpub * 2 * sizeof(CookSession));
	if (sessions == NULL) {
		failpub = 0;
		return;
	}
	int totalSessions = Emulated_EEPROM_Read_Cooking_Session_Num();
	
	
	for(int i=0; i<failpub; i++){
		if (failedOffsets[i] >= totalSessions) {
			free(sessions);
			failpub = 0;
			return;
		}
		int chunkSize = (totalSessions - failedOffsets[i] >= 2) ? 2 : (totalSessions - failedOffsets[i]);

		for (int j = 0; j < chunkSize; j++) {
			if (totalBlocks >= failpub * 2) break;
			sessions[totalBlocks] = Emulated_EEPROM_Read_Cook_Session(failedOffsets[i] + j);
			totalBlocks++;
		}
	}

	if (totalBlocks > (int)COOK_SESSION_MAX_COUNT) {
		free(sessions);
		failpub = 0;
		return; /* do not erase and restore a subset; that would discard sessions */
	}

	Emulated_EEPROM_Erase_All_Cook_Sessions();

	for(int k =0; k<totalBlocks; k++){
		Emulated_EEPROM_Write_Cook_Session(k, sessions[k].pan, sessions[k].onT, sessions[k].offT, sessions[k].pwr, sessions[k].Ag, sessions[k].Vg);
	}

	Emulated_EEPROM_Write_Cooking_Session_Num(totalBlocks);
	failpub = 0;
	
	free(sessions);
}

void gracefulDisconnect(void){
		sendToGSM("AT+CMQTTDISC=0");
		isSubscribedTopic = false;
		enterConnState(DISCONNECTED);
}

void mqttFullTeardown(void){
		sendToGSM("AT+CMQTTDISC=0");
		sendToGSM("AT+CMQTTREL=0");
		sendToGSM("AT+CMQTTSTOP");
		Systick_Delay_us(100000);
		UART0_ClearRxBuffer();
		mqtt_service_started = false;
		isSubscribedTopic = false;
		/* Keep cellular registration (NETWORK_READY) — do not re-run NETWORK_INIT on turn-off. */
		if (currentState == STATE_FACTORY) {
			enterConnState(DISCONNECTED);
		} else {
			enterConnState(NETWORK_READY);
		}
}

void subscribeSerial(void){
    char formattedCommand[10];           
    sprintf(formattedCommand, "%d", Emulated_EEPROM_Read_Serial_ID());

    sendToGSM("AT+CMQTTSUBTOPIC=0,9,1");
    sendToGSM(formattedCommand);

    sendToGSM("AT+CMQTTSUB=0");
}

void subscribeTopic(void) {
    GSMResponse res = GSMsendATCommand("AT+CMQTTCONNECT?");
    if (!res.response) {
        isSubscribedTopic = false;
        return;
    }

    if (!isSubscribedTopic && res.success) {
        uint32_t serial = Emulated_EEPROM_Read_Serial_ID();
        char serial_str[11];
        int serial_len = snprintf(serial_str, sizeof(serial_str), "%u", serial);
				static const char at_subtopic_fmt[] = "AT+CMQTTSUBTOPIC=0,%d,1\r";
        static const char at_sub_fmt[]      = "AT+CMQTTSUB=0,%d,1\r";
				char cmd_buf[36];

        snprintf(cmd_buf, sizeof(cmd_buf), at_subtopic_fmt, serial_len);
        sendToGSM(cmd_buf);
        sendToGSMAndPrintResponse(HT_UART0, serial_str, "OK");

        snprintf(cmd_buf, sizeof(cmd_buf), at_sub_fmt, serial_len);
        sendToGSM(cmd_buf);
        sendToGSMAndPrintResponse(HT_UART0, serial_str, "OK");

        isSubscribedTopic = true;
    }
}

// Function to get the length of the Serial ID
int get_serial_id_length(void) {
	char serial_id_str[10];
	snprintf(serial_id_str, sizeof(serial_id_str), "%d", Emulated_EEPROM_Read_Serial_ID());
	return strlen(serial_id_str);
}
    

bool gsmFotaDownload(const char *atCmd) {
    if (atCmd == NULL || atCmd[0] == '\0') {
        return false;
    }

    UART0_ClearRxBuffer();
    sendToGSM(atCmd);

    // Poll for OK/ERROR - the module then manages the download and
    // two automatic reboots entirely on its own. No IAP step needed.
    const char *rxBuf;
    int waitTime = 0;
    const int maxWaitTime  = 15000;
    const int pollInterval = 500;

    while (waitTime < maxWaitTime) {
        BFTM0_Delay_ms(pollInterval);
        waitTime += pollInterval;
        rxBuf = (const char *)rxd_comm0.buffer;
        if (strstr(rxBuf, "OK") || strstr(rxBuf, "ERROR")) {
            break;
        }
    }
    return true;
}

void fwDownload(char data[]) {
    if (data == NULL || data[0] == '\0') {  // Prevent false OTA if no data received
        handleSyncLED(LED_OFF, 'b');
        fileTriggered = false;
        updateTriggered = false;
        result.success = false;  
        fwUpdate.success = false;
        update_bftm0 = 0;
        file_bftm0 = 0;
        return;
    }

    // Step 1: Delete existing firmware file if it exists
    const char *hex_name = "eCook3_OTA_TEST1.hex";
    const char *header   = "+FSLS: FILES:\r\n";
    const char *rxBuf;

    UART0_ClearRxBuffer();
    sendToGSM("AT+FSLS");
    BFTM0_Delay_ms(1000);  // Wait for FSLS response
    rxBuf = (const char *)rxd_comm0.buffer;

    const char *p = strstr(rxBuf, header);
    if (p) {
        p += strlen(header);
        if (strstr(p, hex_name)) {
            UART0_ClearRxBuffer();
            sendToGSM("AT+FSDEL=eCook3_OTA_TEST1.hex");
            BFTM0_Delay_ms(500);  // Wait for delete confirmation
        }
    }

    // Step 2: Retry HTTP download up to 5 times
    char formattedCommand[128];
    int retryCount = 0;
    bool success = false;
    int httpStatus = 0;
		
    while (retryCount < 5 && !success) {
        UART0_ClearRxBuffer();
        sendToGSM("AT+CSSLCFG=\"sslversion\",1,3");
        BFTM0_Delay_ms(300);
        UART0_ClearRxBuffer();
        sendToGSM("AT+CSSLCFG=\"ignorertctime\",1,1");
        BFTM0_Delay_ms(300);
        UART0_ClearRxBuffer();
        sendToGSM("AT+CSSLCFG=\"enableSNI\",1,1");
        BFTM0_Delay_ms(300);

        UART0_ClearRxBuffer();
        sendToGSM("AT+HTTPINIT");
        BFTM0_Delay_ms(500);

        sprintf(formattedCommand, "AT+HTTPPARA=\"URL\",\"%s\"", fw_url);
        UART0_ClearRxBuffer();
        sendToGSM(formattedCommand);
        BFTM0_Delay_ms(500);

        UART0_ClearRxBuffer();
        sendToGSM("AT+HTTPPARA=\"SSLCFG\",1");
        BFTM0_Delay_ms(500);

         if (fwUpdate.xapi == 1) {
           UART0_ClearRxBuffer();
            sendToGSM("AT+HTTPPARA=\"CONTENT\",\"text/plain\"");
            BFTM0_Delay_ms(500);  // Wait for HTTPPARA response
						UART0_ClearRxBuffer();
            sendToGSM("AT+HTTPPARA=\"USERDATA\",\"X-API-Key: skdjf384urj934ur39hr384384uhr384y39euhwrwrf9w84ryh3843843hr3yqodfkj39387rg4r\"");
            BFTM0_Delay_ms(500);  // Wait for HTTPPARA response
        }

        UART0_ClearRxBuffer();
        sendToGSM("AT+HTTPACTION=0");

        // Wait for HTTP response (can take several seconds to download)
        const char *httpResp = NULL;
        int waitTime = 0;
        const int maxWaitTime = 30000;  // 30 seconds timeout
        const int pollInterval = 500;    // Check every 500ms
        
        while (waitTime < maxWaitTime && httpResp == NULL) {
            BFTM0_Delay_ms(pollInterval);
            waitTime += pollInterval;
            rxBuf = (const char *)rxd_comm0.buffer;
            httpResp = strstr(rxBuf, "+HTTPACTION:");
        }
        if (httpResp) {
            int method, status, length;
            if (sscanf(httpResp, "+HTTPACTION: %d,%d,%d", &method, &status, &length) == 3) {
                httpStatus = status;
                if (status == 200) {
                    success = true;
										fwUpdate.success = true;
                    break;  // Successful download - exit retry loop immediately
                }
            }
        }
        // If download failed, terminate HTTP session before retrying
        if (!success) {
            UART0_ClearRxBuffer();
            sendToGSM("AT+HTTPTERM");
            BFTM0_Delay_ms(500);  // Wait for HTTPTERM response
        }
        retryCount++;
    }
    (void)httpStatus; /* reserved for future use (e.g. error reporting) */

    if (!success) {
        handleSyncLED(LED_OFF, 'b');        
        fileTriggered = false;
        updateTriggered = false;
        result.success = false;  
        fwUpdate.success = false;
        update_bftm0 = 0;
        file_bftm0 = 0;
        extern bool updateAnimationActive;
        updateAnimationActive = false;
        buzz(3);
        writeLineToSevenSeg();
        BFTM0_Delay_ms(2000);
        clearSevenSegDisplay();
        return;
    }
		
    // Display download status for 2 seconds
    // if (success) {
    //     handleSyncLED(LED_STEADY, 'g');  // Green LED for success
    //     writeSevenSegInt(200);  // Display HTTP 200
    //     BFTM0_Delay_ms(2000);  // Wait 2 seconds
    //     handleSyncLED(LED_OFF, 'g');
    //     clearSevenSegDisplay();
    // } else {
    //     handleSyncLED(LED_STEADY, 'r');  // Red LED for failure
    //     writeSevenSegInt(httpStatus);  // Display HTTP status code (0 if no response)
    //     BFTM0_Delay_ms(2000);  // Wait 2 seconds
    //     handleSyncLED(LED_OFF, 'r');
    //     clearSevenSegDisplay();
		
    // Step 3: Save downloaded content to filesystem before terminating HTTP session
    UART0_ClearRxBuffer();
    sendToGSM("AT+HTTPREADFILE=\"eCook3_OTA_TEST1.hex\",1");
    
    // Wait for file write completion (large files can take time)
    // Poll for OK response indicating file write is complete
    int fileWriteTimeout = 0;
    const int maxFileWriteTime = 10000;  // 10 seconds timeout for 184KB file
    const int fileWritePollInterval = 500;
    bool fileWriteComplete = false;
    
    while (fileWriteTimeout < maxFileWriteTime && !fileWriteComplete) {
        BFTM0_Delay_ms(fileWritePollInterval);
        fileWriteTimeout += fileWritePollInterval;
        rxBuf = (const char *)rxd_comm0.buffer;
        if (strstr(rxBuf, "OK") || strstr(rxBuf, "+HTTPREADFILE:")) {
            fileWriteComplete = true;
            break;
        }
    }
    
    // Additional delay to ensure filesystem sync
    BFTM0_Delay_ms(500);
    
		sendToGSM("AT+HTTPTERM");
}
