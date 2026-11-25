#include "UART0.h"
#include "string.h"
#include <stdlib.h>
#include <ctype.h>

//-----------------------------------------------------------------------------
__ALIGN4 _UART0_STRUCT rxd_comm0;
__ALIGN4 _UART0_STRUCT txd_comm0;

__attribute__((aligned(4))) uint8_t IAP_Buffer[GSM_BUFFER_SIZE];

volatile bool gsm_data_received = false;
volatile u8 gsm_at_exchange_busy = 0;
volatile u16 gsm_mqtt_pub_seq = 0;
volatile u8 gsm_mqtt_pub_ok = 0;

static char uart0_msg_stage[UART0_MSG_MAX];
static u16 uart0_msg_stage_len = 0;
static u8 uart0_msg_in = 0;
static char uart0_msg_ring[UART0_MSG_RING_SIZE];
static u16 uart0_msg_wr = 0;
static u16 uart0_msg_rd = 0;
static volatile u16 uart0_msg_cnt = 0;
static u8 uart0_pub_i = 0;
static u8 uart0_pub_code = 0;

/* Runs on every received byte (IRQ, or with UART0 IRQ masked), before the raw ring can drop or clear it. */
static void UART0_msg_feed(u8 b)
{
  static const char pub_pfx[] = "+CMQTTPUB: 0,";
  u16 i;

  if (b == '\n') {
    uart0_pub_i = 0;
  } else if (uart0_pub_i < 13) {
    uart0_pub_i = (b == (u8)pub_pfx[uart0_pub_i]) ? (u8)(uart0_pub_i + 1) : 0xFF;
  } else if (uart0_pub_i == 13) {
    if (b >= '0' && b <= '9') {
      uart0_pub_code = b;
      uart0_pub_i = 14;
    } else {
      uart0_pub_i = 0xFF;
    }
  } else if (uart0_pub_i == 14) {
    /* ok before seq: a reader that sees the new seq must also see its result. */
    gsm_mqtt_pub_ok = (uart0_pub_code == '0' && (b < '0' || b > '9')) ? 1 : 0;
    gsm_mqtt_pub_seq++;
    uart0_pub_i = 0xFF;
  }

  /* Commands are flat objects: '{' starts, the first '}' ends, a new '{' restarts. */
  if (b == '{') {
    uart0_msg_in = 1;
    uart0_msg_stage_len = 0;
  }
  if (!uart0_msg_in) {
    return;
  }
  if (uart0_msg_stage_len >= (UART0_MSG_MAX - 1)) {
    uart0_msg_in = 0;
    return;
  }
  uart0_msg_stage[uart0_msg_stage_len++] = (char)b;
  if (b != '}') {
    return;
  }
  uart0_msg_in = 0;
  uart0_msg_stage[uart0_msg_stage_len] = '\0';
  /* Echoed uplink payloads have no "ty"; only downlink commands are queued. */
  if (strstr(uart0_msg_stage, "\"ty\"") == NULL || strstr(uart0_msg_stage, "\"cid\"") == NULL) {
    return;
  }
  if ((u16)(uart0_msg_stage_len + 1u) > (u16)(UART0_MSG_RING_SIZE - uart0_msg_cnt)) {
    return;
  }
  for (i = 0; i <= uart0_msg_stage_len; i++) {
    uart0_msg_ring[uart0_msg_wr] = uart0_msg_stage[i];
    uart0_msg_wr = (uart0_msg_wr + 1) % UART0_MSG_RING_SIZE;
  }
  uart0_msg_cnt += (u16)(uart0_msg_stage_len + 1u);
}

u16 UART0_MsgPop(char *out, u16 size)
{
  u16 len = 0;
  u16 used = 0;
  char c;

  if (uart0_msg_cnt == 0 || out == NULL || size == 0) {
    return 0;
  }
  do {
    c = uart0_msg_ring[uart0_msg_rd];
    uart0_msg_rd = (uart0_msg_rd + 1) % UART0_MSG_RING_SIZE;
    used++;
    if (c != '\0' && len < (u16)(size - 1u)) {
      out[len++] = c;
    }
  } while (c != '\0');
  out[len] = '\0';

  NVIC_DisableIRQ(UART0_IRQn);
  uart0_msg_cnt -= used;
  NVIC_EnableIRQ(UART0_IRQn);
  return len;
}

u32 tick_base;

uint32_t fileAttribute = 0;
volatile char last_tx_byte = 0;

volatile bool suppress_cmd = false;
volatile bool suppress_resp = false;

volatile int cmd_match_idx = 0;
volatile int resp_match_idx = 0;

const char cftrantx_cmd[] = "AT+CFTRANTX=";
const char cftrantx_resp[] = "+CFTRANTX: DATA";

//-----------------------------------------------------------------------------
void UART0_Configuration(void)
{
  USART_InitTypeDef USART_InitStruct;
  CKCU_PeripClockConfig_TypeDef CKCUClock = {{0}};
  CKCUClock.Bit.UART0   = 1;
  CKCUClock.Bit.AFIO    = 1;
  CKCUClock.Bit.PB      = 1;
  CKCU_PeripClockConfig(CKCUClock, ENABLE);

  GPIO_PullResistorConfig(HT_GPIOB, GPIO_PIN_12, GPIO_PR_UP);  //Turn on UxART Rx internal pull up resistor to prevent unknow state

  AFIO_GPxConfig(GPIO_PB, AFIO_PIN_9, AFIO_FUN_USART_UART);  // UR0TX Config AFIO mode
  AFIO_GPxConfig(GPIO_PB, AFIO_PIN_12, AFIO_FUN_USART_UART);  // UR0RX Config AFIO mode

  USART_InitStruct.USART_BaudRate = 115200;
  USART_InitStruct.USART_WordLength = USART_WORDLENGTH_8B;
  USART_InitStruct.USART_StopBits = USART_STOPBITS_1;
  USART_InitStruct.USART_Parity = USART_PARITY_NO;
  USART_InitStruct.USART_Mode = USART_MODE_NORMAL;
  USART_Init(HT_UART0, &USART_InitStruct);
  USART_RxCmd(HT_UART0, ENABLE);
  USART_TxCmd(HT_UART0, ENABLE);

  USART_IntConfig(HT_UART0, USART_INT_RXDR, ENABLE);

  //NVIC_EnableIRQ(UART0_IRQn);
  UART0_init_buffer();
}

//-----------------------------------------------------------------------------
void UART0_init_buffer (void)
{
  rxd_comm0.read_pt = 0;
  rxd_comm0.write_pt = 0;
  rxd_comm0.cnt = 0;

  txd_comm0.read_pt = 0;
  txd_comm0.write_pt = 0;
  txd_comm0.cnt = 0;
}
//-----------------------------------------------------------------------------
void UART0_SEND_DATA(const char* data,u16 Length)
{
		const u16 CHUNK_SIZE = 64;  // Adjust based on UART buffer size
    u16 bytes_remaining = Length;
    
    while(bytes_remaining > 0) {
			u16 i ;
        u16 chunk_size = (bytes_remaining > CHUNK_SIZE) ? CHUNK_SIZE : bytes_remaining;
        
        // Directly transmit without malloc
        for(i = 0; i < chunk_size; i++) {
            USART_SendData(HT_UART0, data[i]);
            // bounded wait for TX FIFO ready
            int guard = 20000;
            while(USART_GetFlagStatus(HT_UART0, USART_FLAG_TXDE) == RESET) {
                if (--guard <= 0) { break; }
            }
        }
        
        data += chunk_size;
        bytes_remaining -= chunk_size;
    }
}

void UART0_IRQHandler(void)
{
  if ((HT_UART0->SR) & USART_FLAG_TXC)
  {
    if (!txd_comm0.cnt)
    {
      USART_IntConfig(HT_UART0, USART_INT_TXC, DISABLE);
    }
    else
    {
      u8 i;
      for (i = 0; i < UART0_FIFO_LEN; i++)   //have FIFO?
      {
        USART_SendData(HT_UART0, txd_comm0.buffer[txd_comm0.read_pt]);
        txd_comm0.read_pt = (txd_comm0.read_pt + 1) % UART0_BUF_SIZE;
        txd_comm0.cnt--;
        if (!txd_comm0.cnt)
          break;
      }
    }
  }

  /* One byte per IRQ keeps the soft ring from filling on fire-and-forget AT bursts
   * (MQTT connect setup). Hardware leftovers are flushed in UART0_ClearRxBuffer(). */
  if ((HT_UART0->SR) & USART_FLAG_RXDR) {
    u8 b = (u8)USART_ReceiveData(HT_UART0);
    UART0_msg_feed(b);
    if (rxd_comm0.cnt < UART0_BUF_SIZE) {
      rxd_comm0.buffer[rxd_comm0.write_pt] = b;
      rxd_comm0.write_pt = (rxd_comm0.write_pt + 1) % UART0_BUF_SIZE;
      rxd_comm0.cnt++;
      gsm_data_received = true;
    }
  }
  if ((HT_UART0->SR) & USART_FLAG_OE) {
    USART_ClearFlag(HT_UART0, USART_FLAG_OE);
  }
}

int UART0_getchar(void)
{
	uint8_t c;
	
	// Check if buffer is empty first (no need to disable interrupts for this)
	if(!rxd_comm0.cnt)
		return -1;

	// Disable UART0 interrupt (not USART1!) to protect critical section
	NVIC_DisableIRQ(UART0_IRQn);
	
	// Read the buffer FIRST while interrupts are disabled
	c = rxd_comm0.buffer[rxd_comm0.read_pt];
	
	// Then update pointers and count atomically
	rxd_comm0.read_pt = (rxd_comm0.read_pt + 1) % UART0_BUF_SIZE;
	rxd_comm0.cnt--;
	
	// Re-enable interrupts
	NVIC_EnableIRQ(UART0_IRQn);
	
	return (int) c;
}


//-----------------------------------------------------------------------------
int UART0_getBufferSpace (void) {
	return (txd_comm0.read_pt > txd_comm0.write_pt)? txd_comm0.write_pt - txd_comm0.read_pt : UART0_BUF_SIZE - txd_comm0.read_pt + txd_comm0.write_pt;
}

void sendCommandGSM(const char* data) {
	UART0_tx_data((u8*)data, strlen(data));
	UART0_tx_data((u8*)"\r\n", 2);
}

void UART0_tx_data(u8 *pt, u16 len)
{
 while(len--)
  {
    txd_comm0.buffer[txd_comm0.write_pt] = *pt++;
    txd_comm0.write_pt = (txd_comm0.write_pt + 1) % UART0_BUF_SIZE;
    NVIC_DisableIRQ(UART0_IRQn);
    txd_comm0.cnt++;
    NVIC_EnableIRQ(UART0_IRQn);
  }
  if(txd_comm0.cnt)
    USART_IntConfig(HT_UART0,  USART_INT_TXC , ENABLE);
}


void UART0_reset_tx_buffer(bool clear_content){
	  NVIC_DisableIRQ(UART0_IRQn);
    if (clear_content) {
        memset(txd_comm0.buffer, 0, UART0_BUF_SIZE);
    }
    txd_comm0.read_pt = 0;
    txd_comm0.write_pt = 0;
    txd_comm0.cnt = 0;
    NVIC_EnableIRQ(UART0_IRQn);
    USART_IntConfig(HT_UART0, USART_INT_TXC, DISABLE);
}

void UART0_ClearRxBuffer(void) {
    u16 drained = 0;

    NVIC_DisableIRQ(UART0_IRQn);
    /* Software ring only is not enough: USART may still hold echo/OK from the
     * previous AT command. Drain hardware, then drop the ring. */
    while ((USART_GetFlagStatus(HT_UART0, USART_FLAG_RXDR) != RESET) &&
           (drained < (UART0_BUF_SIZE + 16))) {
        UART0_msg_feed((u8)USART_ReceiveData(HT_UART0));
        drained++;
    }
    if (USART_GetFlagStatus(HT_UART0, USART_FLAG_OE) != RESET) {
        USART_ClearFlag(HT_UART0, USART_FLAG_OE);
    }
    memset(rxd_comm0.buffer, 0, UART0_BUF_SIZE);
    rxd_comm0.read_pt  = 0;
    rxd_comm0.write_pt = 0;
    rxd_comm0.cnt      = 0;
    gsm_data_received  = false;
    NVIC_EnableIRQ(UART0_IRQn);
}

bool sendToGSMAndPrintResponse(HT_USART_TypeDef *thisUsart, const char* data, const char* expectedResponse) {
	int gsmChar;
	char responseBuffer[GSM_BUFFER_SIZE];
	int bufferIndex = 0;
	int timeout = 30000; 
	int responseFound = 0;
	uint8_t ch;

	if (gsm_at_exchange_busy) {
		return false;
	}
	
	while (*data) {
			int guard = 10000;
			while (UART0_getBufferSpace() == 0) {
				if (--guard <= 0) { break; }
			}
        ch = (uint8_t)(*data & 0xFF);
        UART0_tx_data(&ch, 1); 
        data++;
    }

		int guard2 = 10000;
		while (UART0_getBufferSpace() == 0) {
			if (--guard2 <= 0) { break; }
		}
    UART0_tx_data((u8*)"\r\n", 2);
    delay(); 
    while (timeout--) {
        gsmChar = UART0_getchar(); 
        if (gsmChar != -1) {  
            if (bufferIndex < GSM_BUFFER_SIZE - 1) {
                responseBuffer[bufferIndex++] = (char)gsmChar;
                responseBuffer[bufferIndex] = '\0'; 
            }           
            if (expectedResponse && strstr(responseBuffer, expectedResponse)) {
                responseFound = 1;
                break; 
            }
        } else {
            break;  
        }
    }

    if (!responseFound) {
    }

    while (UART0_getchar() != -1);
		
		return (bool)responseFound;
}

void sendToGSM(const char* data){
	   if (gsm_at_exchange_busy) {
		   return;
	   }
	   while (*data) {
			 uint8_t ch;
				// bounded wait for buffer space
				int guard = 10000;
				while (UART0_getBufferSpace() == 0) {
					if (--guard <= 0) { break; }
				}
        ch = (uint8_t)(*data & 0xFF);
        UART0_tx_data(&ch, 1); 
        data++;
    }
		
		// bounded wait for buffer space
		int guard2 = 10000;
		while (UART0_getBufferSpace() == 0) {
			if (--guard2 <= 0) { break; }
		}

    UART0_tx_data((u8*)"\r\n", 2);
		delay();
}



void delay(void){
	extern vu32 bftm0_ct;
	uint32_t start = bftm0_ct;
	while ((bftm0_ct - start) < 42)
		__NOP();
}

uint32_t parse_file_attribute(void) {
	  uint16_t i;
    uint8_t parsing;
    uint32_t attrValue;
    uint16_t write_pos;
    char current_char;

    parsing = 0;
    attrValue = 0;
    write_pos = rxd_comm0.write_pt;

    for (i = 0; i < write_pos; i++) {
        current_char = rxd_comm0.buffer[i];

        if (!parsing && i < write_pos - 1) {
            if (current_char == ':' && rxd_comm0.buffer[i+1] == ' ') {
                parsing = 1;
                i++;
                continue;
            }
        }
        if (parsing) {
            if (current_char >= '0' && current_char <= '9') {
                attrValue = attrValue * 10 + (current_char - '0');
            }
            else if (i < write_pos - 1 && 
                    current_char == 'O' && 
                    rxd_comm0.buffer[i+1] == 'K') {
								return attrValue;		                
            }
        }
    }
    return attrValue;
}

void cleanAndFormatIntelHexBuffer(void) {
    uint8_t *src = rxd_comm0.buffer;
    uint8_t *dst = IAP_Buffer;
    int lineStarted = 0;
    memset(IAP_Buffer, 0x00, GSM_BUFFER_SIZE);
    while (*src != '\0' && (dst - IAP_Buffer) < GSM_BUFFER_SIZE - 2) {
        if (*src == '\r' || *src == '\n' || *src == '>') {
            src++;
            continue;
        }

        if (*src == 'O' && *(src + 1) == 'K') {
            src += 2;
            continue;
        }

        if (*src == ':') {
            if ((dst - IAP_Buffer) < GSM_BUFFER_SIZE - 2) {
                *dst++ = '\n';     // Start line with newline
                *dst++ = *src++;   // Copy ':'
                lineStarted = 1;
            } else {
                break;
            }
            continue;
        }

        if (lineStarted && isxdigit(*src)) {
            *dst++ = *src++;
        } else {
            src++;
        }
    }
    if ((dst - IAP_Buffer) < GSM_BUFFER_SIZE) {
        *dst = '\0';
    }
}
