#include "sepcur.h"
#include "usart.h"
#include "sys.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "string.h"


//------------------------------------
#define SEPCUR_MONITOR_PROMPT 0
//------------------------------------

extern QueueHandle_t que_usart4send;

TaskHandle_t CheckSepcurOperationTask_Handle = NULL;

static const u8 up_M[]={0x55,0x01,0x11,0x03,0x01,0XB8,0xC5}; //马达上行命令
static const u8 down_M[]={0x55,0x01,0x11,0x03,0x02,0xF8,0xC4}; //马达下行命令


void SepcurSendCmd(const u8* cmdstr, u8 len)
{
   usart4data_t usrsend4;
    u8 i = 0;
    
    usrsend4.len = len;
    
    memset(usrsend4.data, 0, sizeof(usrsend4.data));
    for(i=0; i<len; i++)
    {
        usrsend4.data[i] = cmdstr[i];
    }
    
    xQueueSend(que_usart4send, &usrsend4, 1000);
    
    #if SEPCUR_MONITOR_PROMPT
    taskENTER_CRITICAL();
    printf("sepcur cmd send success!\r\n");
    taskEXIT_CRITICAL();
    #endif
}

void CheckSepcurOperationTask( void *pvParameters )
{
    u32 notifyValue;
    
    for(;;)
    {
        xTaskNotifyWait(0x0, 0xffffffff, &notifyValue, portMAX_DELAY);
        switch(notifyValue)
        {
            case SEPCUR_UP:
                SepcurSendCmd(up_M, sizeof(up_M));
                break;
            case SEPCUR_DOWN:
                SepcurSendCmd(down_M, sizeof(down_M));
                break;
        }
    }
}

void Sepcur_Init(void)
{
    xTaskCreate(CheckSepcurOperationTask,   "CheckSepcurOperationTask", 150,    NULL, 6, &CheckSepcurOperationTask_Handle);
}
