#include "monitor.h"

#include "FreeRTOS.h"
#include "task.h"


#if MONITOR_FUNCTION

TaskHandle_t MonitorTask_Handle;



void MonitorTask( void *pvParameters )
{
    char *pxTaskStatusArray;
    u8 notifyValue;
    u32 freemem;
    
    for(;;)
    {
        notifyValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if(notifyValue)
        {
            pxTaskStatusArray = pvPortMalloc( uxTaskGetNumberOfTasks() * sizeof( TaskStatus_t ) );
            vTaskList(pxTaskStatusArray);
            taskENTER_CRITICAL();
            printf("\r\n%s\r\n", pxTaskStatusArray);
            taskEXIT_CRITICAL();
            vPortFree(pxTaskStatusArray);
            taskENTER_CRITICAL();
            printf("\r\nMonitorTask stack high water mark: remain %d word\r\n", uxTaskGetStackHighWaterMark(NULL));
            taskEXIT_CRITICAL();
            freemem=xPortGetFreeHeapSize();
            taskENTER_CRITICAL();
            printf("freemem:%d\r\n",freemem);
            taskEXIT_CRITICAL();
        }
    }
}

void Monitor_Init(void)
{
    xTaskCreate(MonitorTask,            "MonitorTask",          100,     NULL, 3, &MonitorTask_Handle);
}
#endif
