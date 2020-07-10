/*
1.�Զ��������ܣ����Ѿ����ڿ���WIFI��ϵͳ�״��ϵ�����ӵ�������ʱ����10�����ڣ����������WIFI��ʧ���ָֻ����30�������ӵ�������
2.����������������գ�Ȼ��֪ͨ����ģ���Ӧ����
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
#define SERVERONLINE_PROMPT 0//��⵽������������������Ƿ���Ҫ�����ʾ(��������������ʾ��
#define LOCKOPRATION_PROMPT 0//��⵽�������������Ĳ���������Ƿ���Ҫ�����ʾ

const u8 CHECKNETWORK_PERIOD = 10;//�೤ʱ����һ���������ӣ���λ��s
const u8 CHECK_ESP32EXIST_FAILTIMES = 10;
const u8 POWERON_WAIT_ESP32AUTORSTTIME = 5;//ϵͳ�ϵ��ȴ�ESP32��λ�ȶ�ʱ�䣬��λ��s

//------------------------------------------------------
typedef struct
{
    u8 cmd_id;//��ţ���Ӧ�����ţ�������ת
    char cmdstr[50];//�����ESP32���͵�����
    char keystr[20];//�ؼ�����һ���������ɺ���Ҫ�ȴ�ESP32�ظ������ݣ�������
    u8 sucnext;//�ɹ���������ɹ���ESP32Ӧ������ؼ���������һ��Ҫ��ת�ı��
    u8 failnext;//ʧ�ܣ�ͬ���ɹ���
    u16 pridelay;//ǰ�����ʹ�����ǰ����ʱ����λ��ms
    u16 lagdelay;//�󣺷��ʹ�����ҳɹ��õ�Ӧ������ʱ����λ��ms
    u16 faillagdelay;//ʧ����ʱ���������ʧ�ܻ���Ӧ���Ӧ����ȷ�����ʱ����λ��ms
    u16 resendtime;//��ʱ��һ���������ɺ���ESP32һֱ��Ӧ��ʱ����ȴ�ʱ�䣬��λ��ms
    u8 keystrexist;//�ؼ����������費��ҪESP32Ӧ��1����ҪӦ��0������ҪӦ��
}esp32localcmd_t;

esp32localcmd_t esp32locmdset[10];//�������������

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

const u8 CHECKNETWORK_FIRSTPROGRESS_NUMBER = 0; //ϵͳ�ϵ���״��������͵������ţ���������������ȷ��

void Esp32LocalCmdInit(void)
{
    //                  ���  ����                                              �ؼ���         ��ʱ  �ؼ�  �ɹ�  ʧ��  ǰ   ��       ʧ����ʱ
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
//�жϷ��������������Ƿ�����Ҫ����������������ȡ״̬�����ǣ���֪ͨ��Ӧ����
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

//������շ�����������������
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

//�յ�֪ͨ�󣬸������ESP32�������ı�������Ӧ�����ݣ�������ֵ֪ͨ����жϽ��յ��������������ĳ���ؼ�������������֪ͨCheckNetworkTask()����
//���ճɹ���ֵ֪ͨ��1
//����ʧ�ܣ�ֵ֪ͨ��0
//��ʱ��ֵ֪ͨ��0
void LocalCmdAnalyzeTask( void *pvParameters )
{
    u32 notifyValue;
    usartdata_t usrcv3;
    u32 timeout = 0;
    
    for(;;)
    {
        xTaskNotifyWait(0x0, 0xffffffff, &notifyValue, portMAX_DELAY);
        
        if(esp32locmdset[notifyValue].keystrexist)//����Ҫ�ȴ�ESP32Ӧ��ؼ���
        {
            timeout = 0;
            while(timeout<esp32locmdset[notifyValue].resendtime)
            {
                if( xQueueReceive( que_usart3rcv, &usrcv3, 1 ) == pdPASS )
                {
                    if(MatchString(usrcv3.data, esp32locmdset[notifyValue].keystr))
                    {
                        break;//���յ�����ȷ�Ĺؼ���������
                    }
                    ServerCmdAnalyze(usrcv3.data);
                    
                }
                timeout++;
            }
            if(timeout<esp32locmdset[notifyValue].resendtime)//���޳�ʱ,ֵ֪ͨ��1
            {
                timeout = 0;
                xTaskNotify(CheckNetworkTask_Handle, 1, eSetValueWithOverwrite);
                
            }
            else//��ʱ��ֵ֪ͨ��0
            {
                timeout = 0;
                xTaskNotify(CheckNetworkTask_Handle, 0, eSetValueWithOverwrite);
            }
        }
        else//����Ҫ�ȴ�ESP32Ӧ��ؼ�����ֵ֪ͨ��1
        {
            xTaskNotify(CheckNetworkTask_Handle, 1, eSetValueWithOverwrite);
        }
    }
}

//����Ҫ��ESP32���͵�����ŵ�USART3���Ͷ���
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

//����������������״��
//ϵͳ�ϵ�����ȼ��WIFI����״��
//�״������Ϸ��������ڷ�����һֱ���ߣ���ESP32���ӣ�������£�ÿ��10�������������һ���豸�ţ�������Ӧ��success������ΪESP32���������������
//�����ӷ�����������������Ͽ�������WIFI�Ͽ��������˳�͸��ģʽ���ٽ�������
//��������������ת�ο� esp32localcmd_t
void CheckNetworkTask( void *pvParameters )
{
    u32 notifyValue;
    
    /*�������ܵĵ�һ��*/
    //��Ҫ�������������ȷ��
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
            vTaskDelay(POWERON_WAIT_ESP32AUTORSTTIME*1000);//ESP32�Զ���λ��Լ��Ҫ4����ȶ�
        }
        
        
        if(esp32locmdset[index_progress].pridelay>0)//ǰ��ʱ
            vTaskDelay(esp32locmdset[index_progress].pridelay);
        
        Esp32SendCmd(esp32locmdset[index_progress].cmdstr);//��������
        xTaskNotify(LocalCmdAnalyzeTask_Handle, index_progress, eSetValueWithOverwrite);//֪ͨLocalCmdAnalyzeTask�ȴ�����
        xTaskNotifyWait(0x0, 0xffffffff, &notifyValue, portMAX_DELAY);//�ȴ�LocalCmdAnalyzeTask��֪ͨ
        
        //���е�����ʱ��������Ѿ��õ���Ӧ����Ӧ��
        if(notifyValue)//��ESP32��������ȷ�Ĺؼ���
        {
            if(index_progress==6)//����˲�������6,˵�����������������
            {
                vTaskResume(ServerCmdAnalyzeTask_Handle);//�ָ�ServerCmdAnalyzeTask���Խ��շ��������������������Ȳ�������
                
                
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
            if(esp32locmdset[original_indexprogress].sucnext!=0xff)//������0xff˵����һ����Ҫ��ת���������
                index_progress = esp32locmdset[original_indexprogress].sucnext;
            if(esp32locmdset[original_indexprogress].lagdelay>0)//���������õ���ȷӦ�����Ҫ��ʱ
            {
                vTaskDelay(esp32locmdset[original_indexprogress].lagdelay);
                //���е������Ѿ�˵���Ѿ���ʱ����
                /*ע�⣺���������еı��6������ĺ���ʱΪ0������Ҫ�����if�ŵ�����*/
                if(original_indexprogress==6)//�����������6��˵���ָü�������������������״����
                {
                    vTaskSuspend(ServerCmdAnalyzeTask_Handle);//����ServerCmdAnalyzeTask
                }
            }
            if(original_indexprogress==9)
            {
                esp32_exist_ok = 1;
            }
        }
        else//����ͺ�û�еõ���ȷ��Ӧ��
        {
            #if SERVERONLINE_PROMPT
            taskENTER_CRITICAL();
            printf("failed: %d!\r\n", index_progress);
            taskEXIT_CRITICAL();
            #endif
            
            
            if( (index_progress==9) && (++esp32notexist>CHECK_ESP32EXIST_FAILTIMES) )//��ESP32ģ��һֱû�м�⵽
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
            if(esp32locmdset[original_indexprogress].failnext!=0xff)//��ʧ�ܺ���Ҫ��ת���������
                index_progress = esp32locmdset[original_indexprogress].failnext;
            if(esp32locmdset[original_indexprogress].faillagdelay>0)//��ʧ�ܺ���Ҫ��ʱ
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
    
    esp32_exist_ok = 1;//�ϵ�֮�����ESP32����
}


