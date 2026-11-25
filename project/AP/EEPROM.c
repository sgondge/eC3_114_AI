#include "EEPROM.h"
#include <string.h>
#include <stdlib.h>
#include "cli.h"
#include "utils.h"
#include "ht32f5xxxx_wdt.h"

CookSession Emulated_EEPROM_Read_Cook_Session(uint32_t session_index)
{
    CookSession session = {0};
    uint32_t session_addr = COOK_SESSION_BASE_ADDR + (session_index * COOK_SESSION_SIZE);
    uint32_t word;

    // Word 0
    word = rw(session_addr);
    session.pan = (char)(word & 0xFF);
    session.onT = (long)((word >> 8) & 0xFFFFFF);

    // Word 1
    word = rw(session_addr + 4);
    session.onT |= ((long)(word & 0xFF) << 24);
    session.offT = (long)((word >> 8) & 0xFFFFFF);

    // Word 2
    word = rw(session_addr + 8);
    session.offT |= ((long)(word & 0xFF) << 24);
    session.pwr = (uint16_t)((word >> 8) & 0xFFFF);

    // Word 3: Ag (float)
    {
        uint32_t ag_word = rw(session_addr + 12);
        memcpy(&session.Ag, &ag_word, sizeof(session.Ag));
    }

    // Word 4: Vg (float)
    {
        uint32_t vg_word = rw(session_addr + 16);
        memcpy(&session.Vg, &vg_word, sizeof(session.Vg));
    }

    // Derived metric unchanged
    session.PwrC = (float)(session.pwr * (session.offT - session.onT)) / 3600.0f;

    return session;
}

// Function to write cooking session to EEPROM at a fixed location
ErrStatus Emulated_EEPROM_Write_Cook_Session(uint32_t session_index,
                                             char pan,
                                             long onT,
                                             long offT,
                                             uint16_t pwr,
                                             float Ag,
                                             float Vg)
{
    FLASH_State FLASHState;
    uint32_t temp_word;
    uint32_t session_addr = COOK_SESSION_BASE_ADDR + (session_index * COOK_SESSION_SIZE);

    // We now write 5 words (20 bytes) total: 3 existing + Ag + Vg
    const uint32_t bytes_needed = 5u * 4u;
    const uint32_t end_addr = session_addr + bytes_needed - 1u;
    if (end_addr > COOK_SESSION_MAX_ADDR) {
        return ERROR;
    }

    // Word 0: [ pan (8) | onT bits [7:0..23] (24) ]
    temp_word = (uint32_t)pan | ((uint32_t)(onT & 0xFFFFFF) << 8);
    FLASHState = FLASH_ProgramWordData(session_addr, temp_word);
    if (FLASHState != FLASH_COMPLETE) return ERROR;

    // Word 1: [ onT bits [24..31] (8) | offT bits [7:0..23] (24) ]
    temp_word = ((uint32_t)(onT >> 24) & 0xFF) | ((uint32_t)(offT & 0xFFFFFF) << 8);
    FLASHState = FLASH_ProgramWordData(session_addr + 4, temp_word);
    if (FLASHState != FLASH_COMPLETE) return ERROR;

    // Word 2: [ offT bits [24..31] (8) | pwr (16) | pad(8)=0 ]
    temp_word = ((uint32_t)(offT >> 24) & 0xFF) | ((uint32_t)pwr << 8);
    FLASHState = FLASH_ProgramWordData(session_addr + 8, temp_word);
    if (FLASHState != FLASH_COMPLETE) return ERROR;

    // Word 3: Ag as raw IEEE-754 bits
    uint32_t ag_word;
    memcpy(&ag_word, &Ag, sizeof(ag_word));
    FLASHState = FLASH_ProgramWordData(session_addr + 12, ag_word);
    if (FLASHState != FLASH_COMPLETE) return ERROR;

    // Word 4: Vg as raw IEEE-754 bits
    uint32_t vg_word;
    memcpy(&vg_word, &Vg, sizeof(vg_word));
    FLASHState = FLASH_ProgramWordData(session_addr + 16, vg_word);
    if (FLASHState != FLASH_COMPLETE) return ERROR;

    return SUCCESS;
}


ErrStatus Emulated_EEPROM_Overwrite_Last_Cook_Session(char pan, long onT, long offT, uint16_t pwr, float Ag, float Vg) {
    uint32_t last_index = COOK_SESSION_MAX_COUNT - 1;
    uint32_t last_addr = COOK_SESSION_BASE_ADDR + (last_index * COOK_SESSION_SIZE);
    uint32_t page_start = (last_addr / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;
    uint32_t page_buf[FLASH_PAGE_SIZE / 4];
    uint32_t i;
    FLASH_State FLASHState;

    // Read entire page into RAM
    for (i = 0; i < (FLASH_PAGE_SIZE / 4); i++) {
        page_buf[i] = rw(page_start + (i * 4));
    }

    // Clear the last session's 5 words in the buffer (set to 0xFF = erased)
    uint32_t offset_in_page = (last_addr - page_start) / 4;
    for (i = 0; i < 5; i++) {
        page_buf[offset_in_page + i] = 0xFFFFFFFF;
    }

    // Erase the page
    FLASHState = FLASH_ErasePage(page_start);
    if (FLASHState != FLASH_COMPLETE) return ERROR;

    // Restore the page (all words except the last session slot)
    for (i = 0; i < (FLASH_PAGE_SIZE / 4); i++) {
        if (page_buf[i] != 0xFFFFFFFF) {
            FLASHState = FLASH_ProgramWordData(page_start + (i * 4), page_buf[i]);
            if (FLASHState != FLASH_COMPLETE) return ERROR;
        }
    }

    // Write the new session at the last slot
    return Emulated_EEPROM_Write_Cook_Session(last_index, pan, onT, offT, pwr, Ag, Vg);
}

ErrStatus Emulated_EEPROM_Erase_All_Cook_Sessions(void) {
    FLASH_State FLASHState;
    uint32_t page;
    uint32_t primask;
    uint32_t first_page = (COOK_SESSION_BASE_ADDR / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;
    /* Last physical page (LIBCFG_FLASH_SIZE, 0x0001FC00 on HT32F54253) is the
       option-byte page, aliased at 0x1FF00000. Meta at 0x0001FF04 shares it.
       Page-erasing it with option-byte protection enabled hard-faults. */
    uint32_t opt_page = LIBCFG_FLASH_SIZE;
    uint32_t cook_end = COOK_SESSION_MAX_ADDR;
    uint32_t last_page;

    if (cook_end >= opt_page) {
        cook_end = opt_page - 1u;
    }
    last_page = (cook_end / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;

    primask = __get_PRIMASK();
    __disable_irq();

    for (page = first_page; page <= last_page; page += FLASH_PAGE_SIZE) {
        WDT_Restart();
        FLASHState = FLASH_ErasePage(page);
        if (FLASHState != FLASH_COMPLETE) {
            __set_PRIMASK(primask);
            return ERROR;
        }
    }

    __set_PRIMASK(primask);
    return Emulated_EEPROM_Write_Cooking_Session_Num(0);
}

/* Shift the sessions after [start, start+count) down. One page at a time so the tail is not copied into RAM.
 * The page buffer is taken from the heap only for the compaction: static RAM has no room for it. */
ErrStatus Emulated_EEPROM_Delete_Cook_Sessions_Range(uint32_t start_index, uint32_t count) {
    int32_t total = Emulated_EEPROM_Read_Cooking_Session_Num();
    uint32_t shift;
    uint32_t dst;
    uint32_t old_end;
    uint32_t new_end;
    uint32_t page;
    uint32_t last_page;
    uint32_t opt_page;
    uint32_t primask;
    uint32_t *cook_page_buf;
    ErrStatus ret = SUCCESS;

    if (total <= 0 || count == 0 || start_index >= (uint32_t)total) {
        return ERROR;
    }
    if (start_index + count > (uint32_t)total) {
        count = (uint32_t)total - start_index;
    }
    if (start_index == 0 && count == (uint32_t)total) {
        return Emulated_EEPROM_Erase_All_Cook_Sessions();
    }

    shift = count * COOK_SESSION_SIZE;
    dst = COOK_SESSION_BASE_ADDR + (start_index * COOK_SESSION_SIZE);
    old_end = COOK_SESSION_BASE_ADDR + ((uint32_t)total * COOK_SESSION_SIZE);
    new_end = old_end - shift;

    opt_page = LIBCFG_FLASH_SIZE;
    if (old_end > opt_page) {
        old_end = opt_page;
    }
    if (dst >= old_end) {
        return Emulated_EEPROM_Write_Cooking_Session_Num((int32_t)start_index);
    }

    page = dst & ~(FLASH_PAGE_SIZE - 1u);
    last_page = (old_end - 1u) & ~(FLASH_PAGE_SIZE - 1u);
    if (last_page >= opt_page) {
        last_page = opt_page - FLASH_PAGE_SIZE;
    }

    cook_page_buf = (uint32_t *)malloc(FLASH_PAGE_SIZE);
    if (cook_page_buf == NULL) {
        return ERROR;
    }

    for (; page <= last_page && ret == SUCCESS; page += FLASH_PAGE_SIZE) {
        uint32_t i;
        FLASH_State st;
        /* Interrupts off per page only, so touch/I2C/UART are not stalled for the whole compact. */
        primask = __get_PRIMASK();
        __disable_irq();
        WDT_Restart();
        for (i = 0; i < (FLASH_PAGE_SIZE / 4u); i++) {
            cook_page_buf[i] = rw(page + (i * 4u));
        }
        for (i = 0; i < (FLASH_PAGE_SIZE / 4u); i++) {
            uint32_t addr = page + (i * 4u);
            if (addr < dst || addr >= old_end) {
                continue;
            }
            if (addr < new_end) {
                /* Source is ahead of dest, so it is still the original flash word. */
                cook_page_buf[i] = rw(addr + shift);
            } else {
                cook_page_buf[i] = 0xFFFFFFFFu;
            }
        }
        st = FLASH_ErasePage(page);
        if (st != FLASH_COMPLETE) {
            ret = ERROR;
        }
        for (i = 0; ret == SUCCESS && i < (FLASH_PAGE_SIZE / 4u); i++) {
            if (cook_page_buf[i] != 0xFFFFFFFFu) {
                st = FLASH_ProgramWordData(page + (i * 4u), cook_page_buf[i]);
                if (st != FLASH_COMPLETE) {
                    ret = ERROR;
                }
            }
        }
        __set_PRIMASK(primask);
    }
    free(cook_page_buf);

    if (ret != SUCCESS) {
        return ERROR;
    }
    return Emulated_EEPROM_Write_Cooking_Session_Num((int32_t)((uint32_t)total - count));
}


void intializeCookNum(void){
	int32_t temp = Emulated_EEPROM_Read_Cooking_Session_Num();

	if(temp == -1)
		Emulated_EEPROM_Write_Cooking_Session_Num(0);
	else if(temp > (int32_t)COOK_SESSION_MAX_COUNT)
		Emulated_EEPROM_Write_Cooking_Session_Num(COOK_SESSION_MAX_COUNT);
}
//write cooking sessions num
ErrStatus Emulated_EEPROM_Write_Cooking_Session_Num(int32_t cook_sess_count) {
    int32_t current_value = rw(NUMBER_COOKING_SESSIONS_ADDRESS);

    if (current_value == cook_sess_count) {
        return SUCCESS;
    }

    FLASH_State FLASHState;
    uint32_t page_address = (NUMBER_COOKING_SESSIONS_ADDRESS / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;
    uint32_t offset_in_page = (NUMBER_COOKING_SESSIONS_ADDRESS - page_address) / 4;
    uint32_t i;

    // Check if we can skip the erase (flash can only clear bits 1->0).
    // If current is erased (0xFFFFFFFF) we just program directly.
    if ((uint32_t)current_value == 0xFFFFFFFF) {
        FLASHState = FLASH_ProgramWordData(NUMBER_COOKING_SESSIONS_ADDRESS, (uint32_t)cook_sess_count);
        return (FLASHState == FLASH_COMPLETE) ? SUCCESS : ERROR;
    }

    // Read-modify-write: preserve the entire page
    uint32_t page_buffer[FLASH_PAGE_SIZE / 4];
    for (i = 0; i < (FLASH_PAGE_SIZE / 4); i++) {
        page_buffer[i] = rw(page_address + (i * 4));
    }

    page_buffer[offset_in_page] = (uint32_t)cook_sess_count;

    FLASHState = FLASH_ErasePage(page_address);
    if (FLASHState != FLASH_COMPLETE) {
        return ERROR;
    }

    for (i = 0; i < (FLASH_PAGE_SIZE / 4); i++) {
        if (page_buffer[i] != 0xFFFFFFFF) {
            FLASHState = FLASH_ProgramWordData(page_address + (i * 4), page_buffer[i]);
            if (FLASHState != FLASH_COMPLETE) {
                return ERROR;
            }
        }
    }

    return SUCCESS;
}

//read cooking sessions num
int32_t Emulated_EEPROM_Read_Cooking_Session_Num(void) {
		//functionStatus = 34;
    return rw(NUMBER_COOKING_SESSIONS_ADDRESS);
}


//Write Number of Plates
ErrStatus Emulated_EEPROM_Write_Plate_Number(uint8_t num_plates) {
		//functionStatus = 35;
    FLASH_State FLASHState;

    FLASHState = FLASH_ProgramWordData(PLATE_NUMBER_ADDRESS, num_plates);
    if (FLASHState != FLASH_COMPLETE) {
        return ERROR;
    }

    return SUCCESS;
}


//Read Plate Number
uint8_t Emulated_EEPROM_Read_Plate_Number(void) {
		//functionStatus = 36;
    return rw(PLATE_NUMBER_ADDRESS);
}

//Write to EEPROM Address
ErrStatus Emulated_EEPROM_Write(u32 word_address, u32 word_data) {
		//functionStatus = 37;
    static u32 word_arr[EEPROM_Length + 1];
    u8 i = 0;
    FLASH_State FLASHState;
    u32 write_address = 0;

    for (i = 0; i < EEPROM_Length; i++) {
        word_arr[i] = Emulated_EEPROM_Read(i);
    }
    word_arr[EEPROM_Length] = '\0';

    word_arr[word_address] = word_data;

    for (i = 0; i < EEPROM_Length; i++) {
        write_address = EEPROM_Address + (i * 4);
//				my_fprintf(HT_USART1, "Writing to address %x\n",write_address);
        FLASHState = FLASH_ProgramWordData(write_address, word_arr[i]);
        if (FLASHState != FLASH_COMPLETE)
            return ERROR;
    }

    return SUCCESS;
}

void EEPROM_Write_Cook(char pan, long onT, long offT, uint16_t pwr) {
		//functionStatus = 38;
    u32 temp_word;
		u32 base_addr = ncook*3;
	
    temp_word = (u32)pan | ((u32)(onT & 0xFFFFFF) << 8);
    Emulated_EEPROM_Write(base_addr+0, temp_word);

    temp_word = ((u32)(onT >> 24) & 0xFF) | ((u32)(offT & 0xFFFFFF) << 8);
    Emulated_EEPROM_Write(base_addr+1, temp_word);

    temp_word = ((u32)(offT >> 24) & 0xFF) | ((u32)(pwr << 8));
    Emulated_EEPROM_Write(base_addr+2, temp_word);
}

u32 Emulated_EEPROM_Read(u32 word_address) {
		//functionStatus = 39;
    u32 read_address = 0;
    read_address = EEPROM_Address + (word_address * 4);
    return rw(read_address);
}

//Write Serial ID
ErrStatus Emulated_EEPROM_Write_Serial_ID(uint32_t serial_id) {
		//functionStatus = 40;
    FLASH_State FLASHState;

    FLASHState = FLASH_ProgramWordData(SERIAL_ID_ADDRESS, serial_id);
    if (FLASHState != FLASH_COMPLETE) {
        return ERROR;
    }

    return SUCCESS;
}

uint32_t Emulated_EEPROM_Read_Serial_ID(void) {
		//functionStatus = 41;
    return rw(SERIAL_ID_ADDRESS);
}

static char key_string_buf[33];

char* Emulated_EEPROM_Read_Secret_Key(void) {
		//functionStatus = 42;
    uint8_t secret_key[16];

    for (u8 i = 0; i < 4; i++) {
        uint32_t word = rw(SECRET_KEY_ADDRESS + (i * 4));
        secret_key[i * 4]     = (word >> 0) & 0xFF;
        secret_key[i * 4 + 1] = (word >> 8) & 0xFF;
        secret_key[i * 4 + 2] = (word >> 16) & 0xFF;
        secret_key[i * 4 + 3] = (word >> 24) & 0xFF;
    }
    for (u8 i = 0; i < 16; i++) {
        snprintf(&key_string_buf[i * 2], 33 - (i * 2), "%02x", secret_key[i]);
    }
    key_string_buf[32] = '\0';
    return key_string_buf;
}

ErrStatus Emulated_EEPROM_Write_RuntimeParams(void) {
    FLASH_State FLASHState;
    uint32_t i;
    uint32_t *pParams;
    uint32_t word_data;
    
   pParams = (uint32_t *)&g_params;
    uint32_t first_word = rw(RUNTIME_PARAMS_ADDRESS);
    bool needs_erase = false;
   for (i = 0; i < (RUNTIME_PARAMS_SIZE / 4); i++) {
        uint32_t current_word = rw(RUNTIME_PARAMS_ADDRESS + (i * 4));
        if (current_word != pParams[i] && current_word != 0xFFFFFFFF) {
            needs_erase = true;
            break;
        }
    }
   if (needs_erase) {
        uint32_t page_address = (RUNTIME_PARAMS_ADDRESS / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;
        uint32_t page_buffer[FLASH_PAGE_SIZE / 4];
        uint32_t *pFlashData;
        uint32_t offset_in_page;
        
        pFlashData = (uint32_t *)page_address;
        for (i = 0; i < (FLASH_PAGE_SIZE / 4); i++) {
            page_buffer[i] = pFlashData[i];
        }
       offset_in_page = (RUNTIME_PARAMS_ADDRESS - page_address) / 4;
        for (i = 0; i < (RUNTIME_PARAMS_SIZE / 4); i++) {
            page_buffer[offset_in_page + i] = pParams[i];
        }
        
        FLASHState = FLASH_ErasePage(page_address);
        if (FLASHState != FLASH_COMPLETE) {
            return ERROR;
        }
        
         for (i = 0; i < (FLASH_PAGE_SIZE / 4); i++) {
            FLASHState = FLASH_ProgramWordData(page_address + (i * 4), page_buffer[i]);
            if (FLASHState != FLASH_COMPLETE) {
                return ERROR;
            }
        }
    } else {
        for (i = 0; i < (RUNTIME_PARAMS_SIZE / 4); i++) {
            word_data = pParams[i];
            uint32_t current_word = rw(RUNTIME_PARAMS_ADDRESS + (i * 4));
            if (current_word != word_data) {
                FLASHState = FLASH_ProgramWordData(RUNTIME_PARAMS_ADDRESS + (i * 4), word_data);
                if (FLASHState != FLASH_COMPLETE) {
                    return ERROR;
                }
            }
        }
    }
    
    return SUCCESS;
}

ErrStatus Emulated_EEPROM_Read_RuntimeParams(void) {
    uint32_t i;
    uint32_t *pParams;
    uint32_t word_data;
    pParams = (uint32_t *)&g_params;
    
    for (i = 0; i < (RUNTIME_PARAMS_SIZE / 4); i++) {
        word_data = rw(RUNTIME_PARAMS_ADDRESS + (i * 4));
        
        if (word_data == 0xFFFFFFFF) {
            continue;
        }
        pParams[i] = word_data;
    }
    
    return SUCCESS;
}
