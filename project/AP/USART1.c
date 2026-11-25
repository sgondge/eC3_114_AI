#include "USART1.h"
#include "utils.h"

//-----------------------------------------------------------------------------
__ALIGN4 _USART1_STRUCT rxd_scomm1;
__ALIGN4 _USART1_STRUCT txd_scomm1;

//-----------------------------------------------------------------------------
void USART1_Configuration(void)
{
		//functionStatus = 129;
  USART_InitTypeDef USART_InitStruct;
  CKCU_PeripClockConfig_TypeDef CKCUClock = {{0}};
  CKCUClock.Bit.USART1   = 1;
  CKCUClock.Bit.AFIO     = 1;
  CKCUClock.Bit.PD      = 1;
  CKCU_PeripClockConfig(CKCUClock, ENABLE);

  GPIO_PullResistorConfig(HT_GPIOD, GPIO_PIN_5, GPIO_PR_UP);  //Turn on UxART Rx internal pull up resistor to prevent unknow state

  AFIO_GPxConfig(GPIO_PD, AFIO_PIN_4, AFIO_FUN_USART_UART);  // Config AFIO mode
  AFIO_GPxConfig(GPIO_PD, AFIO_PIN_5, AFIO_FUN_USART_UART);  // Config AFIO mode

  USART_InitStruct.USART_BaudRate = 115200;
  USART_InitStruct.USART_WordLength = USART_WORDLENGTH_8B;
  USART_InitStruct.USART_StopBits = USART_STOPBITS_1;
  USART_InitStruct.USART_Parity = USART_PARITY_NO;
  USART_InitStruct.USART_Mode = USART_MODE_NORMAL;
  USART_Init(HT_USART1, &USART_InitStruct);
  USART_RxCmd(HT_USART1, ENABLE);
  USART_TxCmd(HT_USART1, ENABLE);

  USART_RXTLConfig(HT_USART1, USART_RXTL_04);
  USART_TXTLConfig(HT_USART1, USART_TXTL_04);
  USART_IntConfig(HT_USART1, USART_INT_RXDR | USART_INT_TOUT, ENABLE);
  HT_USART1->TPR = 0x80 | 40;
  USART1_init_buffer();
 // NVIC_EnableIRQ(USART1_IRQn);

}

//-----------------------------------------------------------------------------
void USART1_init_buffer (void)
{	//functionStatus = 130;
  rxd_scomm1.read_pt = 0;
  rxd_scomm1.write_pt = 0;
  rxd_scomm1.cnt = 0;

  txd_scomm1.read_pt = 0;
  txd_scomm1.write_pt = 0;
  txd_scomm1.cnt = 0;
}
static uint8_t c;

//-----------------------------------------------------------------------------
void USART1_IRQHandler(void)
{	//functionStatus = 131;
  // Tx, move data from buffer to UART FIFO
  if ((HT_USART1->SR) & USART_FLAG_TXC)
  {
    if (!txd_scomm1.cnt)
    {
      USART_IntConfig(HT_USART1, USART_INT_TXC, DISABLE);
    }
    else
    {
      u16 i;
      for (i = 0; i < USART1_FIFO_LEN; i++)   //have FIFO?
      {
        USART_SendData(HT_USART1, txd_scomm1.buffer[txd_scomm1.read_pt]);
        txd_scomm1.read_pt = (txd_scomm1.read_pt + 1) % USART1_BUF_SIZE;
        txd_scomm1.cnt--;
        if (!txd_scomm1.cnt)
          break;
      }
    }
  }

  // Rx, move data from UART FIFO to buffer
//  if ((HT_USART1->SR) & USART_FLAG_RXDR)
  if ((HT_USART1->SR) & USART_FLAG_RXDNE)
  {
    while(USART_GetFIFOStatus(HT_USART1, USART_FIFO_RX))
    {
			c =  USART_ReceiveData(HT_USART1);
      rxd_scomm1.buffer[rxd_scomm1.write_pt] = c;
      rxd_scomm1.write_pt = (rxd_scomm1.write_pt + 1) % USART1_BUF_SIZE;
      rxd_scomm1.cnt++;
    }
  }
}

//-----------------------------------------------------------------------------
int USART1_getchar(void)
{
		//functionStatus = 132;
	uint8_t c; // really only need this for debug but optimiser means it probably wont make any difference
  if(!rxd_scomm1.cnt)
    return -1;

	NVIC_DisableIRQ(USART1_IRQn);
	rxd_scomm1.cnt--;  
	NVIC_EnableIRQ(USART1_IRQn);
	c = rxd_scomm1.buffer[rxd_scomm1.read_pt];
	rxd_scomm1.read_pt = (rxd_scomm1.read_pt + 1) % USART1_BUF_SIZE;
	return (int) c;
}

// get space in TX buffer
int USART1_getBufferSpace (void) {
		//functionStatus = 133;
	return (txd_scomm1.read_pt > txd_scomm1.write_pt)? txd_scomm1.write_pt - txd_scomm1.read_pt : USART1_BUF_SIZE - txd_scomm1.read_pt + txd_scomm1.write_pt;
}
//-----------------------------------------------------------------------------
void USART1_tx_data(u8 *pt, u8 len)
{	//functionStatus = 134;
  while(len--)
  {
    txd_scomm1.buffer[txd_scomm1.write_pt] = *pt++;
    txd_scomm1.write_pt = (txd_scomm1.write_pt + 1) % USART1_BUF_SIZE;
    NVIC_DisableIRQ(USART1_IRQn);
    txd_scomm1.cnt++;
    NVIC_EnableIRQ(USART1_IRQn);
  }

  if(txd_scomm1.cnt)
    USART_IntConfig(HT_USART1,  USART_INT_TXC , ENABLE);
}

