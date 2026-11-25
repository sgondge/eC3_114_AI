#include "keyScan.h"

void keyScan(){
  Touch_Key_Main_Function();  //Touch Main Loop
    
    if(!TKL_Is_Standby())       //Check Sleep
    {if(TKL_Is_KeyScanCycle()){
        KeyData = TKL_Get_AllKeyState();		
				if(KeyData & (1<<12)) 
				{ printf("\"9 Pressed\"-tkey12\r\n");
					a=1;//KEY0 PRESS
        }
				else if(KeyData & (1<<13)) 
				{ printf("\"BOOST Pressed\"-tkey13\r\n");
					a=1;//KEY0 PRESS
        }
				else if(KeyData & (1<<0)) 
				{ printf("\"PAYGO Pressed\"-tkey0\r\n");
					a=1;//KEY0 PRESS
        }
        else if(KeyData & (1<<1)) 
				{ printf("\"SYNC Pressed\"-tkey1\r\n");
					a=1;//KEY0 PRESS
        }
				else if(KeyData & (1<<2)) 
				{ printf("\"L Plate Pressed\"-tkey2\r\n");
					a=1;//KEY0 PRESS
        }
				else if(KeyData & (1<<3)) 
				{ printf("\"0 Pressed\"-tkey3\r\n");
					a=1;//KEY0 PRESS
        }	
				else if(KeyData & (1<<4)) 
				{ printf("\"2 Pressed\"-tkey4\r\n");
					a=1;//KEY0 PRESS
        }			
				else if(KeyData & (1<<5)) 
				{ printf("\"1 Pressed\"-tkey5\r\n");
					a=1;//KEY0 PRESS
        }				
				else if(KeyData & (1<<6)) 
				{ printf("\"3 Pressed\"-tkey6\r\n");
					a=1;//KEY0 PRESS
        }
				else if(KeyData & (1<<7)) 
				{ printf("\"4 Pressed\"-tkey7\r\n");
					a=1;//KEY0 PRESS
        }
				else if(KeyData & (1<<8)) 
				{ printf("\"5 Pressed\"-tkey8\r\n");
					a=1;//KEY0 PRESS
        }
				else if(KeyData & (1<<9)) 
				{ printf("\"6 Pressed\"-tkey9\r\n");
					a=1;//KEY0 PRESS
				}
				else if(KeyData & (1<<10)) 
				{ printf("\"7 Pressed\"-tkey10\r\n");
					a=1;//KEY0 PRESS
				}
				else if(KeyData & (1<<11)) 
				{ printf("\"8 Pressed\"-tkey11\r\n");
					a=1;//KEY0 PRESS
				}
				else if(KeyData & (1<<14)) 
				{ printf("\"R Plate Pressed\"-tkey14\r\n");
					a=1;//KEY0 PRESS
				}
				else if(KeyData & (1<<15)) 
				{ printf("\"LOCK Pressed\"-tkey15\r\n");
					a=1;//KEY0 PRESS
				}					
				else if(KeyData & (1<<17)) 
				{ printf("\"POWER Pressed\"-tkey17\r\n");
					a=1;//KEY0 PRESS
        }				
				else if(KeyData & (1<<16)) 
				{ printf("\"TIMER Pressed\"-tkey16\r\n");      
					a=1;//KEY0 PRESS
        }
        else
				{  
          a=0;	//KEY0 RELEASE
        }
      }
    }
    else{ //Touch Key Sleep
      /******************************standby note************************************
      //After power-on is stable, in the absence of any keystrokes or interference, 
      //TKL_Is_Standby() will return 0 within 1 second, indicating that it 
      //cannot go to sleep, and after 1 second, TKL_Is_Standby() will return 1, 
      //indicating that it can go to sleep. 
      ******************************************************************************/
      /*********** Sleep mode loop *****************/
      while(TKL_Is_Standby()){
        if(Touch_Key_Main_Sleep_Function(SEL_SLEEP_MODE)){ //if no touch, keep sleep
         #if SEL_SLEEP_MODE == USE_SLEEP_MODE
          PWRCU_Sleep(PWRCU_SLEEP_ENTRY_WFI);              //31.25ms form RTC wake up
         #endif
          
         #if SEL_SLEEP_MODE == USE_DEEP_SLEEP1_MODE
          PWRCU_DeepSleep1(PWRCU_SLEEP_ENTRY_WFI);         //31.25ms form RTC wake up
         #endif
          
         #if SEL_SLEEP_MODE == USE_DEEP_SLEEP2_MODE
          PWRCU_DeepSleep2(PWRCU_SLEEP_ENTRY_WFI);         //31.25ms form RTC wake up
         #endif
        }
      }
      ExitSleepModePrepare();
    }
  }
