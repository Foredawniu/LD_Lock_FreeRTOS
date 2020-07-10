#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "kmp.h"
#include "monitor.h"
#include "esp32.h"
#include "lockdrv.h"
#include "sepcur.h"
#include "iwdg.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

#include "string.h"


//extern TimerHandle_t   OpenDoorTimeoutTimer_Handle;
extern TimerHandle_t   CheckLockActionTimer_Handle;
//-------------------------------------------------

TaskHandle_t StartTask_Handle = NULL;
void StartTask( void *pvParameters )
{
    for(;;)
    {
//        xTimerStart(OpenDoorTimeoutTimer_Handle, portMAX_DELAY);//最好不要立即开启此软件定时器
        xTimerStart(CheckLockActionTimer_Handle, portMAX_DELAY);
        
        vTaskDelete(NULL);
    }
}

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    
	delay_init();
    
	Usart_Init();
    
    #if MONITOR_FUNCTION
    Led_Init();
    #endif
	
    #if MONITOR_FUNCTION
    Monitor_Init();
    #endif
    
    Esp32_Init();
    
    ElectricLock_Init();
    
    Sepcur_Init();
    
    xTaskCreate(StartTask,  "StartTask",    200,    NULL, configMAX_PRIORITIES, &StartTask_Handle);
    
    #if IWDG_FUNCTION
    Iwdg_Init(4, 3125);//看门狗5秒
    #endif
    
    vTaskStartScheduler();
}
