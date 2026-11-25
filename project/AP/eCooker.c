// Configure this stove

#include "eCooker.h"
#include "EEPROM.h"
#include "Utils.h"
#include "I2C0.h"
#include "I2C1.h"
#include "FSM.h"

//SINGLE GLOBAL eCooker configuration structure
eCooker_t eCooker;
bool ecConfig = false;

void eCooker_Configuration (void) {
	memset (&eCooker, 0, sizeof(eCooker));
	eCooker.eCookerVersion = 3;
	eCooker.gsmModemType = 4;
  globalPayload.devID.FW = "1.1.3.1";
	globalPayload.devID.mc = I2C_Rx_Data[0];
	
	if(i2c1_master_connected || (I2C_Rx_Data[1] == 1) || (I2C1_Rx_Data[1] == 1) || (I2C1_Rx_Data[1] == 2)){
		eCooker.numberOfPlates = DOUBLE_STOVE;
		globalPayload.devID.pt=DOUBLE_STOVE; 
		ecConfig = true;
		return;}
	else if(I2C_Rx_Data[1] == 3) {
		eCooker.numberOfPlates = SINGLE_STOVE; 
		globalPayload.devID.pt = SINGLE_STOVE;
		ecConfig = true;
		return;}
	else{
		ecConfig = false;
	}
}

bool isStoveI2cConnected(void) {
	return ((i2c0_master_connected != 0) || (i2c1_master_connected != 0)) ? true : false;
}
