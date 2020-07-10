#ifndef KMP_H
#define KMP_H

#include "stm32f10x.h"
//½è¼øÀ´Ô´£ºhttps://blog.csdn.net/weixin_43904021/article/details/87792889
void getnext(char *p,int *next);
int kmp(char *s,char *p,int *next);

u8 MatchCmd_FromESP32(char* strings, char *cmd);
u8 MatchString_FromESP32(char* strings, char *cmd);
u8 MatchString(char* strings, char *cmd);

#endif
