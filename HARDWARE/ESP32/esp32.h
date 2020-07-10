#ifndef ESP32_H
#define ESP32_H
#include "stm32f10x.h"

void Esp32SendCmd(char* cmdstr);
void Esp32_Init(void);

#endif
