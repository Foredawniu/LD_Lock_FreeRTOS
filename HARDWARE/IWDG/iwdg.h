#ifndef IWDG_H
#define IWDG_H

#include "sys.h"

#define IWDG_FUNCTION   1

#if IWDG_FUNCTION

void Iwdg_Init(u8 psc,u16 rlr) ;

#endif //IWDG_FUNCTION
#endif //IWDG_H
