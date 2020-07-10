#ifndef __LOCKDRV_H
#define __LOCKDRV_H	 
#include "sys.h"


//开锁线上开锁信号有无
#define LOCKED 0    //无开锁信号
#define UNLOCKED 1  //有开锁信号

#define CLOSED 0    //无门磁
#define OPENED 1    //有门磁
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
