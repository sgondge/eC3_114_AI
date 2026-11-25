#ifndef __UTILS_H
#define __UTILS_H

#include "UART0.h"
//#include "USART1.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

//extern u8 functionStatus;

// ===== enums =====
typedef enum { PAYLOAD_ALL = 0, PAYLOAD_ESSENTIAL = 1 } PayloadMode;
typedef enum { CONN_ALWAYS_ON = 0, CONN_ON_DEMAND = 1, CONN_ONCE = 2 } ConnStrategy;

// ===== runtime parameters (the only things "pu" can change) =====
typedef struct {
    int          keep_alive_s;
    PayloadMode  payload_mode;
    ConnStrategy conn_strategy;
    int          max_power_w;
    int          meta_payload_interval_s;
    bool         send_cooking_sessions;    
    bool         send_error_payloads;     
    bool         send_meta_payloads;
		uint32_t     lb_cap_until_epoch;
		int          persistent_flag;
		int 				 num_cooking_blocks;
} RuntimeParams;

// ===== defaults =====
#define DEFAULT_KEEP_ALIVE_S   600
#define DEFAULT_PAYLOAD_MODE   PAYLOAD_ALL
#define DEFAULT_CONN_STRATEGY  CONN_ON_DEMAND
#define DEFAULT_MAX_POWER_W 10
#define DEFAULT_META_PAYLOAD_INTERVAL_S 24*60*60
#define DEFAULT_SEND_COOKING_SESSIONS true
#define DEFAULT_SEND_ERROR_PAYLOADS true
#define DEFAULT_SEND_META_PAYLOADS true
#define DEFAULT_LOAD_BALANCER_DURATION 60
#define DEFAULT_PERSISTENT_FLAG 0
#define DEFAULT_NUM_COOKING_BLOCK 2


typedef struct {
    bool success;
    char *response;
} GSMResponse;

typedef struct {
    char *coordinates;  // dynamically allocated "latitude,longitude"
    bool success;
} CLBSResult;

extern char g_clbs_time[24];
extern bool g_clbs_time_valid;
extern unsigned long g_clbs_epoch;
extern bool g_clbs_epoch_valid;

/** Max AT+CLBS modem transactions per Power-ON cycle (epoch + location share this budget). */
#define CLBS_MAX_MODEM_ATTEMPTS 5
void resetCLBSModemAttemptCount(void);
/** True in NETWORK_INIT / NETWORK_READY / CONNECTED (callers must not invoke before NETOPEN). */
bool gsmNetworkReadyForClbs(void);

//typedef struct {
//    char *payload;
//    bool success;
//} ExtractResult;
typedef struct {
    const char *payload;  // start of �{� in buffer
    size_t      len;  // number of bytes from �{� through �}�
    bool        success;
} ExtractResult;

typedef struct {
    char ty[4];      // Type string, e.g. "FW"
    int cid;         // Integer value
    char data[128];  // Adjust size as needed for the expected length of data
    bool success;    // Flag to indicate successful parsing
    int xapi;        // API key flag: 0 = no API key, 1 = use API key
} ParsedPayload;

void Systick_Configuration(void);

GSMResponse GSMsendATCommand(const char *command);
bool GSMprobeATUartOk(void);
void gsmInvalidateAtProbeCache(void);
/** AT+CFUN=0, wait for OK/ERROR, then AT+CFUN=1 and wait again. Invalidates AT-OK cache after CFUN=1. */
void gsmCfun0Then1(void);
/** Fresh AT handshake (no cache). True when a complete AT OK is received. */
bool gsmWaitForAtHandshake(void);

char* geticcid(void);
char* getimei(void);
void resetCGSNRetryCountOnGSMReset(void);

typedef enum {
    GSM_FPLMN_ALREADY_EMPTY,
    GSM_FPLMN_CLEARED,
    GSM_FPLMN_FAILED,
    GSM_FPLMN_SKIPPED, /* CRSM write already done this GSM power cycle */
} GsmFplmnStatus;

/** Clear SIM FPLMN on recovery: read-first, at most one CRSM write per GSM reboot. */
GsmFplmnStatus gsm_clear_fplmn(void);
/** Allow one more EF_FPLMN write after a PWRKEY GSM reboot (not after CFUN). */
void gsmAllowFplmnWriteAfterGsmReboot(void);
void synctime(void);
float getBatV(void);

extern ParsedPayload fwUpdate;
extern ExtractResult result;
extern RuntimeParams g_params;

void params_reset_to_defaults(void);
CLBSResult extractCoordinatesFromCLBS(void);
bool fetchCLBSEpochTime(void);  // Fetch CLBS epoch without cache (for epoch sync)
ExtractResult extractCurlyBracedPayload(void);
ParsedPayload parsePayload(const char *payload);
void Systick_Delay_us( u32 Dealy_Cnt );
const char* findHexFilenameInFSLS(void);
void strlcpy0(char *dst, const char *src, size_t dstsz);
//bool mqtt_send_action_response(const char *type, int cid, bool ok, const char *msg, int qos);
bool mqtt_send_action_response(const char *type, int cid, bool ok, int qos);
void shutoff_func(void);
void params_reset_to_defaults(void);
#endif
