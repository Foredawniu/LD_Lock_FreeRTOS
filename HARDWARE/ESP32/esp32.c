/*
1.自动重连功能：若已经存在可用WIFI，系统首次上电后连接到服务器时间在10秒以内，其余情况（WIFI消失后又恢复）最长30秒内连接到服务器
2.服务器操作命令接收，然后通知其余模块对应功能
*/

#include "esp32.h"
#include "led.h"
#include "kmp.h"
#include "lockdrv.h"
#include "sepcur.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "string.h"


extern QueueHandle_t que_usart1rcv;
extern QueueHandle_t que_usart3rcv;
extern QueueHandle_t que_usart1send;
extern QueueHandle_t que_usart3send;

//------------------------------------------------------
#define SERVERONLINE_PROMPT 0//检测到与服务器连接正常后是否需要输出提示(包括联网过程提示）
#define LOCKOPRATION_PROMPT 0//检测到服务器发过来的操作命令后是否需要输出提示

const u8 CHECKNETWORK_PERIOD = 10;//多长时间检测一次网络连接，单位：s
const u8 CHECK_ESP32EXIST_FAILTIMES = 10;
const u8 POWERON_WAIT_ESP32AUTORSTTIME = 5;//系统上电后等待ESP32复位稳定时间，单位：s

//------------------------------------------------------
typedef struct
{
    u8 cmd_id;//编号：对应命令编号，用于跳转
    char cmdstr[50];//命令：向ESP32发送的命令
    char keystr[20];//关键串：一个命令发送完成后，需要等待ESP32回复的内容（包含）
    u8 sucnext;//成功：此命令成功（ESP32应答包括关键串）后，下一步要跳转的编号
    u8 failnext;//失败：同“成功”
    u16 pridelay;//前：发送此命令前的延时，单位：ms
    u16 lagdelay;//后：发送此命令，且成功得到应答后的延时，单位：ms
    u16 faillagdelay;//失败延时：此命令发送失败或无应答或应答不正确后的延时，单位：ms
    u16 resendtime;//耗时：一个命令发送完成后，若ESP32一直无应答时的最长等待时间，单位：ms
    u8 keystrexist;//关键：此命令需不需要ESP32应答，1：需要应答，0：不需要应带
}esp32localcmd_t;

esp32localcmd_t esp32locmdset[10];//重连功能命令表

TaskHandle_t CheckNetworkTask_Handle;
TaskHandle_t LocalCmdAnalyzeTask_Handle;
TaskHandle_t ServerCmdAnalyzeTask_Handle;

volatile static u8 esp32_exist_ok = 1;
//---------------------------------------------------------------------------------------



void Esp32LocalCmdInitUnit( u8 index,
                            char* p_cmdstr,
                            char* keystr,
                            u16 resendtime,
                            u8 keystrexist,
                            u8 scnext,
                            u8 failnext,
                            u16 pridelay,
                            u16 lagdelay,
                            u16 faillagdelay )
{
    esp32locmdset[index].cmd_id = index;
    strcpy(esp32locmdset[index].cmdstr, p_cmdstr);
    if(keystr!=NULL)
        strcpy(esp32locmdset[index].keystr, keystr);
    esp32locmdset[index].resendtime = resendtime;
    esp32locmdset[index].keystrexist = keystrexist;
    esp32locmdset[index].sucnext = scnext;
    esp32locmdset[index].failnext = failnext;
    esp32locmdset[index].pridelay = pridelay;
    esp32locmdset[index].lagdelay = lagdelay;
    esp32locmdset[index].faillagdelay = faillagdelay;
}

const u8 CHECKNETWORK_FIRSTPROGRESS_NUMBER = 0; //系统上电后首次重连发送的命令编号，根据下面的命令表确定

void Esp32LocalCmdInit(void)
{
    //                  编号  命令                                              关键串         耗时  关键  成功  失败  前   后       失败延时
    Esp32LocalCmdInitUnit(0, "+++",                                             NULL,           0,      0,  8,   0xff,  0, 1000,    0);
    Esp32LocalCmdInitUnit(1, "AT+CWJAP?\r\n",                                   "nenglai",      3000,   1,  5,   4,     0, 1000,    5000);
    Esp32LocalCmdInitUnit(2, "AT+CIPMODE=1\r\n",                                NULL,           3000,   0,  3,   0xff,  0, 100,    100);
    Esp32LocalCmdInitUnit(3, "AT+CIPSEND\r\n",                                  NULL,           3000,   0,  6,   2,     0, 100,    100);
    Esp32LocalCmdInitUnit(4, "AT+CWJAP=\"nenglai\",\"12345678\"\r\n",           "NNECTED",      20000,  1,  1,   1,     0, 5000,   2000);
    Esp32LocalCmdInitUnit(5, "AT+CIPSTATUS\r\n",                                "STATUS:3",     3000,   1,  2,   7,     0, 100,      100);
    Esp32LocalCmdInitUnit(6, "U:12312312\r\n",                                  "success",      5000,   1,  6,   0,     0, CHECKNETWORK_PERIOD*1000,  1000);
    Esp32LocalCmdInitUnit(7, "AT+CIPSTART=\"TCP\",\"47.103.44.244\",6789\r\n",  "NNECT",        20000,  1,  5,   1,     0, 0,      2000);
    Esp32LocalCmdInitUnit(8, "+++\r\n",                                         NULL,           0,      0,  9,   0xff,  0, 1000,    0);
    Esp32LocalCmdInitUnit(9, "AT\r\n",                                           "OK",          1000,   1,  1,   9,     0, 1000,    10000);
}

extern TaskHandle_t   LockOperationDetectTask_Handle;
extern TaskHandle_t CheckSepcurOperationTask_Handle;
//判断服务器来的命令是否是需要操作锁、窗帘、获取状态，若是，则通知对应任务
void ServerCmdAnalyze(char* cmdstr)
{
    if(MatchString(cmdstr, "C_UNLOCK"))
    {
        xTaskNotify(LockOperationDetectTask_Handle, UNLOCK_LOCK, eSetValueWithOverwrite);
        
        #if LOCKOPRATION_PROMPT
        taskENTER_CRITICAL();
        printf("server cmd: unlock\r\n");
        taskEXIT_CRITICAL();
        #endif
    }
    if(MatchString(cmdstr, "C_LOCK"))
    {
        xTaskNotify(LockOperationDetectTask_Handle, LOCK_LOCK, eSetValueWithOverwrite);
        
        #if LOCKOPRATION_PROMPT
        taskENTER_CRITICAL();
        printf("server cmd: lock\r\n");
        taskEXIT_CRITICAL();
        #endif
    }
    if(MatchString(cmdstr, "C_START_EXAM"))
    {
        xTaskNotify(CheckSepcurOperationTask_Handle, SEPCUR_DOWN, eSetValueWithOverwrite);
        
        #if LOCKOPRATION_PROMPT
        taskENTER_CRITICAL();
        printf("server cmd: begin exam\r\n");
        taskEXIT_CRITICAL();
        #endif
    }
    if(MatchString(cmdstr, "C_END_EXAM"))
    {
        xTaskNotify(CheckSepcurOperationTask_Handle, SEPCUR_UP, eSetValueWithOverwrite);
        
        #if LOCKOPRATION_PROMPT
        taskENTER_CRITICAL();
        printf("server cmd: end exam\r\n");
        taskEXIT_CRITICAL();
        #endif
    }
    if(MatchString(cmdstr, "T_LOCKSTATE"))
    {
        xTaskNotify(LockOperationDetectTask_Handle, UPLOAD_LOCKSTATE, eSetValueWithOverwrite);
        
        #if LOCKOPRATION_PROMPT
        taskENTER_CRITICAL();
        printf("server cmd: get lock state\r\n");
        taskEXIT_CRITICAL();
        #endif
    }
    
}

//负责接收服务器来的命令数据
void ServerCmdAnalyzeTask( void *pvParameters )
{
    usartdata_t usrcv3;
    
    for(;;)
    {
        if( xQueueReceive( que_usart3rcv, &usrcv3, portMAX_DELAY ) == pdPASS )
        {
            ServerCmdAnalyze(usrcv3.data);
        }
    }
}

//收到通知后，负责接收ESP32发过来的本地命令应答数据，并根据通知值查表判断接收到的命令包不包含某个关键串，若包含则通知CheckNetworkTask()任务
//接收成功：通知值：1
//接收失败：通知值：0
//超时：通知值：0
void LocalCmdAnalyzeTask( void *pvParameters )
{
    u32 notifyValue;
    usartdata_t usrcv3;
    u32 timeout = 0;
    
    for(;;)
    {
        xTaskNotifyWait(0x0, 0xffffffff, &notifyValue, portMAX_DELAY);
        
        if(esp32locmdset[notifyValue].keystrexist)//若需要等待ESP32应答关键串
        {
            timeout = 0;
            while(timeout<esp32locmdset[notifyValue].resendtime)
            {
                if( xQueueReceive( que_usart3rcv, &usrcv3, 1 ) == pdPASS )
                {
                    if(MatchString(usrcv3.data, esp32locmdset[notifyValue].keystr))
                    {
                        break;//接收到了正确的关键串，跳出
                    }
                    ServerCmdAnalyze(usrcv3.data);
                    
                }
                timeout++;
            }
            if(timeout<esp32locmdset[notifyValue].resendtime)//若无超时,通知值：1
            {
                timeout = 0;
                xTaskNotify(CheckNetworkTask_Handle, 1, eSetValueWithOverwrite);
                
            }
            else//超时，通知值：0
            {
                timeout = 0;
                xTaskNotify(CheckNetworkTask_Handle, 0, eSetValueWithOverwrite);
            }
        }
        else//不需要等待ESP32应答关键串，通知值：1
        {
            xTaskNotify(CheckNetworkTask_Handle, 1, eSetValueWithOverwrite);
        }
    }
}

//将需要向ESP32发送的命令放到USART3发送队列
void Esp32SendCmd(char* cmdstr)
{
    usartdata_t usrsend3;
    
    if(esp32_exist_ok)
    {
        strcpy(usrsend3.data, cmdstr);
        usrsend3.len = strlen(cmdstr);
        
        xQueueSend(que_usart3send, &usrsend3, 1000);
    }
}

//检查与服务器的连接状况
//系统上电后首先检查WIFI连接状况
//首次连接上服务器后，在服务器一直在线（与ESP32连接）的情况下，每隔10秒向服务器发送一个设备号，服务器应答success后则认为ESP32与服务器连接正常
//若连接服务器后，又与服务器断开，或与WIFI断开，则先退出透传模式，再进行重连
//具体重连命令跳转参考 esp32localcmd_t
void CheckNetworkTask( void *pvParameters )
{
    u32 notifyValue;
    
    /*重连功能的第一步*/
    //需要根据重连命令表确定
    u8 index_progress = CHECKNETWORK_FIRSTPROGRESS_NUMBER;
    /*----------------*/
    u8 original_indexprogress;
    u8 firstcheck = 0;
    u8 esp32notexist = 0;
    
    for(;;)
    {
        if(firstcheck==0)
        {
            firstcheck = 1;
            vTaskDelay(POWERON_WAIT_ESP32AUTORSTTIME*1000);//ESP32自动复位大约需要4秒后稳定
        }
        
        
        if(esp32locmdset[index_progress].pridelay>0)//前延时
            vTaskDelay(esp32locmdset[index_progress].pridelay);
        
        Esp32SendCmd(esp32locmdset[index_progress].cmdstr);//发送命令
        xTaskNotify(LocalCmdAnalyzeTask_Handle, index_progress, eSetValueWithOverwrite);//通知LocalCmdAnalyzeTask等待命令
        xTaskNotifyWait(0x0, 0xffffffff, &notifyValue, portMAX_DELAY);//等待LocalCmdAnalyzeTask的通知
        
        //运行到这里时这个命令已经得到了应答或非应答
        if(notifyValue)//若ESP32返回了正确的关键串
        {
            if(index_progress==6)//如果此步骤编号是6,说明与服务器连接正常
            {
                vTaskResume(ServerCmdAnalyzeTask_Handle);//恢复ServerCmdAnalyzeTask，以接收服务器发来的锁、窗帘等操作命令
                
                
                #if SERVERONLINE_PROMPT
//                LedFastFlash(100, 2);
                taskENTER_CRITICAL();
                printf("server online!\r\n");
                taskEXIT_CRITICAL();
                #endif
            }
            
            
            #if SERVERONLINE_PROMPT
            taskENTER_CRITICAL();
            printf("succeed: %d!\r\n", index_progress);
            taskEXIT_CRITICAL();
            #endif
            
            
            original_indexprogress = index_progress;
            if(esp32locmdset[original_indexprogress].sucnext!=0xff)//若不是0xff说明下一步需要跳转到别的命令
                index_progress = esp32locmdset[original_indexprogress].sucnext;
            if(esp32locmdset[original_indexprogress].lagdelay>0)//若这个命令得到正确应答后需要延时
            {
                vTaskDelay(esp32locmdset[original_indexprogress].lagdelay);
                //运行到这里已经说明已经延时过了
                /*注意：如果命令表中的编号6的命令的后延时为0，则需要把这个if放到外面*/
                if(original_indexprogress==6)//如果命令编号是6，说明又该检查与服务器的网络连接状况了
                {
                    vTaskSuspend(ServerCmdAnalyzeTask_Handle);//悬起ServerCmdAnalyzeTask
                }
            }
            if(original_indexprogress==9)
            {
                esp32_exist_ok = 1;
            }
        }
        else//命令发送后，没有得到正确的应答
        {
            #if SERVERONLINE_PROMPT
            taskENTER_CRITICAL();
            printf("failed: %d!\r\n", index_progress);
            taskEXIT_CRITICAL();
            #endif
            
            
            if( (index_progress==9) && (++esp32notexist>CHECK_ESP32EXIST_FAILTIMES) )//若ESP32模块一直没有检测到
            {
                esp32_exist_ok = 0;
                
                
                #if SERVERONLINE_PROMPT
                taskENTER_CRITICAL();
                printf("ESP32 mudules do not exist!\r\n");
                taskEXIT_CRITICAL();
                #endif
                
                
                vTaskSuspend(CheckNetworkTask_Handle);
                vTaskSuspend(LocalCmdAnalyzeTask_Handle);
                vTaskSuspend(ServerCmdAnalyzeTask_Handle);
            }
            
            
            original_indexprogress = index_progress;
            if(esp32locmdset[original_indexprogress].failnext!=0xff)//若失败后需要跳转到别的命令
                index_progress = esp32locmdset[original_indexprogress].failnext;
            if(esp32locmdset[original_indexprogress].faillagdelay>0)//若失败后需要延时
                vTaskDelay(esp32locmdset[original_indexprogress].faillagdelay);
        }
    }
}

void Esp32_Init(void)
{
    Esp32LocalCmdInit();
    
    xTaskCreate(CheckNetworkTask,       "CheckNetworkTask",     150,    NULL, 9, &CheckNetworkTask_Handle);
    xTaskCreate(LocalCmdAnalyzeTask,    "LocalCmdAnalyzeTask",  180,    NULL, 10, &LocalCmdAnalyzeTask_Handle);
    xTaskCreate(ServerCmdAnalyzeTask,   "ServerCmdAnalyzeTask", 180,     NULL, 9, &ServerCmdAnalyzeTask_Handle);
    
    vTaskSuspend(ServerCmdAnalyzeTask_Handle);
    
    esp32_exist_ok = 1;//上电之后假设ESP32存在
}


