/*
ע�⣺��ElectricLock_Init()����Ҫ��OpenDoorTimeoutTimer_Handle�����ʱ���رգ�������ܣ��������·Ƿ����ż�⹦���쳣
*/

#include "lockdrv.h"
#include "usart.h"
#include "kmp.h"
#include "esp32.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "stm32f10x_usart.h"

#include "string.h"

//---------------------------------------------------------------------------
//���ų�ʱ����λ����
const u8 UNLOCK_TIMEOUT_VALUE = 10;
//---------------------------------------------------------------------------

//�ϲ�������ź�
#define PORT_UNLOCK_UPPER GPIOC   //�ϲ�������Ŷ˿�
#define CLOCKSOURCE_LOCK_UPPER RCC_APB2Periph_GPIOC   //�ϲ��������ʱ��Դ
#define PIN_UNLOCK_UPPER    GPIO_Pin_6 //�ϲ�������ź�

#define UNLOCK_VOTAGE 1 //�ߵ�ƽ����
#define LOCK_VOTAGE 0   //�͵�ƽ���������ź�
#define UNLOCK_LINE_UPPER PCout(6)//����������

#define UNLOCK_UPPER PCout(6)=1//�ϲ����
#define LOCK_UPPER PCout(6)=0//�ϲ�����
//--------------------------
//�²�������ź�
#define PORT_UNLOCK_LOWER GPIOC   //�ϲ�������Ŷ˿�
#define CLOCKSOURCE_LOCK_LOWER RCC_APB2Periph_GPIOC   //�ϲ��������ʱ��Դ
#define PIN_UNLOCK_LOWER    GPIO_Pin_7 //�ϲ�������ź�

#define UNLOCK_LOWER PCout(7)=1//�²����
#define LOCK_LOWER PCout(7)=0//�²�����

#define LOCK_STATE_UPPER PCin(4)//�Ŵ��ź�����
//-------------------------

TimerHandle_t   LcdrvStateUpdateTimer_Handle;
TimerHandle_t   OpenDoorTimeoutTimer_Handle;
TimerHandle_t   CheckLockActionTimer_Handle;
TaskHandle_t   LockOperationDetectTask_Handle;


void ElecLock_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(CLOCKSOURCE_LOCK_UPPER|RCC_APB2Periph_GPIOC, ENABLE);

	GPIO_InitStructure.GPIO_Pin = PIN_UNLOCK_UPPER;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(PORT_UNLOCK_UPPER, &GPIO_InitStructure);
	GPIO_ResetBits(PORT_UNLOCK_UPPER, PIN_UNLOCK_UPPER);
    
    GPIO_InitStructure.GPIO_Pin = PIN_UNLOCK_LOWER;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(PORT_UNLOCK_LOWER, &GPIO_InitStructure);
	GPIO_ResetBits(PORT_UNLOCK_LOWER, PIN_UNLOCK_LOWER);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

//---------------------------------------------------

void CapitalToLetter(char* str)
{
    u8 i=0;
    for(i=0; str[i]!='\0'; i++)
    {
        if(str[i]>='A'&&str[i]<='Z')
        {
            str[i]+=32;
        }
    }
}

static u8 door_state = CLOSED;//�ŵ�״̬
static u8 unlock_line_state = LOCKED;//�������Ƿ��п����ź�
static u8 door_opened_mark = 0;

static u8 illegalopendoor = 0;
static u8 unlockupper_mark = 0;

u8 Get_DoorState(void)
{
    return door_state;
}

u8 Get_LockState(void)
{
    return door_state;
}


//�ϲ�������
void Unlock_Upper(void)
{
    if(unlock_line_state==LOCKED&&door_state==CLOSED)
    {
        UNLOCK_UPPER;
        unlock_line_state = UNLOCKED;
        unlockupper_mark = 1;
    }
}

//�ϲ��������
void Lock_Upper(void)
{
    LOCK_UPPER;
    unlock_line_state = LOCKED;
    unlockupper_mark = 0;
}

//�������Ƿ��Ѵ�
void CheckDoorOpen(void)
{
    if(unlock_line_state==UNLOCKED&&door_state==OPENED)//������������н����źţ������Ŵ��źţ����Ѿ����򿪣�
    {
        LOCK_UPPER;//�����������ϵĽ����ź�
        Esp32SendCmd("door are opened\r\n");
        door_opened_mark = 1;
        xTimerStop(OpenDoorTimeoutTimer_Handle,portMAX_DELAY);
    }
    //-----------------------------------
    if(unlockupper_mark==0&&door_state==OPENED&&illegalopendoor==0)
    {
        illegalopendoor = 1;
        Esp32SendCmd("the door was opened illegally!!!\r\n");
    }
}



//��⿪���Ƿ�ʱ
void CheckDoorOpenTimeout(xTimerHandle xTimer)
{
    if(unlock_line_state==UNLOCKED&&door_state==CLOSED)//�����������н����źţ�����δ���򿪣����ų�ʱ��
    {
        LOCK_UPPER;//�����������ϵĽ����ź�
        door_opened_mark = 0;
        Esp32SendCmd("open the door timeout\r\n");
        Esp32SendCmd("door are closed\r\n");
        Esp32SendCmd("lcok cylinder state:locked\r\n");
        
        unlockupper_mark = 0;
    }
}

void CheckDoorClose(void)
{
    if(door_state==CLOSED&&door_opened_mark)
    {
        door_opened_mark = 0;
        Esp32SendCmd("door are closed\r\n");
        Esp32SendCmd("lcok cylinder state:locked\r\n");
        
        //---------------------------------------
        door_opened_mark = 0;
        unlockupper_mark = 0;
        illegalopendoor = 0;
    }
    //----------------------------------
    if(illegalopendoor&&door_state==CLOSED)
    {
        Esp32SendCmd("the door was closed illegally!!!\r\n");
        illegalopendoor = 0;
    }
}

//��ȡ���������ŵ�״̬
void Update_LockState(void)
{
    if(LOCK_STATE_UPPER==LOCKED)//������Ŵ��ź�
    {
        door_state = CLOSED;
    }
    else if(LOCK_STATE_UPPER==UNLOCKED)//������Ŵ��ź�
    {
        door_state = OPENED;
    }
    
    if(UNLOCK_LINE_UPPER==LOCK_VOTAGE)//������������޽����ź�
    {
        unlock_line_state = LOCKED;
    }
    if(UNLOCK_LINE_UPPER==UNLOCK_VOTAGE)//������������н����ź�
    {
        unlock_line_state = UNLOCKED;
    }
}

void SendLockState(void)
{
    if(Get_DoorState() == CLOSED)
    {
        Esp32SendCmd("door state:closed\r\n");
    }
    if(Get_DoorState() == OPENED)
    {
        Esp32SendCmd("door state:opened\r\n");
    }
    
    if(Get_LockState() == LOCKED)
    {
        Esp32SendCmd("lcok cylinder state:locked\r\n");
    }
    if(Get_LockState() == UNLOCKED)
    {
        Esp32SendCmd("lcok cylinder state:unlocked\r\n");
    }
}


void LockOperationDetectTask( void *pvParameters )
{
    u32 notifyValue;
    
    for(;;)
    {
        xTaskNotifyWait(0x0, 0xffffffff, &notifyValue, portMAX_DELAY);
        switch(notifyValue)
        {
            case UNLOCK_LOCK:
                Unlock_Upper();
                Esp32SendCmd("unlcok cylinder successed\r\n");
                xTimerStart(OpenDoorTimeoutTimer_Handle,portMAX_DELAY);
                break;
            case LOCK_LOCK:
                Lock_Upper();
                break;
            case UPLOAD_LOCKSTATE:
                SendLockState();
                break;
        }
    }
}

/*
ע�⣺Update_LockState()�����������ǰ����ã�CheckDoorOpen()�����ڶ����ã�CheckDoorClose()�����ã�������ܣ��������·Ƿ����ż�⹦���쳣
*/
void CheckLockAction(xTimerHandle xTimer)
{
    Update_LockState();
    CheckDoorOpen();
    CheckDoorClose();
}

void ElectricLock_Init(void)
{
    ElecLock_Init();
    
    xTaskCreate(LockOperationDetectTask,         "LockOperationDetectTask",       200,    NULL, 7, &LockOperationDetectTask_Handle);
    CheckLockActionTimer_Handle = xTimerCreate("CheckLockActionTimer", 100, pdTRUE, (void*)2, CheckLockAction);
    OpenDoorTimeoutTimer_Handle = xTimerCreate("OpenDoorTimeoutTimer", 10000, pdTRUE, (void*)2, CheckDoorOpenTimeout);
    xTimerStop(OpenDoorTimeoutTimer_Handle,portMAX_DELAY);
}
