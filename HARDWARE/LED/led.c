#include "led.h"

#include "FreeRTOS.h"
#include "timers.h"

#if LED_FUNCTION
TimerHandle_t   LedTimer_Handle;

static u16 ledflash_remain = 0;//led剩余需要闪烁的次数（实际上是剩余需要取反的次数）


void LedTimer(xTimerHandle xTimer)
{
    if(ledflash_remain>0)
    {
        LED0_REV;
        ledflash_remain--;
    }
    else
    {
        LED0_OFF;
        xTimerStop(LedTimer_Handle,portMAX_DELAY);
    }
}

void Led_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_SetBits(GPIOC,GPIO_Pin_13);
    
    LedTimer_Handle = xTimerCreate("LedTimer", 100, pdTRUE, (void*)1, LedTimer);
    
}

void LedFastFlash(u16 period, u16 usetime)
{
    if(usetime)
    {
        LED0 = LED_OFF;
        ledflash_remain = usetime*2;
        xTimerChangePeriod(LedTimer_Handle, period, portMAX_DELAY);
        xTimerStart(LedTimer_Handle,portMAX_DELAY);
    }
}
#endif
