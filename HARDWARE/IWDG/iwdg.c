#include "iwdg.h"
#include "usart.h"

#include "FreeRTOS.h"
#include "task.h"

#if IWDG_FUNCTION

#define IWDG_FEED_PROMPT    0

TaskHandle_t IwdgTask_Handle = NULL;

void IwdgTask( void *pvParameters )
{
    for(;;)
    {
        vTaskDelay(1000);
        IWDG_ReloadCounter();
        
        #if IWDG_FEED_PROMPT
        taskENTER_CRITICAL();
        printf("feed iwdg\r\n");
        taskEXIT_CRITICAL();
        #endif
    }
}

void Iwdg_Init(u8 psc,u16 rlr) 
{	
 	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
	
	IWDG_SetPrescaler(psc);
	
	IWDG_SetReload(rlr);
	
	IWDG_ReloadCounter();
	
	IWDG_Enable();
    
    xTaskCreate(IwdgTask,   "IwdgTask", 64, NULL, configMAX_PRIORITIES, &IwdgTask_Handle);
}

#endif
