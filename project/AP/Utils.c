#include "Utils.h"
#include "gsm.h"
#include "BFTM0.h"
#include "cli.h"
#include "Payload.h"
#include "LEDC.h"
#include "FSM.h"
#include "batVoltage.h"
#include "cJSON.h"
#include "eCooker.h"
#include "EEPROM.h"
#include "RTC.h"
#include "GPTM0.h"
#include <stdio.h>
#include <stdint.h>

#define NUM_SAMPLES 10

//#define MAX_PAYLOAD_LEN   64   
//static char _payload_buf[MAX_PAYLOAD_LEN + 1];

vu32 gBATVoltageLevel;
volatile bool gBATV_SingleEOC;

static char gsm_response_buffer[GSM_BUFFER_SIZE];
static uint8_t cgsn_retry_count = 0;
static uint32_t gsm_at_uart_ok_bftm;

char g_clbs_time[24] = {0};
bool g_clbs_time_valid = false;
unsigned long g_clbs_epoch = 0;
bool g_clbs_epoch_valid = false;

// Cache for last CLBS result to avoid multiple calls
static CLBSResult g_last_clbs_result = {NULL, false};
static unsigned long g_last_clbs_call_time = 0;
static uint8_t g_clbs_modem_attempt_count = 0;

void resetCLBSModemAttemptCount(void) {
    g_clbs_modem_attempt_count = 0;
}

bool gsmNetworkReadyForClbs(void) {
    /* NETWORK_INIT allowed only so NET_STEP_CLBS (after NETOPEN) can use
     * fetchCLBSEpochTime(); earlier registration steps never call CLBS. */
    return (bool)(currentConnState == NETWORK_INIT ||
                  currentConnState == NETWORK_READY ||
                  currentConnState == CONNECTED);
}

char* extract_iccid(char* response);
char* extract_imei(char* response);
long convertToEpoch(const char *datetime);
void delay2(void);


#define GSM_RESPONSE_TIMEOUT_MS 8000  // Timeout (in ms). CLBS=4 can be slow, so allow more time.
#define GSM_POLL_INTERVAL_MS    10    // Polling interval
#define GSM_AT_PROBE_TIMEOUT_MS 500   // Fast fail when modem is not answering on UART
#define GSM_AT_PROBE_CACHE_MS   3000  // Skip repeat AT probes while link was recently OK (bftm0_ct ms)
#define GSM_CFUN_TIMEOUT_MS     15000 /* CFUN=0/1 can take several seconds for a final OK */
#define GSM_AT_HANDSHAKE_TIMEOUT_MS 2000
#define GSM_AT_HANDSHAKE_ATTEMPTS   5
#define GSM_AT_HANDSHAKE_GAP_MS     1000
#define SYSTICK_BASE 1000	 //us
#define SYSTICK_PRIORITY 0
#define CGSN_MAX_RETRY_COUNT    5

RuntimeParams g_params = {
    .keep_alive_s               = DEFAULT_KEEP_ALIVE_S,
    .payload_mode               = DEFAULT_PAYLOAD_MODE,
    .conn_strategy              = DEFAULT_CONN_STRATEGY,
    .max_power_w                = DEFAULT_MAX_POWER_W,
    .meta_payload_interval_s    = DEFAULT_META_PAYLOAD_INTERVAL_S,
    .send_cooking_sessions      = DEFAULT_SEND_COOKING_SESSIONS,
    .send_error_payloads        = DEFAULT_SEND_ERROR_PAYLOADS,
    .send_meta_payloads         = DEFAULT_SEND_META_PAYLOADS,
		.lb_cap_until_epoch      		= 0,
		.persistent_flag						=	DEFAULT_PERSISTENT_FLAG,
		.num_cooking_blocks					= DEFAULT_NUM_COOKING_BLOCK,
};

void params_reset_to_defaults(void) {
    const RuntimeParams def = {
        .keep_alive_s               = DEFAULT_KEEP_ALIVE_S,
        .payload_mode               = DEFAULT_PAYLOAD_MODE,
        .conn_strategy              = DEFAULT_CONN_STRATEGY,
        .max_power_w                = DEFAULT_MAX_POWER_W,
        .meta_payload_interval_s    = DEFAULT_META_PAYLOAD_INTERVAL_S,
        .send_cooking_sessions      = DEFAULT_SEND_COOKING_SESSIONS,
        .send_error_payloads        = DEFAULT_SEND_ERROR_PAYLOADS,
        .send_meta_payloads         = DEFAULT_SEND_META_PAYLOADS,
				.lb_cap_until_epoch      		= 0,
				.persistent_flag						=	DEFAULT_PERSISTENT_FLAG,
				.num_cooking_blocks					= DEFAULT_NUM_COOKING_BLOCK,
    };
    g_params = def;
}

void strlcpy0(char *dst, const char *src, size_t dstsz) {
    if (!dst || !dstsz) return;
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, dstsz - 1);
    dst[dstsz - 1] = '\0';
}

void shutoff_func(void){
					LEDC_redLED(POWER_KEY, false);
					handlePaygoLED(LED_OFF, 'g');
					handleSyncLED(LED_OFF, 'g');
					clearSevenSegDisplay();
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
//					gracefulDisconnect();
				if (!cookPublishBusy()) {
			  if (g_params.conn_strategy != CONN_ALWAYS_ON) {
					mqttFullTeardown();
				}
				Deepsleep_Start_Flag = sendToGSMAndPrintResponse(HT_USART1,"AT+CSCLK=2","OK");
				}
}

//bool mqtt_send_action_response(const char *type, int cid, bool ok, const char *msg, int qos)
//{
//    cJSON *root = cJSON_CreateObject();
//    if (!root) return false;

//    cJSON_AddStringToObject(root, "ty", type ? type : "");
//    cJSON_AddNumberToObject(root, "cid", cid);
//    cJSON_AddBoolToObject(root, "ok", ok);
//    if (msg && *msg) cJSON_AddStringToObject(root, "msg", msg);

//    if (type && strcmp(type, "pu") == 0) {
//        cJSON *params = cJSON_CreateObject();
//        if (params) {
//            cJSON_AddNumberToObject(params, "keepAlive", g_params.keep_alive_s);
//            cJSON_AddStringToObject(params, "payloadMode", (g_params.payload_mode == 0) ? "all" : "essential");
//            cJSON_AddStringToObject(params, "connStrategy", (g_params.conn_strategy == 0) ? "always_on" : "on_demand");
//            cJSON_AddItemToObject(root, "params", params);
//        }
//    }

//    char *payload = cJSON_PrintUnformatted(root);
//    cJSON_Delete(root);
//    if (!payload) return false;

//    const char *topic = "test";
//    size_t topic_len   = strlen(topic);
//    size_t payload_len = strlen(payload);

//    char cmd[64];
//    bool ok_all = true;

//    ok_all &= sendToGSMAndPrintResponse(HT_USART1, "AT", "OK");

//    snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=0,%zu", topic_len);
//    ok_all &= sendToGSMAndPrintResponse(HT_USART1, cmd, ">");
//    ok_all &= sendToGSMAndPrintResponse(HT_USART1, topic, "OK");

//    snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=0,%zu", payload_len);
//    ok_all &= sendToGSMAndPrintResponse(HT_USART1, cmd, ">");
//    ok_all &= sendToGSMAndPrintResponse(HT_USART1, payload, "OK");


//    snprintf(cmd, sizeof(cmd), "AT+CMQTTPUB=0,%d,60", qos);
//    ok_all &= sendToGSMAndPrintResponse(HT_USART1, cmd, "OK");

//    free(payload);
//		UART0_ClearRxBuffer();
//    return ok_all;
//}


bool mqtt_send_action_response(const char *type, int cid, bool ok, int qos)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return false;

    // Always include cid, ok, and sid
    cJSON_AddNumberToObject(root, "cid", cid);
    cJSON_AddBoolToObject(root, "ok", ok);
    uint32_t sid = Emulated_EEPROM_Read_Serial_ID();
    cJSON_AddNumberToObject(root, "sid", sid);

    if (type && (strcmp(type, "pu") == 0 || strcmp(type, "rp") == 0)) {
        // Parameter update or read params: always return all params
        cJSON *params = cJSON_CreateObject();
        if (!params) { cJSON_Delete(root); return false; }

        cJSON_AddNumberToObject(params, "ka", g_params.keep_alive_s);
				cJSON_AddNumberToObject(params, "ps", g_params.persistent_flag);
        cJSON_AddStringToObject(params, "pm", (g_params.payload_mode == 0) ? "all" : "essential");
        cJSON_AddStringToObject(params, "cs", (g_params.conn_strategy == 0) ? "always_on" : (g_params.conn_strategy == 1) ? "on_demand" : "once");

        cJSON_AddNumberToObject(params, "mp", g_params.max_power_w);
        cJSON_AddNumberToObject(params, "mpi", g_params.meta_payload_interval_s);
				
				char cem[6];
				snprintf(cem, sizeof(cem), "%dX%dX%d",
								 g_params.send_cooking_sessions ? 1 : 0,
								 g_params.send_error_payloads ? 1 : 0,
								 g_params.send_meta_payloads ? 1 : 0);
				cJSON_AddStringToObject(params, "CEM", cem);
				cJSON_AddNumberToObject(params, "ncb", g_params.num_cooking_blocks);

        cJSON_AddItemToObject(root, "params", params);
    }

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return false;

    char topic[32];
    snprintf(topic, sizeof(topic), "resp/%lu", (unsigned long)sid);
    size_t topic_len   = strlen(topic);
    size_t payload_len = strlen(payload);

    char cmd[64];
    bool ok_all = true;

    ok_all &= sendToGSMAndPrintResponse(HT_USART1, "AT", "OK");

    snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=0,%zu", topic_len);
    ok_all &= sendToGSMAndPrintResponse(HT_USART1, cmd, ">");
    ok_all &= sendToGSMAndPrintResponse(HT_USART1, topic, "OK");

    snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=0,%zu", payload_len);
    ok_all &= sendToGSMAndPrintResponse(HT_USART1, cmd, ">");
    ok_all &= sendToGSMAndPrintResponse(HT_USART1, payload, "OK");

    snprintf(cmd, sizeof(cmd), "AT+CMQTTPUB=0,%d,60", qos);
    ok_all &= sendToGSMAndPrintResponse(HT_USART1, cmd, "OK");

    free(payload);
    UART0_ClearRxBuffer();
    return ok_all;
}




void Systick_Configuration(void)
{
	tick_base = SystemCoreClock / 1000000;
	SysTick_Config(tick_base * SYSTICK_BASE);
	NVIC_SetPriority(SysTick_IRQn, SYSTICK_PRIORITY);
}

static void gsm_note_at_uart_ok(void) {
    gsm_at_uart_ok_bftm = (uint32_t)bftm0_ct;
}

/** True when OK appears as a final result line (not embedded in URC text). */
static bool gsm_at_has_final_ok(const char *resp) {
    const char *p;

    if (!resp || !*resp) {
        return false;
    }
    if (strncmp(resp, "OK", 2) == 0 &&
        (resp[2] == '\0' || resp[2] == '\r' || resp[2] == '\n')) {
        return true;
    }
    p = resp;
    while ((p = strstr(p, "OK")) != NULL) {
        if ((p == resp || p[-1] == '\r' || p[-1] == '\n') &&
            (p[2] == '\0' || p[2] == '\r' || p[2] == '\n')) {
            return true;
        }
        p += 2;
    }
    return false;
}

/** True when the AT exchange has a final error result (ERROR or +CME ERROR). */
static bool gsm_at_has_final_error(const char *resp) {
    const char *p;

    if (!resp || !*resp) {
        return false;
    }
    p = resp;
    while ((p = strstr(p, "ERROR")) != NULL) {
        if (p == resp || p[-1] == '\r' || p[-1] == '\n') {
            return true;
        }
        /* +CME ERROR is the final result when CMEE is on. Do not match +IP ERROR. */
        if (p >= resp + 5 && strncmp(p - 5, "+CME ", 5) == 0) {
            return true;
        }
        p += 5;
    }
    return false;
}

static bool gsm_at_response_complete(const char *resp) {
    return (gsm_at_has_final_ok(resp) || gsm_at_has_final_error(resp)) ? true : false;
}

static bool gsm_at_is_crsm_cmd(const char *command) {
    return (command != NULL && strstr(command, "+CRSM=") != NULL) ? true : false;
}

/** CRSM must not finish on a leftover OK; wait until this command's +CRSM: arrives. */
static bool gsm_at_response_complete_for(const char *command, const char *resp) {
    if (!gsm_at_response_complete(resp)) {
        return false;
    }
    if (gsm_at_is_crsm_cmd(command) && strstr(resp, "+CRSM:") == NULL) {
        return false;
    }
    return true;
}

static void gsm_copy_uart_into_response(char *resp, int *index) {
    while (rxd_comm0.cnt > 0 && *index < GSM_BUFFER_SIZE - 1) {
        resp[(*index)++] = (char)rxd_comm0.buffer[rxd_comm0.read_pt];
        rxd_comm0.read_pt = (rxd_comm0.read_pt + 1) % UART0_BUF_SIZE;
        __disable_irq();
        rxd_comm0.cnt--;
        __enable_irq();
    }
    resp[*index] = '\0';
}

/** UART AT exchange without calling GSMisReadyForAT (avoids recursion). */
static GSMResponse gsm_send_at_command_core(const char *command, uint32_t timeout_ms) {
    GSMResponse result = {.success = false, .response = gsm_response_buffer};
    uint32_t start;
    int index = 0;

    if (!result.response || !command || !*command) {
        return result;
    }
    if (gsm_at_exchange_busy) {
        return result;
    }

    gsm_at_exchange_busy = 1;

    UART0_reset_tx_buffer(true);
    UART0_ClearRxBuffer();
    memset(result.response, 0, GSM_BUFFER_SIZE);

    /* Keep UART RX IRQ running so echo is not lost. Direct TX must not race TXC. */
    USART_IntConfig(HT_UART0, USART_INT_TXC, DISABLE);
    UART0_SEND_DATA(command, strlen(command));
    UART0_SEND_DATA("\r\n", 2);

    start = bftm0_ct;
    while ((bftm0_ct - start) < timeout_ms) {
        if (!gsm_data_received) {
            GPTM0_ProcessEvents();
            continue;
        }

        gsm_data_received = false;
        gsm_copy_uart_into_response(result.response, &index);

        if (gsm_at_response_complete_for(command, result.response)) {
            gsm_copy_uart_into_response(result.response, &index);
            result.success = gsm_at_has_final_ok(result.response);
            if (result.success) {
                gsm_note_at_uart_ok();
            } else {
                gsm_at_uart_ok_bftm = 0;
            }
            break;
        }
        GPTM0_ProcessEvents();
    }
    if (!result.success) {
        gsm_at_uart_ok_bftm = 0;
    }
    gsm_at_exchange_busy = 0;
    return result;
}

void gsmInvalidateAtProbeCache(void) {
    gsm_at_uart_ok_bftm = 0;
}

void gsmCfun0Then1(void) {
    (void)gsm_send_at_command_core("AT+CFUN=0", GSM_CFUN_TIMEOUT_MS);
    (void)gsm_send_at_command_core("AT+CFUN=1", GSM_CFUN_TIMEOUT_MS);
    /* CFUN=1 OK is not "ready for any AT" — boot URCs still follow. */
    gsmInvalidateAtProbeCache();
}

bool gsmWaitForAtHandshake(void) {
    uint8_t attempt;

    gsmInvalidateAtProbeCache();
    for (attempt = 0; attempt < GSM_AT_HANDSHAKE_ATTEMPTS; attempt++) {
        GSMResponse r = gsm_send_at_command_core("AT", GSM_AT_HANDSHAKE_TIMEOUT_MS);
        if (r.success) {
            return true;
        }
        UART0_ClearRxBuffer();
        if (attempt + 1u < GSM_AT_HANDSHAKE_ATTEMPTS) {
            uint32_t t = bftm0_ct;
            while ((bftm0_ct - t) < GSM_AT_HANDSHAKE_GAP_MS) {
                GPTM0_ProcessEvents();
            }
        }
    }
    return false;
}

bool GSMprobeATUartOk(void) {
    uint32_t now = (uint32_t)bftm0_ct;
    if (gsm_at_uart_ok_bftm != 0 && (now - gsm_at_uart_ok_bftm) < GSM_AT_PROBE_CACHE_MS) {
        return true;
    }
    GSMResponse r = gsm_send_at_command_core("AT", GSM_AT_PROBE_TIMEOUT_MS);
    return r.success;
}

GSMResponse GSMsendATCommand(const char *command) {
    GSMResponse result = {.success = false, .response = gsm_response_buffer};
    if (!result.response || !command || !*command) {
        return result;
    }
    /* Nested AT during GPTM0_ProcessEvents must not wipe the in-flight buffer. */
    if (gsm_at_exchange_busy) {
        return result;
    }
    if (!GSMisReadyForAT()) {
        memset(result.response, 0, GSM_BUFFER_SIZE);
        return result;
    }

    return gsm_send_at_command_core(command, GSM_RESPONSE_TIMEOUT_MS);
}

float getBatV(void){
    if (gBATV_SingleEOC) {
        gBatVoltage = (float)gBATVoltageLevel / 4095.0;
        gBatVoltage *= 3.2;  // vref = 3.3 or 3.2
        gBatVoltage *= 2;    // Voltage divider reducing max voltage			
				globalPayload.stat.BatV = gBatVoltage;
        return gBatVoltage;
    }
    return 0.0;
}


char* geticcid(void) {
    static char iccid_buf[32];
    GSMResponse res = GSMsendATCommand("AT+CICCID");
    delay();
    if (!res.success || res.response == NULL) {
        iccid_buf[0] = '\0';
        return NULL;
    }
    char* iccid = extract_iccid(res.response);
    if (iccid == NULL) {
        iccid_buf[0] = '\0';
        return NULL;
    }
    strlcpy0(iccid_buf, iccid, sizeof(iccid_buf));
    return iccid_buf;
}

char* getimei(void) {
    static char imei_buf[32];
    if (cgsn_retry_count >= CGSN_MAX_RETRY_COUNT) {
        imei_buf[0] = '\0';
        return NULL;
    }

    GSMResponse res = GSMsendATCommand("AT+CGSN");
    delay();  
    char* imei = extract_imei(res.response);

    if (imei == NULL) {
        imei_buf[0] = '\0';
        if (cgsn_retry_count < CGSN_MAX_RETRY_COUNT) {
            cgsn_retry_count++;
        }
        return NULL;
    } else {
        strlcpy0(imei_buf, imei, sizeof(imei_buf));
        cgsn_retry_count = 0;
        return imei_buf;
    }
}

void resetCGSNRetryCountOnGSMReset(void) {
    if (!__init_sync_time__) {
        cgsn_retry_count = 0;
    }
}

#define FPLMN_EMPTY_HEX   "FFFFFFFFFFFFFFFFFFFFFFFF"
#define FPLMN_READ_CMD    "AT+CRSM=176,28539,0,0,12"
#define FPLMN_CLEAR_CMD   "AT+CRSM=214,28539,0,0,12,\"FFFFFFFFFFFFFFFFFFFFFFFF\""
#define FPLMN_MAX_ATTEMPTS 5

static bool fplmn_crsm_sw_ok(const char *line) {
    return (strstr(line, "144,0") != NULL || strstr(line, "144, 0") != NULL) ? true : false;
}

static bool fplmn_line_has_empty_hex(const char *line) {
    uint8_t run = 0;
    const char *p;

    if (line == NULL) {
        return false;
    }
    for (p = line; *p != '\0' && *p != '\r' && *p != '\n'; p++) {
        if (*p == 'F' || *p == 'f') {
            run++;
            if (run >= 24) {
                return true;
            }
        } else if (*p != '"' && *p != ' ' && *p != ',') {
            run = 0;
        }
    }
    return false;
}

/* True if ANY +CRSM: payload line (not the AT+CRSM= echo) shows empty FPLMN. */
static bool fplmn_read_verify_ok(const char *resp) {
    const char *p = resp;

    if (p == NULL) {
        return false;
    }
    while ((p = strstr(p, "+CRSM:")) != NULL) {
        if (fplmn_crsm_sw_ok(p) && fplmn_line_has_empty_hex(p)) {
            return true;
        }
        p += 6;
    }
    return false;
}

static bool fplmn_write_response_ok(const char *resp) {
    const char *p = resp;

    if (p == NULL) {
        return false;
    }
    while ((p = strstr(p, "+CRSM:")) != NULL) {
        if (fplmn_crsm_sw_ok(p)) {
            return true;
        }
        p += 6;
    }
    return false;
}

static void fplmn_radio_reset(void) {
    gsmCfun0Then1();
    mqtt_service_started = false;
    resetmodem = true;
}

static bool fplmn_crsm_written_this_gsm_boot = false;

void gsmAllowFplmnWriteAfterGsmReboot(void) {
    fplmn_crsm_written_this_gsm_boot = false;
}

GsmFplmnStatus gsm_clear_fplmn(void) {
    if (!isGSMpoweredUp()) {
        return GSM_FPLMN_FAILED;
    }

    if (fplmn_crsm_written_this_gsm_boot) {
        return GSM_FPLMN_SKIPPED;
    }

    GSMResponse initial = gsm_send_at_command_core(FPLMN_READ_CMD, GSM_RESPONSE_TIMEOUT_MS);
    if (fplmn_read_verify_ok(initial.response)) {
        /* Already empty — do not write FFFF or CFUN. */
        return GSM_FPLMN_ALREADY_EMPTY;
    }

    for (uint8_t attempt = 0; attempt < FPLMN_MAX_ATTEMPTS; attempt++) {
        GSMResponse write = gsm_send_at_command_core(FPLMN_CLEAR_CMD, GSM_RESPONSE_TIMEOUT_MS);
        GSMResponse read = gsm_send_at_command_core(FPLMN_READ_CMD, GSM_RESPONSE_TIMEOUT_MS);

        if (fplmn_read_verify_ok(read.response)) {
            fplmn_crsm_written_this_gsm_boot = true;
            if (fplmn_write_response_ok(write.response)) {
                fplmn_radio_reset();
                return GSM_FPLMN_CLEARED;
            }
            /* Read shows empty even if the write URC was missed — stop writing. */
            return GSM_FPLMN_ALREADY_EMPTY;
        }

        if (!fplmn_write_response_ok(write.response)) {
            break;
        }
    }

    /* One write session per GSM boot, even if verify failed. */
    fplmn_crsm_written_this_gsm_boot = true;
    return GSM_FPLMN_FAILED;
}

char* extract_iccid(char* response) {
    if (response == NULL) {
        return NULL;
    }
    char* iccid_start = strstr(response, "+ICCID:");
    if (iccid_start != NULL) {
        iccid_start += strlen("+ICCID: ");
        
        char* iccid_end = strpbrk(iccid_start, "\r\n");
        if (iccid_end != NULL) {
            *iccid_end = '\0';
        }
        
        return iccid_start;
    }
    
    return NULL;
}

char* extract_imei(char* response) {
    // IMEI response format: AT+CGSN returns just the IMEI number (15 digits) followed by \r\nOK\r\n
    // Example: "AT+CGSN\r\n123456789012345\r\nOK\r\n" or just "123456789012345\r\nOK\r\n"
    char* imei_start = response;
    
    // Skip any leading whitespace or AT command echo
    while (*imei_start && (*imei_start == ' ' || *imei_start == '\r' || *imei_start == '\n' || *imei_start == '\t')) {
        imei_start++;
    }
    
    // Skip AT command echo if present (e.g., "AT+CGSN")
    if (strstr(imei_start, "AT+CGSN") != NULL) {
        imei_start = strstr(imei_start, "AT+CGSN");
        imei_start = strchr(imei_start, '\r');
        if (imei_start != NULL) {
            imei_start++; // Skip \r
            while (*imei_start == '\n' || *imei_start == '\r') {
                imei_start++;
            }
        }
    }
    
    // Find the first line that contains exactly 15 consecutive digits
    char* line_start = imei_start;
    while (*line_start) {
        // Check if this line contains only digits and is 15 characters
        int digit_count = 0;
        char* check_pos = line_start;
        
        // Count consecutive digits
        while (*check_pos >= '0' && *check_pos <= '9') {
            digit_count++;
            check_pos++;
        }
        
        // If we found exactly 15 digits followed by \r or \n, this is the IMEI
        if (digit_count == 15 && (*check_pos == '\r' || *check_pos == '\n' || *check_pos == '\0')) {
            // Null-terminate at the end of digits
            char* imei_end = check_pos;
            if (imei_end != NULL) {
                *imei_end = '\0';
            }
            return line_start;
        }
        
        // Move to next line
        char* next_line = strchr(line_start, '\r');
        if (next_line == NULL) {
            next_line = strchr(line_start, '\n');
        }
        if (next_line == NULL) {
            break;
        }
        line_start = next_line + 1; // Skip \r or \n
        while (*line_start == '\n' || *line_start == '\r') {
            line_start++;
        }
    }
    
    return NULL;
}

// Helper function to calculate UTC epoch time manually
// This is needed because mktime() interprets time as local timezone
static long calculateUTCepoch(const struct tm *tm_utc) {
    // Manual calculation of epoch time from UTC datetime
    // Unix epoch: January 1, 1970 00:00:00 UTC
    
    int year = tm_utc->tm_year + 1900;
    int month = tm_utc->tm_mon + 1;
    int day = tm_utc->tm_mday;
    int hour = tm_utc->tm_hour;
    int minute = tm_utc->tm_min;
    int second = tm_utc->tm_sec;
    
    // Days per month (non-leap year)
    const int days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // Calculate days since epoch
    long days = 0;
    
    // Add days for each year since 1970
    for (int y = 1970; y < year; y++) {
        days += 365;
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
            days++; // Leap year
        }
    }
    
    // Add days for each month in current year
    for (int m = 1; m < month; m++) {
        days += days_per_month[m - 1];
        // Add leap day if February has passed and it's a leap year
        if (m == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
            days++;
        }
    }
    
    // Add days for current month (day - 1 because day 1 is the first day)
    days += day - 1;
    
    // Convert days to seconds and add time of day
    long epoch = days * 24L * 3600L;
    epoch += hour * 3600L;
    epoch += minute * 60L;
    epoch += second;
    
    return epoch;
}

long convertToEpoch(const char *datetime) {
    if (!datetime) {
        return -1;
    }

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;

    int parsed = sscanf(datetime, "%4d/%2d/%2d,%2d:%2d:%2d",
                        &year, &month, &day, &hour, &minute, &second);
    if (parsed != 6) {
        parsed = sscanf(datetime, "%2d/%2d/%2d,%2d:%2d:%2d",
                        &year, &month, &day, &hour, &minute, &second);
        if (parsed != 6) {
            return -1;
        }
        year += 2000;  // Normalize 2-digit years to 2000-based
    }

    if (year < 100) {
        year += 2000;
    }

    if (month < 1 || month > 12 || day < 1 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        return -1;
    }

    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    tm.tm_isdst = 0;    // No daylight saving time for UTC/GMT

    // Use manual UTC to epoch conversion to ensure GMT/UTC epoch time
    return (long)calculateUTCepoch(&tm);   
}



void delay2(void)
{
	for(int i=0;i<50000;i++);
}

static void trim_trailing_ws(char *str) {
    size_t len = strlen(str);
    while (len > 0) {
        char c = str[len - 1];
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') {
            str[len - 1] = '\0';
            len--;
        } else {
            break;
        }
    }
}

static void strip_timezone_suffix(char *time_str) {
    for (size_t i = 0; time_str[i] != '\0'; ++i) {
        char c = time_str[i];
        if ((c < '0' || c > '9') && c != ':') {
            time_str[i] = '\0';
            break;
        }
    }
    if (strlen(time_str) > 8) {
        time_str[8] = '\0';
    }
}

void synctime(void) {
	sendToGSM("AT+CNTP=\"pool.ntp.org\",0");
	sendToGSM("AT+CNTP");
}

static CLBSResult extractCoordinatesFromCLBSInternal(bool bypass_cache) {
    static char coordBuffer[64];  // Persistent buffer for coordinates
    CLBSResult result = {coordBuffer, false};
    const unsigned long CLBS_CACHE_SUCCESS_SECONDS = 300;
    const unsigned long CLBS_CACHE_FAIL_SECONDS = 60;

    if (!bypass_cache && g_last_clbs_call_time > 0) {
        unsigned long time_since_last_call = device_time - g_last_clbs_call_time;
        unsigned long cache_ttl = g_last_clbs_result.success ? CLBS_CACHE_SUCCESS_SECONDS : CLBS_CACHE_FAIL_SECONDS;
        if (time_since_last_call < cache_ttl) {
            return g_last_clbs_result;
        }
    }

    /* Do not run CLBS in parallel with network registration AT commands. */
    if (!gsmNetworkReadyForClbs()) {
        return g_last_clbs_result;
    }

    if (g_clbs_modem_attempt_count >= CLBS_MAX_MODEM_ATTEMPTS) {
        return g_last_clbs_result;
    }

    g_clbs_time[0] = '\0';
    g_clbs_time_valid = false;
    g_clbs_epoch = 0;
    g_clbs_epoch_valid = false;

    // Cancel any background LBS retry loop in the module
    UART0_ClearRxBuffer();
    sendToGSM("AT+CLBS=0");
    BFTM0_Delay_ms(500);

    // Request full CLBS info including timestamp
    UART0_ClearRxBuffer();
    g_clbs_modem_attempt_count++;
    sendToGSM("AT+CLBS=4");
    BFTM0_Delay_ms(3000);  // allow modem time to respond

    const char *rxStr = (const char *)rxd_comm0.buffer;
    const char *clbsStart = strstr(rxStr, "+CLBS:");
    if (clbsStart == NULL) {
        result.coordinates = NULL;
        // Update cache even on failure
        g_last_clbs_result = result;
        g_last_clbs_call_time = device_time;
        return result;
    }

    int stat = -1;
    char lat[32] = {0};
    char lon[32] = {0};
    char date_part[16] = {0};
    char time_part[16] = {0};

    int matched = sscanf(clbsStart, "+CLBS: %d,%31[^,],%31[^,],%*[^,],%15[^,],%15s",
                         &stat, lat, lon, date_part, time_part);

    trim_trailing_ws(date_part);
    trim_trailing_ws(time_part);
    strip_timezone_suffix(time_part);

    if (matched >= 5 && stat == 0) {
        snprintf(coordBuffer, sizeof(coordBuffer), "%s,%s", lat, lon);
        result.success = true;
    } else {
        result.coordinates = NULL;
    }

    if (matched >= 5 && strlen(date_part) > 0 && strlen(time_part) >= 6) {
        snprintf(g_clbs_time, sizeof(g_clbs_time), "%s,%s", date_part, time_part);
        g_clbs_time_valid = true;
        long epoch = convertToEpoch(g_clbs_time);
        if (epoch >= 0) {
            g_clbs_epoch = (unsigned long)epoch;
            g_clbs_epoch_valid = true;
        }
    }

    // Update cache
    g_last_clbs_result = result;
    g_last_clbs_call_time = device_time;

    return result;
}

CLBSResult extractCoordinatesFromCLBS(void) {
    return extractCoordinatesFromCLBSInternal(false);
}

bool fetchCLBSEpochTime(void) {
    (void)extractCoordinatesFromCLBSInternal(true);
    return (bool)(g_clbs_epoch_valid && g_clbs_epoch > 0);
}



//ExtractResult extractCurlyBracedPayload(void) {
//	  ExtractResult result = {NULL, false};

//    const char *rxStr = (const char *)rxd_comm0.buffer;
//    const char *start = strchr(rxStr, '{');
//    const char *end = strchr(rxStr, '}');

//    if (!start || !end || end <= start) {
//        return result;
//    }

//    size_t len = end - start + 2; // +1 for '}' and +1 for '\0'
//    result.payload = (char *)malloc(len);
//    if (result.payload == NULL) {
//        return result;
//    }

//    strncpy(result.payload, start, len - 1);
//    result.payload[len - 1] = '\0';

//    result.success = true;
//    return result;
//}

ExtractResult extractCurlyBracedPayload(void) {
    ExtractResult res = { .payload = NULL, .len = 0, .success = false };

    // not enough data for �{}� pair?
    if (rxd_comm0.cnt < 2)  
        return res;

    // scan from read_pt up to cnt bytes
    uint16_t base = rxd_comm0.read_pt;
    for (uint16_t i = 0; i < rxd_comm0.cnt; ++i) {
        uint16_t idx = (base + i) % UART0_BUF_SIZE;
        if (rxd_comm0.buffer[idx] == '{') {
            // found the '{' at offset i � look for matching '}'
            for (uint16_t j = i + 1; j < rxd_comm0.cnt; ++j) {
                uint16_t idx2 = (base + j) % UART0_BUF_SIZE;
                if (rxd_comm0.buffer[idx2] == '}') {
                    res.payload     = (char *)&rxd_comm0.buffer[idx];
                    res.len     = (size_t)(j - i + 1);
                    res.success = true;
                    return res;
                }
            }
            // saw a '{' but no closing '}' yet
            return res;
        }
    }
    // never saw a '{'
    return res;
}
const char *Recode_Start_Data;

//ParsedPayload parsePayload(const char *payload) {
//    ParsedPayload result = {"", 0, "", false};

//    if (!payload || strlen(payload) == 0) {
//        return result;
//    }
//		
//		const char *testStart = "\"ty\": \"";
//    const char *tyStart = strstr(payload, testStart);
//		Recode_Start_Data = tyStart;
//    if (tyStart != NULL) {
//        tyStart += strlen("\"ty\": \"");
//        char *tyEnd = strchr(tyStart, '"');
//        if (tyEnd && (tyEnd - tyStart) < sizeof(result.ty)) {
//            strncpy(result.ty, tyStart, tyEnd - tyStart);
//            result.ty[tyEnd - tyStart] = '\0';
//        } else {
//            return result;
//        }
//    } else {
//        return result;
//    }
//		testStart = "\"cid\":";
//    const char *cidStart = strstr(payload, testStart);
//		Recode_Start_Data = cidStart;
//    if (cidStart != NULL ) {
//        cidStart += strlen("\"cid\":");
//        result.cid = atoi(cidStart);
//    } else {
//        return result;
//    }
//		testStart = "\"data\": \"";
//    const char *dataStart = strstr(payload, testStart);
//		Recode_Start_Data = testStart;
//    if (dataStart != NULL) {
//        dataStart += strlen("\"data\": \"");
//        char *dataEnd = strchr(dataStart, '"');
//        if (dataEnd && (dataEnd - dataStart) < sizeof(result.data)) {
//            strncpy(result.data, dataStart, dataEnd - dataStart);
//            result.data[dataEnd - dataStart] = '\0';
//        } else {
//            return result;
//        }
//    } else {
//        return result;
//    }

//    result.success = true;
//    return result;
//}


/******************************************************************
ParsedPayload parsePayload(const char *payload) {
    ParsedPayload result = {"", 0, "", false, 0};

    if (!payload || strlen(payload) == 0) {
        return result;
    }

    // Parse "ty"
    const char *tyStart = strstr(payload, "\"ty\": \"");
    if (!tyStart) return result;
    tyStart += strlen("\"ty\": \"");
    const char *tyEnd = strchr(tyStart, '"');
    if (!tyEnd || (tyEnd - tyStart) >= sizeof(result.ty)) return result;
    strncpy(result.ty, tyStart, tyEnd - tyStart);
    result.ty[tyEnd - tyStart] = '\0';

    // Parse "cid"
    const char *cidStart = strstr(payload, "\"cid\":");
    if (!cidStart) return result;
    cidStart += strlen("\"cid\":");
    result.cid = atoi(cidStart);

    // Parse "data"
    const char *dataStart = strstr(payload, "\"data\": \"");
    if (!dataStart) return result;
    dataStart += strlen("\"data\": \"");

    char *writePtr = result.data;
    size_t maxLen = sizeof(result.data) - 1;
    while (*dataStart && *dataStart != '"' && maxLen > 0) {
        if (*dataStart == '\\' && *(dataStart + 1) == '"') {
            *writePtr++ = '"';
            dataStart += 2;
        } else {
            *writePtr++ = *dataStart++;
        }
        maxLen--;
    }
    *writePtr = '\0';

    result.success = true;
    return result;
}
***************************************************************************/


ParsedPayload parsePayload(const char *payload) {
    ParsedPayload result = {"", 0, "", false, 0};

    if (!payload || strlen(payload) == 0) {
        return result;
    }

    cJSON *root = cJSON_Parse(payload);
    if (!root) {
				result.success = false;
        return result;
    }

    const cJSON *ty = cJSON_GetObjectItemCaseSensitive(root, "ty");
    const cJSON *cid = cJSON_GetObjectItemCaseSensitive(root, "cid");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");

    if (cJSON_IsString(ty) && ty->valuestring &&
        cJSON_IsNumber(cid) &&
        cJSON_IsString(data) && data->valuestring) {

        strncpy(result.ty, ty->valuestring, sizeof(result.ty) - 1);
        result.cid = cid->valueint;
        strncpy(result.data, data->valuestring, sizeof(result.data) - 1);
        result.success = true;
    }

    cJSON_Delete(root);
    return result;
}


//const char* findHexFilenameInFSLS(void) {
//    const char *target = "eCook3_OTA_TEST1.hex";
//		UART0_ClearRxBuffer();
//		sendToGSM("AT+FSLS");
//		const char *rxBuf = (const char *)rxd_comm0.buffer;
//	
//    char *match = strstr(rxBuf, target);
//    if (match == NULL) {
//        return NULL;  
//    }
//    return match;  
//}

const char* findHexFilenameInFSLS(void) {
    static const char *hex_name = "eCook3_OTA_TEST1.hex";
    static const char *header   = "+FSLS: FILES:\r\n";
    const char *rxBuf;

    // Step 1: Send FSLS and read response
    UART0_ClearRxBuffer();
    sendToGSM("AT+FSLS");
    BFTM0_Delay_ms(1000);  // Wait for FSLS response
    rxBuf = (const char *)rxd_comm0.buffer;

    // Step 2: Check for and delete known unwanted file
    if (strstr(rxBuf, "iPhone_Ring.mp3")) {
        UART0_ClearRxBuffer();
        sendToGSM("AT+FSDEL=iPhone_Ring.mp3");
        BFTM0_Delay_ms(500);  // Wait for delete confirmation

        UART0_ClearRxBuffer();
        sendToGSM("AT+FSLS");
        BFTM0_Delay_ms(1000);  // Wait for FSLS response
        rxBuf = (const char *)rxd_comm0.buffer;
    }

    // Step 3: Find FSLS header
    const char *p = strstr(rxBuf, header);
    if (!p){
//			fileTriggered = false;
			file_bftm0 = 0;
			return NULL;}

    // Step 4: Search for hex filename after header
    p += strlen(header);
    const char *match = strstr(p, hex_name);
    return match;  // NULL if not found
}



void Systick_Delay_us( u32 Dealy_Cnt )
{
	u32 ticks;
	u32 told;
	u32 tnow;
	u32 tcnt;
	u32 reload;

	reload = SysTick->LOAD;
	tcnt = 0;
	told = SysTick->VAL;
	ticks = tick_base * Dealy_Cnt;
	while (1)
	{
		tnow = SysTick->VAL;
		if (tnow != told)
		{
			if (tnow < told)
			{
				tcnt += told - tnow;
			}
			else
			{
				tcnt += reload - tnow + told;
			}
			told = tnow;
			if (tcnt >= ticks)
			{
				break;
			}
		}
	}	
}
