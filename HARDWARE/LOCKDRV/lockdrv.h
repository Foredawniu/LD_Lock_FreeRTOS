#ifndef __LOCKDRV_H
#define __LOCKDRV_H	 
#include "sys.h"


//�������Ͽ����ź�����
#define LOCKED 0    //�޿����ź�
#define UNLOCKED 1  //�п����ź�

#define CLOSED 0    //���Ŵ�
#define OPENED 1    //���Ŵ�
//------------------------------------------------
void ElecLock_Init(void);

void Unlock_Upper(void);
void Lock_Upper(void);

void CheckDoorOpen(void);
void CheckDoorClose(void);

u8 Get_DoorState(void);
u8 Get_LockState(void);

#define LOCK_LOCK   1
#define UNLOCK_LOCK 2
#define UPLOAD_LOCKSTATE    3

void ElectricLock_Init(void);

#endif
