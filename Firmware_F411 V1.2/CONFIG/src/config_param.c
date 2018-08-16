#include <stdbool.h>
#include <string.h>
#include "math.h"
#include "config.h"
#include "config_param.h"
#include "watchdog.h"
#include "stmflash.h"
#include "delay.h"

/*FreeRTOS���ͷ�ļ�*/
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

/********************************************************************************	 
 * ������ֻ��ѧϰʹ�ã�δ���������ɣ��������������κ���;
 * ALIENTEK MiniFly
 * ���ò�����������	
 * ����ԭ��@ALIENTEK
 * ������̳:www.openedv.com
 * ��������:2018/6/22
 * �汾��V1.2
 * ��Ȩ���У�����ؾ���
 * Copyright(C) �������������ӿƼ����޹�˾ 2014-2024
 * All rights reserved
********************************************************************************/


#define VERSION 12	/*12 ��ʾV1.2*/

configParam_t configParam;

static configParam_t configParamDefault=
{
	.version = VERSION,		/*�����汾��*/

	.pidAngle=	/*�Ƕ�PID*/
	{	
		.roll=
		{
			.kp=8.0,
			.ki=0.0,
			.kd=0.0,
		},
		.pitch=
		{
			.kp=8.0,
			.ki=0.0,
			.kd=0.0,
		},
		.yaw=
		{
			.kp=10.0,
			.ki=0.0,
			.kd=0.35,
		},
	},	
	.pidRate=	/*���ٶ�PID*/
	{	
		.roll=
		{
			.kp=320.0,
			.ki=0.0,
			.kd=15.0,
		},
		.pitch=
		{
			.kp=320.0,
			.ki=0.0,
			.kd=15.0,
		},
		.yaw=
		{
			.kp=120.0,
			.ki=18.5,
			.kd=0.0,
		},
	},	
	.pidPos=	/*λ��PID*/
	{	
		.vx=
		{
			.kp=12.5,
			.ki=0.0,
			.kd=1.2,
		},
		.vy=
		{
			.kp=12.5,
			.ki=0.0,
			.kd=1.2,
		},
		.vz=
		{
			.kp=100.0,
			.ki=150.0,
			.kd=10.0,
		},
		
		.x=
		{
			.kp=1.0,
			.ki=0.0,
			.kd=0.0,
		},
		.y=
		{
			.kp=1.0,
			.ki=0.0,
			.kd=0.0,
		},
		.z=
		{
			.kp=6.0,
			.ki=0.0,
			.kd=4.5,
		},
	},
	.trimP = 0.f,	/*pitch΢��*/
	.trimR = 0.f,	/*roll΢��*/
	.thrustBase=36000,	/*�������Ż���ֵ*/
};

static u32 lenth = 0;
static bool isInit = false;
static bool isConfigParamOK = false;

static SemaphoreHandle_t  xSemaphore = NULL;


static u8 configParamCksum(configParam_t* data)
{
	int i;
	u8 cksum=0;	
	u8* c = (u8*)data;  	
	size_t len=sizeof(configParam_t);

	for (i=0; i<len; i++)
		cksum += *(c++);
	cksum-=data->cksum;
	
	return cksum;
}

void configParamInit(void)	/*�������ó�ʼ��*/
{
	if(isInit) return;
	
	lenth=sizeof(configParam);
	lenth=lenth/4+(lenth%4 ? 1:0);

	STMFLASH_Read(CONFIG_PARAM_ADDR, (u32 *)&configParam, lenth);
	
	if(configParam.version == VERSION)	/*�汾��ȷ*/
	{
		if(configParamCksum(&configParam) == configParam.cksum)	/*У����ȷ*/
		{
			printf("Version V%1.1f check [OK]\r\n", configParam.version / 10.0f);
			isConfigParamOK = true;
		} else
		{
			printf("Version check [FAIL]\r\n");
			isConfigParamOK = false;
		}
	}	
	else	/*�汾����*/
	{
		isConfigParamOK = false;
	}
	
	if(isConfigParamOK == false)	/*���ò�������д��Ĭ�ϲ���*/
	{
		memcpy((u8 *)&configParam, (u8 *)&configParamDefault, sizeof(configParam));
		configParam.cksum = configParamCksum(&configParam);				/*����У��ֵ*/
		STMFLASH_Write(CONFIG_PARAM_ADDR,(u32 *)&configParam, lenth);	/*д��stm32 flash*/
		isConfigParamOK=true;
	}	
	
	xSemaphore = xSemaphoreCreateBinary();
	
	isInit=true;
}

void configParamTask(void* param)
{
	u8 cksum = 0;
	
	while(1) 
	{	
		xSemaphoreTake(xSemaphore, portMAX_DELAY);
		cksum = configParamCksum(&configParam);		/*����У��*/
		
		if(configParam.cksum != cksum)	
		{
			configParam.cksum = cksum;	/*����У��*/
			watchdogInit(250);			/*����ʱ��Ƚϳ������Ź�ʱ�����ô�һЩ*/					
			STMFLASH_Write(CONFIG_PARAM_ADDR,(u32 *)&configParam, lenth);	/*д��stm32 flash*/
			watchdogInit(WATCHDOG_RESET_MS);		/*�������ÿ��Ź�*/
		}						
	}
}

bool configParamTest(void)
{
	return isInit;
}

void configParamGiveSemaphore(void)
{
	xSemaphoreGive(xSemaphore);		
}

void resetConfigParamPID(void)
{
	configParam.pidAngle = configParamDefault.pidAngle;
	configParam.pidRate = configParamDefault.pidRate;
	configParam.pidPos = configParamDefault.pidPos;
}

void saveConfigAndNotify(void)
{
	u8 cksum = configParamCksum(&configParam);		/*����У��*/
	if(configParam.cksum != cksum)	
	{
		configParam.cksum = cksum;	/*����У��*/				
		STMFLASH_Write(CONFIG_PARAM_ADDR,(u32 *)&configParam, lenth);	/*д��stm32 flash*/
	}
}