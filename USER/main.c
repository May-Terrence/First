#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"  
#include "sram.h"   
#include "malloc.h" 
#include <stdio.h>
#include "string.h"
#include "ringbuffer.h"
#include "rs232.h"
#include "atk_m750.h"
#include "rs485.h"
#include "lcd.h"
#include "usmart.h"
#include "rtc.h"

#define ADDR 0x15    //modbus校验
#define DTU_NETDATA_RX_BUF (1024)


int temp,humi,temph,templ,humih,humil,PM2_5,CO2=450,TVOC=225;//定义空气质量参数变量
static uint32_t dtu_rxlen = 0;
static uint8_t dtu_rxbuf[DTU_NETDATA_RX_BUF];//接收DTU缓冲数组
char dtbuf[50];//手机App接收缓冲数组
	u8 Command[8]={0x15,0x03,0x00,0x64,0x00,0x08,0x06,0xC7};//设置向传感器发送的查询指令
	u8 Rxbuf[32]; //接收传感器缓冲数组
	u8 key;//按键
	u8 mode=0;//风扇控制模式，0为手动，1为自动
	
	
	
	
	u8 m;
RingBuffer *p_uart3_rxbuf;

int main(void)
{
    int ret;//DTU状态配置用
    uint32_t i,t,timeout = 0;
    uint8_t buf,num;//buf用来接收由DTU传来的数据
	
	RTC_TimeTypeDef RTC_TimeStruct;
	RTC_DateTypeDef RTC_DateStruct;
	u8 tbuf[40];
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置系统中断优先级分组2
	delay_init(168);  //初始化延时函数
	uart_init(115200);		//初始化串口波特率为115200
	usmart_dev.init(84); 	//初始化USMART
	LED_Init();					//初始化LED 
	LCD_Init();					//LCD初始化	
	My_RTC_Init();		 		//初始化RTC

 	KEY_Init();					//按键初始化 
	RS485_Init(9600);		//初始化RS485串口2
	RTC_Set_WakeUp(RTC_WakeUpClock_CK_SPRE_16bits,0);		//配置WAKE UP中断,1秒钟中断一次
		
 	FSMC_SRAM_Init();			//初始化外部SRAM  	
	my_mem_init(SRAMIN);		//初始化内部内存池   
    p_uart3_rxbuf = RingBuffer_Malloc(1024);        /*从内存池中分配1K的内存给串口3接收DTU数据*/  
    RS232_Init(115200);		//初始化RS232串口3
	
	LCD_Clear(GRAYBLUE);//设置屏幕背景色为灰蓝色
	BACK_COLOR=GRAYBLUE;//设置文字背景色为灰蓝色
 	POINT_COLOR=RED;//设置字体为红色 
	LCD_ShowString(30,5,240,32,24,"Fresh air system");//新风系统
	POINT_COLOR=BLACK;//设置字体为黑色 
	LCD_ShowString(30,32,240,24,24,"Time:");
	LCD_ShowString(30,59,240,16,24,"Mode:");
	LCD_ShowString(90,64,240,16,16,"Manual   ");	
	LCD_ShowString(30,85,240,24,24,"Fan:");
	LCD_ShowString(80,85,240,24,24,"OFF");	
	LCD_ShowString(30,110,240,16,16,"Command:");		//显示发送的查询指令
	for(i=0;i<8;i++)
	{
		LCD_ShowxNum(10+i*28,130,Command[i],3,12,0X80);	//
 	}
	
	LCD_ShowString(10,145,240,16,24,"Real-time Data:");	//接收到的数据	
	
 	LCD_ShowString(10,170,240,24,24,"Temp:");//室内温度
	LCD_ShowString(165,170,200,24,24,"'C");//单位
	
	LCD_ShowString(10,200,240,24,24,"Humi:");//室内湿度
	LCD_ShowString(165,200,200,24,24,"%RH");//单位
	
	LCD_ShowString(10,230,240,24,24,"PM2.5:");//室内PM2.5浓度
	LCD_ShowString(165,230,240,24,24,"ug/m3");//单位
	
	LCD_ShowString(10,260,240,24,24,"E-CO2:");//室内CO2浓度
	LCD_ShowString(165,260,240,24,24,"ppm");//单位
	
	LCD_ShowString(10,290,240,24,24,"TVOC:");//室内TVOC浓度
	LCD_ShowString(165,290,240,24,24,"ppb");//单位
	
    printf("Wait for Cat1 DTU to start, wait 10s.... \r\n");
    {
        while( timeout <= 10 )   /* 等待Cat1 DTU启动，需要等待5-6s才能启动 */
        {   
            ret = dtu_config_init(DTU_WORKMODE_NET);    /*初始化DTU工作参数*/
            if( ret == 0 )
                break;
            timeout++;
            delay_ms(1000);
        }
        while( timeout > 10 )   /* 超时 */
        {
            printf("**************************************************************************\r\n");
            printf("ATK-DTU Init Fail ...\r\n");
            printf("请按照以下步骤进行检查:\r\n");
            printf("1.使用电脑上位机配置软件检查DTU能否单独正常工作\r\n");
            printf("2.检查DTU串口参数与STM32通讯的串口参数是否一致\r\n");
            printf("3.检查DTU与STM32串口的接线是否正确\r\n");
            printf("4.检查DTU供电是否正常，DTU推荐使用12V/1A电源供电，不要使用USB的5V给模块供电！！\r\n");
            printf("**************************************************************************\r\n\r\n");
            delay_ms(3000);
        }
    }
    printf("Cat1 DTU Init Success \r\n");
    dtu_rxlen = 0;
    RingBuffer_Reset(p_uart3_rxbuf);
	
	
    while (1)
    {
		mode=0;//默认手动控制风扇
//**************************************以下为手动模式************************************************************//
		while(mode==0)
		{			
			LCD_ShowString(90,64,240,16,16,"Manual   ");
			
			t++;
		if((t%10)==0)	//每100ms更新一次显示数据
		{
			RTC_GetTime(RTC_Format_BIN,&RTC_TimeStruct);	
			RTC_GetDate(RTC_Format_BIN, &RTC_DateStruct);	
			sprintf((char*)tbuf,"20%02d.%02d.%02d %02d:%02d",RTC_DateStruct.RTC_Year,RTC_DateStruct.RTC_Month,RTC_DateStruct.RTC_Date,RTC_TimeStruct.RTC_Hours,RTC_TimeStruct.RTC_Minutes); 
			LCD_ShowString(90,39,240,24,16,tbuf);	

		}
			key = KEY_Scan(0);								   
			switch(key)
			{				 
				case KEY0_PRES:	//如果按下按键0
					mode=1;//自动控制风扇
					key=5;				
					break;
				case KEY1_PRES:	//如果按下按键1
					LED0=!LED0;//红灯状态翻转
					if(LED0==1)LCD_ShowString(80,85,240,24,24,"OFF");
					if(LED0==0)LCD_ShowString(80,85,240,24,24,"ON ");
					break;
				case KEY2_PRES:	//如果按下按键2 
					mode=2;//退出系统
					break;
				case WKUP_PRES:	//如果按下按键3
					RS485_Send_Data(Command,8);//发送查询指令 
					sprintf((char *)dtbuf,"Temp:   %.1fC\nHumi:   %.1fRH\nPM2.5: %uug/m3\nCO2:    %u ppm\nTVOC:  %u ppb  ",(double)temp/10,(double)humi/10,PM2_5,CO2,TVOC);
					send_data_to_dtu((uint8_t *)dtbuf, strlen(dtbuf));
					num=0;
					break;
			}
	
			if(num==42)//计数到42发送一次查询指令
			{
				RS485_Send_Data(Command,8);
				num=0;
				sprintf((char *)dtbuf,"Temp:   %.1fC\nHumi:   %.1fRH\nPM2.5: %uug/m3\nCO2:    %u ppm\nTVOC:  %uppb  ",(double)temp/10,(double)humi/10,PM2_5,CO2,TVOC);
				send_data_to_dtu((uint8_t *)dtbuf, strlen(dtbuf));
			}
			RS485_Receive_Data(Rxbuf,&m);//得到传感器返回的数据
			dispaly_data();
			num++;
			
			
			/*接收到DTU传送过来的服务器数据*/
        if (RingBuffer_Len(p_uart3_rxbuf) > 0)      //获取当前FIFO中有多少有效数据    
        {
            RingBuffer_Out(p_uart3_rxbuf, &buf, 1);//从FIFO中取数据
            dtu_rxbuf[dtu_rxlen] = buf;
			
			
			
			if(dtu_rxbuf[0]==0X30){LED0=1;LCD_ShowString(80,85,240,24,24,"OFF");}//发送数字0，表示关闭风扇
			if(dtu_rxbuf[0]==0X31){LED0=0;LCD_ShowString(80,85,240,24,24,"ON ");}//发送数字1，表示开启风扇
			if(dtu_rxbuf[0]==0X41){mode=1;}//发送字母A，表示准备自动模式
			if(dtu_rxbuf[0]==0X4F){mode=2;}//发送字母O，表示准备退出系统
			
			
			
                usart1_send_data(dtu_rxbuf, 1); /*接收到从DTU传过来的网络数据，转发到调试串口1输出*/
                dtu_rxlen = 0;
        }
		LED1 = !LED1;
		delay_ms(100);
	}
		
	
//*************************以下为自动模式**********************************************************//
		while(mode==1)	
		{
			LCD_ShowString(90,64,240,16,16,"Automatic");
			
		t++;
		if((t%10)==0)	//每100ms更新一次显示数据
		{
			RTC_GetTime(RTC_Format_BIN,&RTC_TimeStruct);	
			RTC_GetDate(RTC_Format_BIN, &RTC_DateStruct);	
			sprintf((char*)tbuf,"20%02d.%02d.%02d %02d:%02d",RTC_DateStruct.RTC_Year,RTC_DateStruct.RTC_Month,RTC_DateStruct.RTC_Date,RTC_TimeStruct.RTC_Hours,RTC_TimeStruct.RTC_Minutes); 
			LCD_ShowString(90,39,240,24,16,tbuf);	

		}
			if(humih>75)
			{
				LED0=0;
				LCD_ShowString(80,85,240,24,24,"ON ");
			}
			if(humih<70)
			{
				LED0=1;
				LCD_ShowString(80,85,240,24,24,"OFF");
			}
			if(num==42)//计数到42发送一次查询指令
			{
				RS485_Send_Data(Command,8);//间隔1分钟发送8个字节的问询帧 
				num=0;
				sprintf((char *)dtbuf,"Temp:   %.1fC\nHumi:   %.1fRH\nPM2.5: %uug/m3\nCO2:    %u ppm\nTVOC:  %uppb  ",(double)temp/10,(double)humi/10,PM2_5,CO2,TVOC);
				send_data_to_dtu((uint8_t *)dtbuf, strlen(dtbuf));
			}
			RS485_Receive_Data(Rxbuf,&m);//得到传感器返回的数据
			dispaly_data();
			num++;
			//delay_ms(10);
			
		key=KEY_Scan(0);	
		switch(key)
		{				 
			case KEY0_PRES:	//设置为自动控制风扇
				mode=0;//自动控制风扇
				key=5;				
				break;
			case KEY1_PRES:	//手动控制风扇的启动与停止

				break;
			case KEY2_PRES:	//控制LED1翻转	 
				mode=2;
				break;
			case WKUP_PRES:	//手动发送查询指令 
				RS485_Send_Data(Command,8);//发送查询指令 
				sprintf((char *)dtbuf,"Temp:   %.1fC\nHumi:   %.1fRH\nPM2.5: %uug/m3\nCO2:    %u ppm\nTVOC:  %u ppb  ",(double)temp/10,(double)humi/10,PM2_5,CO2,TVOC);
				send_data_to_dtu((uint8_t *)dtbuf, strlen(dtbuf));
				num=0;
				break;
		}			
		   if (RingBuffer_Len(p_uart3_rxbuf) > 0)      //获取当前FIFO中有多少有效数据    
			{
				RingBuffer_Out(p_uart3_rxbuf, &buf, 1);//从FIFO中取数据
				dtu_rxbuf[dtu_rxlen] = buf;
				
				
				
				if( dtu_rxbuf[0]==0X4D){mode=0;}//发送字母M，表示准备进入手动模式
				if( dtu_rxbuf[0]==0X4F){mode=2;}//发送字母O，表示准备退出系统
				
				
				
                usart1_send_data(dtu_rxbuf, 1); /*接收到从DTU传过来的网络数据，转发到调试串口1输出*/
                dtu_rxlen = 0;
			}
			LED1 = !LED1;
			delay_ms(100);
		}
		
		
		
//**********************退出新风系统*********************************************************************//
		if(mode==2){break;}
    }
	LED0=1;
	LCD_ShowString(80,85,240,24,24,"OFF");
	LCD_ShowString(90,64,240,16,16,"Exit     ");
	
	
}
void modbus_hundle(u8 *buf, u8 len)
{
	//unsigned int crch=0x87,crcl=0xE8;
	if(buf[0]!=ADDR)
	{
		return;
	}
	else if(buf[0]==ADDR)
	{
//		if((buf[len-2]!=crch)||(buf[len-1]!=crcl))
//		{
//			return;
//		}
		temp=((int)buf[3]<<8) +buf[4];
		humi=((int)buf[5]<<8) +buf[6];
		PM2_5=((int)buf[7]<<8) +buf[8];
		CO2=((int)buf[9]<<8) +buf[10];
		TVOC=((int)buf[11]<<8) +buf[12];
		
		temph=temp/10;
		templ=temp-temph*10;		
		humih=humi/10;
		humil=humi-humih*10;
	}
}
void dispaly_data()
{
		modbus_hundle(Rxbuf,m);//数据处理：得到每一个参数的16进制
		POINT_COLOR=RED;//设置字体为红色 
		LCD_ShowxNum(100,170,temph,2,24,0X80);//显示数值，温度整数位
		LCD_ShowString(130,170,240,24,24,".");//小数点
		LCD_ShowxNum(145,170,templ,1,24,0X80);//温度小数位
		
		LCD_ShowxNum(100,200,humih,2,24,0X80);//湿度整数位，相对湿度低于70%是室内标准湿度
		LCD_ShowString(130,200,240,24,24,".");//小数点
		LCD_ShowxNum(145,200,humil,1,24,0X80);//湿度小数位
		
		LCD_ShowxNum(100,230,PM2_5,3,24,0X80);//显示PM2.5浓度，pm2.5的值小于35，说明空气质量非常好
		LCD_ShowxNum(100,260, CO2, 4, 24,0X80);//显示CO2浓度，范围在350～1000ppm，说明空气清新，呼吸顺畅
		LCD_ShowxNum(100,290, TVOC, 3, 24,0X80);//TVOC浓度，TVOC国标是0.6mg/m3
		/*TVOC 输出单位为 ppb，如需转换为ppm，请将输出值除以1000；如需转换为mg/m3，请将输出值除以1000再乘以1.79*/
}
