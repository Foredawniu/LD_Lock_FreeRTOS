#ifndef TIMER_H
#define TIMER_H


#include "FreeRTOSConfig.h"
#include "sys.h"


#if configGENERATE_RUN_TIME_STATS
void TIM3_Int_Init(u16 arr,u16 psc);

extern volatile unsigned long long FreeRTOSRunTimeTicks;
void ConfigureTimeForRunTimeStats(void);
#endif
#endif
