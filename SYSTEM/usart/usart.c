#include "usart.h"
#include "sys.h"
#include "kmp.h"

#include "FreeRTOS.h" 	
#include "task.h"
#include "queue.h"

#include "stm32f10x_tim.h"

#include "string.h"


#if 1
#pragma import(__use_no_semihosting)             
//标准库需要的支持函数                 
struct __FILE 
{ 
	int handle; 

}; 

FILE __stdout;       
//定义_sys_exit()以避免使用半主机模式    
void _sys_exit(int x) 
{ 
	x = x; 
} 
//重定义fputc函数 
int fputc(int ch, FILE *f)
{      
	while((USART1->SR&0X40)==0);//循环发送,直到发送完毕   
    USART1->DR = (u8) ch;      
	return ch;
}
#endif 

QueueHandle_t que_usart1rcv = NULL;
QueueHandle_t que_usart1send = NULL;
QueueHandle_t que_usart3rcv = NULL;
QueueHandle_t que_usart3send = NULL;
QueueHandle_t que_usart4rcv = NULL;
QueueHandle_t que_usart4send = NULL;

TaskHandle_t Usart1SendTask_Handle = NULL;
TaskHandle_t Usart1RcvTask_Handle = NULL;
TaskHandle_t Usart3SendTask_Handle = NULL;
TaskHandle_t Usart3RcvTask_Handle = NULL;
TaskHandle_t Usart4SendTask_Handle = NULL;

static char  usart1_rcvbuf[USART1_RCV_LEN]; 
static volatile u8 usart1_rcv_start = 0;
static volatile u16 usart1_rcv_cnt = 0;
static volatile u16 usart1_rcv_cnt_last = 0;

static char  usart3_rcvbuf[USART3_RCV_LEN]; 
static volatile u8 usart3_rcv_start = 0;
static volatile u16 usart3_rcv_cnt = 0;
static volatile u16 usart3_rcv_cnt_last = 0;

//typedef struct
//{
//    u16 id;
//    char data[200];
//}usartrcv;

u8 usart4data[20];

void TIM2_Init(u16 arr,u16 psc)
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE); //时钟使能

	TIM_TimeBaseStructure.TIM_Period = arr; //设置在下一个更新事件装入活动的自动重装载寄存器周期的值	 计数到5000为500ms
	TIM_TimeBaseStructure.TIM_Prescaler =psc; //设置用来作为TIMx时钟频率除数的预分频值  10Khz的计数频率  
	TIM_TimeBaseStructure.TIM_ClockDivision = 0; //设置时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM向上计数模式
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure); //根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位
    
	TIM_ITConfig(TIM2,TIM_IT_Update,ENABLE ); //使能指定的TIM2中断,允许更新中断

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;  //TIM2中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;  //先占优先级0级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;  //从优先级3级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //IRQ通道被使能
	NVIC_Init(&NVIC_InitStructure);  //根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器

	TIM_Cmd(TIM2, ENABLE);  //使能TIMx外设						 
}

void Uart1_Init(u32 baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1|RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 4;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx|USART_Mode_Tx;
    
    USART_Init(USART1, &USART_InitStructure);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

void Uart3_Init(u32 baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 4;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx|USART_Mode_Tx;
    
    USART_Init(USART3, &USART_InitStructure);
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART3, ENABLE);
}

void Uart4_Init(u32 baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4,ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=4 ;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(UART4, &USART_InitStructure);

    USART_ClearFlag(UART4, USART_FLAG_TC);
    USART_Cmd(UART4, ENABLE);
    USART_ClearFlag(UART4, USART_FLAG_TC);
    USART_ITConfig(UART4,USART_IT_RXNE,ENABLE);
}

void TIM2_IRQHandler(void)
{
    usartdata_t rcv1;
    usartdata_t rcv3;
    BaseType_t xHigherPriorityTaskWoken;
    
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        if(usart1_rcv_start)
        {
            if(usart1_rcv_cnt==usart1_rcv_cnt_last)
            {
                usart1_rcv_start = 0;
                usart1_rcv_cnt_last = 0;
                
                if(que_usart1rcv!=NULL)
                {
                    rcv1.len = usart1_rcv_cnt;
                    strcpy(rcv1.data, usart1_rcvbuf);
                    xQueueSendFromISR(que_usart1rcv,&rcv1,&xHigherPriorityTaskWoken);
                    
                    memset(usart1_rcvbuf,0,USART1_RCV_LEN);
                }
            }
            usart1_rcv_cnt_last=usart1_rcv_cnt;
        }
        if(usart3_rcv_start)
        {
            if(usart3_rcv_cnt==usart3_rcv_cnt_last)
            {
                usart3_rcv_start = 0;
                usart3_rcv_cnt_last = 0;
                
                if(que_usart3rcv!=NULL)
                {
                    rcv3.len = usart3_rcv_cnt;
                    strcpy(rcv3.data, usart3_rcvbuf);
                    xQueueSendFromISR(que_usart3rcv,&rcv3,&xHigherPriorityTaskWoken);
                    
                    memset(usart3_rcvbuf,0,USART3_RCV_LEN);
                    
                }
            }
            usart3_rcv_cnt_last=usart3_rcv_cnt;
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    
}

void USART1_IRQHandler(void)
{
	u8 Res;
	
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		if(usart1_rcv_start==0)
        {
            usart1_rcv_start = 1;
            memset(usart1_rcvbuf, 0, USART1_RCV_LEN);
            usart1_rcv_cnt = 0;
        }
        Res=USART_ReceiveData(USART1);
        usart1_rcvbuf[usart1_rcv_cnt]=Res;
        
        if(usart1_rcv_cnt<USART1_RCV_LEN-1)
        {
            usart1_rcv_cnt++;
        }
     }
}

void USART3_IRQHandler(void)
{
	u8 Res;
	
	if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
	{
		if(usart3_rcv_start==0)
        {
            usart3_rcv_start = 1;
            memset(usart3_rcvbuf, 0, USART3_RCV_LEN);
            usart3_rcv_cnt = 0;
        }
        Res=USART_ReceiveData(USART3);
        usart3_rcvbuf[usart3_rcv_cnt]=Res;
        
        if(usart3_rcv_cnt<USART3_RCV_LEN-1)
        {
            usart3_rcv_cnt++;
        }
     }
}

void UART4_IRQHandler(void)
{

    u8 Res;
    if(USART_GetITStatus(UART4, USART_IT_RXNE) != RESET)
    {
        Res =USART_ReceiveData(UART4);
    
//        Get_M1[Get_M_COUNT]=Res;
//        
//        if(Get_M_COUNT<=49)
//        {
//            Get_M_COUNT++;
//        }
    }
} 

void Usart1_Send_Str(char* data, u16 len)
{
    u16 i=0;
    USART_ClearFlag(USART1, USART_FLAG_TC);
    for(i=0; i<len; i++)
    {
        USART_SendData(USART1, data[i]);
        while(USART_GetFlagStatus(USART1,USART_FLAG_TC)!=SET);
    }
}

void Usart3_Send_Str(char* data, u16 len)
{
    u16 i=0;
    USART_ClearFlag(USART3, USART_FLAG_TC);
    for(i=0; i<len; i++)
    {
        USART_SendData(USART3, data[i]);
        while(USART_GetFlagStatus(USART3,USART_FLAG_TC)!=SET);
    }
}

void Uart4_Send_Str(u8* data, u16 len)
{
    u16 i=0;
    
    USART_ClearFlag(UART4, USART_FLAG_TC);
    for(i=0; i<len; i++)
    {
        USART_SendData(UART4, data[i]);
        while(USART_GetFlagStatus(UART4,USART_FLAG_TC)!=SET);
    }
}

void CreateUsartQueues( void )
{
    que_usart1rcv = xQueueCreate( 1, sizeof( usartdata_t ) );//USART1接收队列
    que_usart1send = xQueueCreate( 1, sizeof( usartdata_t ) );//USART1发送队列
    
    que_usart3rcv = xQueueCreate( 1, sizeof( usartdata_t ) );//USART3接收队列
    que_usart3send = xQueueCreate( 1, sizeof( usartdata_t ) );//USART3发送队列

    que_usart4send = xQueueCreate( 1, sizeof( usart4data ) );//USART4发送队列

    if(que_usart1rcv==NULL || que_usart1send==NULL || que_usart3rcv==NULL  || que_usart3send==NULL || que_usart4send==NULL)
    {
        printf("create queue failed\r\n");
        while(1);
    }
}

void Usart1SendTask( void *pvParameters )
{
    usartdata_t usrsend1;
    
    for(;;)
    {
        if( xQueueReceive( que_usart1send, &usrsend1, portMAX_DELAY ) == pdPASS )
        {
            Usart1_Send_Str(usrsend1.data, strlen(usrsend1.data));
        }
    }
}

extern TaskHandle_t MonitorTask_Handle;
void Usart1RcvTask( void *pvParameters )
{
    usartdata_t usrcv1;
    
    for(;;)
    {
        if( xQueueReceive( que_usart1rcv, &usrcv1, portMAX_DELAY ) == pdPASS )//为什么等待时间不为0会出错？
        {
            if(MatchString(usrcv1.data, "monitor"))
            {
                xTaskNotifyGive(MonitorTask_Handle);
            }
        }
    }
}

void Usart3SendTask( void *pvParameters )
{
    usartdata_t usrsend3;
    for(;;)
    {
        if( xQueueReceive( que_usart3send, &usrsend3, portMAX_DELAY ) == pdPASS )
        {
            taskENTER_CRITICAL();
            Usart3_Send_Str(usrsend3.data, strlen(usrsend3.data));
            taskEXIT_CRITICAL();
        }
    }
}

void Usart4SendTask( void *pvParameters )
{
    usart4data_t usrsend4;
    for(;;)
    {
        if( xQueueReceive( que_usart4send, &usrsend4, portMAX_DELAY ) == pdPASS )
        {
            taskENTER_CRITICAL();
            Uart4_Send_Str(usrsend4.data, usrsend4.len);
            taskEXIT_CRITICAL();
        }
    }
}

void Usart3RcvTask( void *pvParameters )
{
//    usartdata_t usrcv3;
    
    for(;;)
    {
//        if( xQueueReceive( que_usart3rcv, &usrcv3, portMAX_DELAY ) == pdPASS )//为什么等待时间不为0会出错？
//        {
//            xQueueSend(que_usart1send, &usrcv3, 1000);
//        }
        vTaskDelay(1000);
    }
}

void Usart_Init(void)
{
    Uart1_Init(USART1_BOUNDRATE);
    Uart3_Init(USART3_BOUNDRATE);
    Uart4_Init(USART4_BOUNDRATE);
    TIM2_Init(200, 7200);
    CreateUsartQueues();
    
    xTaskCreate(Usart1SendTask, "Usart1SendTask",   200,    NULL, 4, &Usart1SendTask_Handle);
    xTaskCreate(Usart1RcvTask,  "Usart1RcvTask",    200,    NULL, 5, &Usart1RcvTask_Handle);
    xTaskCreate(Usart3SendTask, "Usart3SendTask",   200,    NULL, 8, &Usart3SendTask_Handle);
    xTaskCreate(Usart3RcvTask,  "Usart3RcvTask",    200,    NULL, 9, &Usart3RcvTask_Handle);
    xTaskCreate(Usart4SendTask, "Usart4SendTask",   200,    NULL, 8, &Usart4SendTask_Handle);
}
