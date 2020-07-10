#ifndef USART_H
#define USART_H
#include "stdio.h"
#include "sys.h"

//#include "FreeRTOS.h"
//#include "task.h"

#define USART1_RCV_LEN   200
#define USART3_RCV_LEN   200

#define USART1_BOUNDRATE    115200UL
#define USART3_BOUNDRATE    115200UL
#define USART4_BOUNDRATE    9600

typedef struct
{
    u16 len;
    char data[200];
}usartdata_t;

typedef struct
{
    u16 len;
    u8 data[20];
}usart4data_t;


void Usart_Init(void);

#endif
