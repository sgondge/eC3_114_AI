#include "ht32f5xxxx_flash.h"
#include "Payload.h"
#include "Utils.h"

#define EEPROM_Address 0x0000FF90 
#define EEPROM_Length 4

//#define SERIAL_ID_ADDRESS 0x000017F0
//#define SECRET_KEY_ADDRESS 0x000017F4
#define SERIAL_ID_ADDRESS 0x00002C00
#define SECRET_KEY_ADDRESS 0x00002C04

#define NUMBER_COOKING_SESSIONS_ADDRESS 	0x00018C00
#define COOK_SESSION_BASE_ADDR 0x00019000
#define COOK_SESSION_MAX_ADDR 0x0001FF00
#define COOK_SESSION_SIZE      20
#define COOK_SESSION_MAX_COUNT ((COOK_SESSION_MAX_ADDR - COOK_SESSION_BASE_ADDR + 1 - COOK_SESSION_SIZE) / COOK_SESSION_SIZE + 1)

#define META_INFO_START_ADDRESS   0x0001FF04
#define PLATE_NUMBER_ADDRESS      (META_INFO_START_ADDRESS)

#define RUNTIME_PARAMS_ADDRESS    0x00002C20
#define RUNTIME_PARAMS_SIZE       ((uint32_t)sizeof(RuntimeParams))

#if (RUNTIME_PARAMS_ADDRESS < (SECRET_KEY_ADDRESS + 16))
#error "RUNTIME_PARAMS_ADDRESS overlaps with Secret Key!"
#endif
#if (RUNTIME_PARAMS_ADDRESS < (SERIAL_ID_ADDRESS + 4))
#error "RUNTIME_PARAMS_ADDRESS overlaps with Serial ID!"
#endif


// Function to write cooking session to EEPROM at a fixed location
typedef struct {
    char pan;
    long onT;      
    long offT;    
    uint16_t pwr;   
    float PwrC;
		float Ag;
		float Vg;
} CookSession;

ErrStatus Emulated_EEPROM_Write(u32 write_address, u32 word_data);
u32 Emulated_EEPROM_Read(u32 word_address);
void EEPROM_Write_Cook(char pan, long onT, long offT, uint16_t pwr);

//Serial ID and Secret Key
ErrStatus Emulated_EEPROM_Write_Serial_ID(u32 serial_id);
u32 Emulated_EEPROM_Read_Serial_ID(void);
char* Emulated_EEPROM_Read_Secret_Key(void);

//Plate Version
uint8_t Emulated_EEPROM_Read_Plate_Number(void);
ErrStatus Emulated_EEPROM_Write_Plate_Number(uint8_t num_plates);

ErrStatus Emulated_EEPROM_Write_Cook_Session(uint32_t session_index, char pan, long onT, long offT, uint16_t pwr, float Ag, float Vg);
ErrStatus Emulated_EEPROM_Overwrite_Last_Cook_Session(char pan, long onT, long offT, uint16_t pwr, float Ag, float Vg);
CookSession Emulated_EEPROM_Read_Cook_Session(uint32_t session_index);

ErrStatus Emulated_EEPROM_Write_Cooking_Session_Num(int32_t cook_sess_count);
int32_t Emulated_EEPROM_Read_Cooking_Session_Num(void);

void intializeCookNum(void);
ErrStatus Emulated_EEPROM_Erase_All_Cook_Sessions(void);
ErrStatus Emulated_EEPROM_Delete_Cook_Sessions_Range(uint32_t start_index, uint32_t count);

ErrStatus Emulated_EEPROM_Write_RuntimeParams(void);
ErrStatus Emulated_EEPROM_Read_RuntimeParams(void);


int32_t Emulated_EEPROM_Get_Effective_Session_Count(void);

