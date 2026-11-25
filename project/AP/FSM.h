#ifndef FSM_H
#define FSM_H
#include "ht32.h"


extern u8 I2C_Tx_Data[8];
extern volatile u8 I2C_Rx_Data[16];
extern u8 I2C1_Tx_Data[8];
extern volatile u8 I2C1_Rx_Data[16];

typedef struct {
    char pan;          
		uint32_t onTime;      
    uint32_t offTime;     
    uint16_t power;    
    bool active;       
} CookingSession;

extern CookingSession leftPanSession;
extern CookingSession rightPanSession;

typedef struct {
    uint32_t left_seconds;
    uint32_t right_seconds;
} CookingTimeMonitor;

extern volatile CookingTimeMonitor cookingTimeMonitor;

typedef struct {
	uint16_t power;
	bool timer;
	u8 mins;
	u8 secs;
	u8 timerDigits[4];
	bool boost;
	volatile bool error;
} plate;

extern plate left;
extern plate right;

typedef enum {
		STATE_ONOFF,
    STATE_IDLE,
    STATE_COOKING,
    STATE_ERROR,
    STATE_FACTORY,
    STATE_LOCK,
    STATE_SYNC,
    STATE_PAYGO,
		STATE_TIMER,
		STATE_COUNTDOWN
} State;

typedef enum {
    EVENT_PAYGO_KEY,                            //0
    EVENT_SYNC_KEY,                                //1
    EVENT_L_PLATE_SELECT_KEY,            //2
    EVENT_KEY_PRESS_0,                        //3
    EVENT_KEY_PRESS_1,                        //4
    EVENT_KEY_PRESS_2,                        //5
    EVENT_KEY_PRESS_3,                        //6
    EVENT_KEY_PRESS_4,                        //7
    EVENT_KEY_PRESS_5,                        //8
    EVENT_KEY_PRESS_6,                        //9
    EVENT_KEY_PRESS_7,                        //10
    EVENT_KEY_PRESS_8,                        //11
    EVENT_KEY_PRESS_9,                        //12
    EVENT_BOOST_KEY,                            //13
        EVENT_R_PLATE_SELECT_KEY,            //14
    EVENT_LOCK_KEY,                                //15
    EVENT_TIMER_KEY,                            //16
    EVENT_ONOFF_KEY,                            //17
    EVENT_READ_ERROR,                            //18
    EVENT_INVALID,                                 // Handle invalid inputs
        EVENT_LOCK_KEY_LONG,                    //20
        EVENT_TIMER_KEY_LONG,         //21
        EVENT_BOOST_KEY_LONG,                    //22
        EVENT_SYNC_KEY_LONG,                    //23
        EVENT_KEY_PRESS_0_B,          //24
        EVENT_L_PLATE_SELECT_KEY_LONG, //25
        EVENT_R_PLATE_SELECT_KEY_LONG  //26
} Event;


// Function prototypes
void enterState(State newState);
void handleEvent(Event event);
void stateIdleHandler(Event event);
void stateCookingHandler(Event event);
//void stateErrorHandler(Event event);
void stateOnOffHandler(Event event);
void stateFactoryHandler(Event event);
void stateLockHandler(Event event);
void stateSyncHandler(Event event);
void statePayGoHandler(Event event);
void stateTimerHandler(Event event);
void stateCountDownHandler(Event event);
Event getEventFromInput(u8 input);
void countdownTimeRight(void);
void countdownTimeLeft(void);
void syncSwitch(void);
void closeSession(CookingSession* session);
void startSessionForSelectedPan(uint8_t pan, uint16_t power);
void batlvlSwitch(void);
extern volatile State currentState;
extern State previousState;
extern uint8_t selectedPan;
void atecdispSwitch(void);
void plateConfigSwitch(void);
void HdispSwitch(void);
void countdownTimeLeftRTC(void);
void countdownTimeRightRTC(void);
void syncDelay(void);
void BoostMode(uint8_t selected_pan, bool enable);
extern bool HTriggered;
extern unsigned long synced_epoch_time;
extern uint16_t ntimer;
extern uint16_t nlock;
extern volatile bool turnOff;
extern bool EVENT_ON_STATUS;
extern bool EVENT_OFF_STATUS;
extern bool CONNECT_COMMAND_STATUS;
void disableHdispSwitch(void);
void updateCookingTimeMonitor(void);

#define currentState currentState
#define lastEvent lastEvent

#endif
