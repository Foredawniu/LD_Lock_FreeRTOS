#ifndef LED_H
#define LED_H	 

#include "sys.h"


#define LED_FUNCTION    1

#if LED_FUNCTION

#define LED_ON  0
#define LED_OFF 1

#define LED0        PCout(13)
#define LED0_ON     (LED0=LED_ON)
#define LED0_OFF    (LED0=LED_OFF)
#define LED0_REV    (LED0=!LED0)


void Led_Init(void);
void LedFastFlash(u16 period, u16 usetime);

#endif //LED_FUNCTION
#endif //LED_H
