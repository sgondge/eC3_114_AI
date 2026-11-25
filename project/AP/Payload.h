#ifndef __PAYLOAD_H
#define __PAYLOAD_H
#include <stdint.h>
#include "ht32.h"

//extern const char* DEFAULT_FW_URL;
extern const char* fw_url;
	
typedef enum {
    DISCONNECTED,
    NETWORK_INIT,
    NETWORK_READY,  /* PDP/NETOPEN done; MQTT deferred until not STATE_ONOFF */
    CONNECTING,
    CONNECTED,
    DISCONNECTING
} ConnState;

typedef enum {
	CONNECT,
	DISCONNECT
} ConnEvent;

typedef struct{
	uint32_t sn;
	char* FW; 
	uint8_t pt; 
	uint32_t mc; 
	char* sid;
	char* loc;
} devID;

typedef struct{
	uint32_t Vg;
	float Ag;
	float Pg;
	float BatV;
	char* STL;
	uint32_t stvcnt;
} stat;

typedef struct{
	long ts;
	uint8_t err;
} errC;

typedef struct{
	char pan;
	long onT;
	long offT;
	uint16_t pwr;
	float PwrC;
} Cook;

typedef struct {
	devID devID;
	uint32_t ts;
	stat stat;
	errC* errC;
	Cook* Cook;
	char* status;
	uint32_t GSMsignalStrength;
} Payload;

extern FlagStatus Bat_CHG;

extern ConnState currentConnState;
extern Payload globalPayload;
extern uint32_t ncook;
extern uint32_t nerr;
extern uint32_t nconn;
extern bool __init_sync_time__;
extern bool conn;
extern bool fileTriggered;
extern bool isSubscribedTopic;
extern bool mqtt_service_started;
extern bool resetmodem;

extern u8 PayloadOption;
void sendCookingSessionsChunk(int offset);
void addErrC(long ts, uint8_t err);
void addCookSession(char pan, long onT, long offT, uint16_t pwr);
void updatePayload(void);
void sendPayload(void);
void connect(void);
bool sendCookingSessions(void);
void gracefulDisconnect(void);
void mqttFullTeardown(void);
void connCheck(void);
void connDelay(uint32_t delay);
void sendToGSM(const char* data);
void subscribeTopic(void);
int get_serial_id_length(void);
void subscribeSerial(void);
void fwDownload(char data[]);
void checkMQTTConnection(void);
extern uint32_t file_bftm0;
extern uint32_t update_bftm0;
extern bool downloadInProgress;
extern bool updateTriggered;
bool send24Hrs(void);
void payloadSend(void);
void freeCookSessions(void);
void enterConnState(ConnState state);
void handleConnect(ConnEvent event);
void networkInitCheck(void);
void networkInitRestart(void);
/** Allow another EF_FPLMN CRSM write after leaving STATE_ONOFF (does not disarm FPLMN step). */
void networkAllowFplmnAfterStoveOn(void);
void mqttcheckStart(void);
/** False in STATE_ONOFF and STATE_FACTORY — MQTT must not start while stove is off. */
bool mqttConnectAllowed(void);
/** Stop connect/retry locally with no teardown AT commands. Used on factory entry. */
void factoryConnHold(void);
void initGlobalPayload(void);
bool sendRetryDetails(void);
void payloadDelay(void);
bool sendCookingSessionsAggregated(void);
void mqttcheckDelay(void);
/** OTA "mr": mark meta due; published on next existing MQTT session via send24Hrs(). */
void metaForceRequest(void);
extern bool g_check_cook_publish;
/** True from cook chunk scheduling until its +CMQTTPUB result is handled and flash is updated. */
bool cookPublishBusy(void);
extern unsigned long send_last_rtc_meta;
extern const u16 POWER_LEVEL[2][11];
#endif
