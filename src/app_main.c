/*
 * @File  app_main.c
 * @Brief An example of SDK's mini system
 * 
 * @Author: Neucrack 
 * @Date: 2017-11-11 16:45:17 
 * @Last Modified by: Neucrack
 * @Last Modified time: 2017-11-11 18:24:56
 */


#include "stdint.h"
#include "stdbool.h"
#include "api_os.h"
#include "api_event.h"
#include "api_debug.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

#include "api_sms.h"
#include "api_hal_uart.h"
#include "include/app_main.h"

#define AppMain_TASK_STACK_SIZE    (1024 * 2)
#define AppMain_TASK_PRIORITY      1 
HANDLE mainTaskHandle  = NULL;
HANDLE otherTaskHandle = NULL;

#define PHONE_NUMBER "0033673128710"
static uint8_t flag = 0;

void SMSInit()
{
    if(!SMS_SetFormat(SMS_FORMAT_TEXT,SIM0))
    {
        Trace(1,"sms set format error");
        return;
    }
    SMS_Parameter_t smsParam = {
        .fo = 17 ,
        .vp = 167,
        .pid= 0  ,
        .dcs= 8  ,//0:English 7bit, 4:English 8 bit, 8:Unicode 2 Bytes
    };
    if(!SMS_SetParameter(&smsParam,SIM0))
    {
        Trace(1,"sms set parameter error");
        return;
    }
    if(!SMS_SetNewMessageStorage(SMS_STORAGE_SIM_CARD))
    {
        Trace(1,"sms set message storage fail");
        return;
    }
}

void UartInit()
{
    UART_Config_t config = {
        .baudRate = UART_BAUD_RATE_115200,
        .dataBits = UART_DATA_BITS_8,
        .stopBits = UART_STOP_BITS_1,
        .parity   = UART_PARITY_NONE,
        .rxCallback = NULL,
    };
    UART_Init(UART1,config);
}

void Init()
{
    UartInit();
    SMSInit();
}

void SendSMS(uint8_t message[])
{
    uint8_t* unicode = NULL;
    uint32_t unicodeLen;

    Trace(1,"sms start send UTF-8 message");

    if(!SMS_LocalLanguage2Unicode(message,strlen(message),CHARSET_UTF_8,&unicode,&unicodeLen))
    {
        Trace(1,"local to unicode fail!");
        return;
    }
    if(!SMS_SendMessage(PHONE_NUMBER,unicode,unicodeLen,SIM0))
    {
        Trace(1,"sms send message fail");
    }
    OS_Free(unicode);
}

void messageRecieved(uint8_t* content)
{
    char buffer[200];
    snprintf(buffer, sizeof(buffer), "Message received : %s", content);
    Trace(1, buffer);
    SendSMS(buffer);
}

void ServerCenterTest()
{
    uint8_t addr[32];
    uint8_t temp;
    SMS_Server_Center_Info_t sca;
    sca.addr = addr;
    SMS_GetServerCenterInfo(&sca);
    Trace(1,"server center address:%s,type:%d",sca.addr,sca.addrType);
    temp = sca.addr[strlen(sca.addr)-1];
    sca.addr[strlen(sca.addr)-1] = '0';
    if(!SMS_SetServerCenterInfo(&sca))
        Trace(1,"SMS_SetServerCenterInfo fail");
    else
        Trace(1,"SMS_SetServerCenterInfo success");
    SMS_GetServerCenterInfo(&sca);
    Trace(1,"server center address:%s,type:%d",sca.addr,sca.addrType);
    sca.addr[strlen(sca.addr)-1] = temp;
    if(!SMS_SetServerCenterInfo(&sca))
        Trace(1,"SMS_SetServerCenterInfo fail");
    else
        Trace(1,"SMS_SetServerCenterInfo success");
}

void EventDispatch(API_Event_t* pEvent)
{
    
    switch(pEvent->id)
    {
        case API_EVENT_ID_NO_SIMCARD:
            Trace(10,"!!NO SIM CARD%d!!!!",pEvent->param1);
            break;
        case API_EVENT_ID_SYSTEM_READY:
            Trace(1,"system initialize complete");
            flag |= 1;
            break;
        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
            Trace(2,"network register success");
            flag |= 2;
            break;
        case API_EVENT_ID_SMS_SENT:
            Trace(2,"Send Message Success");
            break;
        case API_EVENT_ID_SMS_RECEIVED:
            Trace(2,"received message");
            //SMS_Encode_Type_t encodeType = pEvent->param1;
            uint32_t contentLength = pEvent->param2;
            uint8_t* header = pEvent->pParam1;
            uint8_t* content = pEvent->pParam2;

            Trace(2,"message header:%s",header);
            Trace(2,"message content length:%d",contentLength);
            messageRecieved(content);
            
            break;
        case API_EVENT_ID_SMS_LIST_MESSAGE:
        {
            SMS_Message_Info_t* messageInfo = (SMS_Message_Info_t*)pEvent->pParam1;
            Trace(1,"message header index:%d,status:%d,number type:%d,number:%s,time:\"%u/%02u/%02u,%02u:%02u:%02u+%02d\"", messageInfo->index, messageInfo->status,
                                                                                        messageInfo->phoneNumberType, messageInfo->phoneNumber,
                                                                                        messageInfo->time.year, messageInfo->time.month, messageInfo->time.day,
                                                                                        messageInfo->time.hour, messageInfo->time.minute, messageInfo->time.second,
                                                                                        messageInfo->time.timeZone);
            Trace(1,"message content len:%d,data:%s",messageInfo->dataLen,messageInfo->data);
            UART_Write(UART1, messageInfo->data, messageInfo->dataLen);//use serial tool that support GBK decode if have Chinese, eg: https://github.com/Neutree/COMTool
            UART_Write(UART1,"\r\n\r\n",4);
            //need to free data here
            OS_Free(messageInfo->data);
            break;
        }
        case API_EVENT_ID_SMS_ERROR:
            Trace(10,"SMS error occured! cause:%d",pEvent->param1);
        default:
            break;
    }

    //system initialize complete and network register complete, now can send message
    if(flag == 3)
    {
        SMS_Storage_Info_t storageInfo;
        SendSMS("Up and ready!");
        ServerCenterTest();
        SMS_GetStorageInfo(&storageInfo,SMS_STORAGE_SIM_CARD);
        Trace(1,"sms storage sim card info, used:%d,total:%d",storageInfo.used,storageInfo.total);
        SMS_GetStorageInfo(&storageInfo,SMS_STORAGE_FLASH);
        Trace(1,"sms storage flash info, used:%d,total:%d",storageInfo.used,storageInfo.total);
        if(!SMS_DeleteMessage(5,SMS_STATUS_ALL,SMS_STORAGE_SIM_CARD))
            Trace(1,"delete sms fail");
        else
            Trace(1,"delete sms success");
        SMS_ListMessageRequst(SMS_STATUS_ALL,SMS_STORAGE_SIM_CARD);
        flag = 0;
    }
}


void AppMainTask(void *pData)
{
    API_Event_t* event=NULL;

    Init(); 
    while(1)
    {
        if(OS_WaitEvent(mainTaskHandle, &event, OS_TIME_OUT_WAIT_FOREVER))
        {
            EventDispatch(event);
            OS_Free(event->pParam1);
            OS_Free(event->pParam2);
            OS_Free(event);
        }
    }
}

void app_Main(void)
{
    mainTaskHandle = OS_CreateTask(AppMainTask ,
        NULL, NULL, AppMain_TASK_STACK_SIZE, AppMain_TASK_PRIORITY, 0, 0, "init Task");
    OS_SetUserMainHandle(&mainTaskHandle);
}