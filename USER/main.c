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

#define ADDR 0x15    //modbusУ��
#define DTU_NETDATA_RX_BUF (1024)


int temp,humi,temph,templ,humih,humil,PM2_5,CO2=450,TVOC=225;//�������������������
static uint32_t dtu_rxlen = 0;
static uint8_t dtu_rxbuf[DTU_NETDATA_RX_BUF];//����DTU��������
char dtbuf[50];//�ֻ�App���ջ�������
	u8 Command[8]={0x15,0x03,0x00,0x64,0x00,0x08,0x06,0xC7};//�����򴫸������͵Ĳ�ѯָ��
	u8 Rxbuf[32]; //���մ�������������
	u8 key;//����
	u8 mode=0;//���ȿ���ģʽ��0Ϊ�ֶ���1Ϊ�Զ�
	
	
	
	
	u8 m;
RingBuffer *p_uart3_rxbuf;

int main(void)
{
    int ret;//DTU״̬������
    uint32_t i,t,timeout = 0;
    uint8_t buf,num;//buf����������DTU����������
	
	RTC_TimeTypeDef RTC_TimeStruct;
	RTC_DateTypeDef RTC_DateStruct;
	u8 tbuf[40];
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//����ϵͳ�ж����ȼ�����2
	delay_init(168);  //��ʼ����ʱ����
	uart_init(115200);		//��ʼ�����ڲ�����Ϊ115200
	usmart_dev.init(84); 	//��ʼ��USMART
	LED_Init();					//��ʼ��LED 
	LCD_Init();					//LCD��ʼ��	
	My_RTC_Init();		 		//��ʼ��RTC

 	KEY_Init();					//������ʼ�� 
	RS485_Init(9600);		//��ʼ��RS485����2
	RTC_Set_WakeUp(RTC_WakeUpClock_CK_SPRE_16bits,0);		//����WAKE UP�ж�,1�����ж�һ��
		
 	FSMC_SRAM_Init();			//��ʼ���ⲿSRAM  	
	my_mem_init(SRAMIN);		//��ʼ���ڲ��ڴ��   
    p_uart3_rxbuf = RingBuffer_Malloc(1024);        /*���ڴ���з���1K���ڴ������3����DTU����*/  
    RS232_Init(115200);		//��ʼ��RS232����3
	
	LCD_Clear(GRAYBLUE);//������Ļ����ɫΪ����ɫ
	BACK_COLOR=GRAYBLUE;//�������ֱ���ɫΪ����ɫ
 	POINT_COLOR=RED;//��������Ϊ��ɫ 
	LCD_ShowString(30,5,240,32,24,"Fresh air system");//�·�ϵͳ
	POINT_COLOR=BLACK;//��������Ϊ��ɫ 
	LCD_ShowString(30,32,240,24,24,"Time:");
	LCD_ShowString(30,59,240,16,24,"Mode:");
	LCD_ShowString(90,64,240,16,16,"Manual   ");	
	LCD_ShowString(30,85,240,24,24,"Fan:");
	LCD_ShowString(80,85,240,24,24,"OFF");	
	LCD_ShowString(30,110,240,16,16,"Command:");		//��ʾ���͵Ĳ�ѯָ��
	for(i=0;i<8;i++)
	{
		LCD_ShowxNum(10+i*28,130,Command[i],3,12,0X80);	//
 	}
	
	LCD_ShowString(10,145,240,16,24,"Real-time Data:");	//���յ�������	
	
 	LCD_ShowString(10,170,240,24,24,"Temp:");//�����¶�
	LCD_ShowString(165,170,200,24,24,"'C");//��λ
	
	LCD_ShowString(10,200,240,24,24,"Humi:");//����ʪ��
	LCD_ShowString(165,200,200,24,24,"%RH");//��λ
	
	LCD_ShowString(10,230,240,24,24,"PM2.5:");//����PM2.5Ũ��
	LCD_ShowString(165,230,240,24,24,"ug/m3");//��λ
	
	LCD_ShowString(10,260,240,24,24,"E-CO2:");//����CO2Ũ��
	LCD_ShowString(165,260,240,24,24,"ppm");//��λ
	
	LCD_ShowString(10,290,240,24,24,"TVOC:");//����TVOCŨ��
	LCD_ShowString(165,290,240,24,24,"ppb");//��λ
	
    printf("Wait for Cat1 DTU to start, wait 10s.... \r\n");
    {
        while( timeout <= 10 )   /* �ȴ�Cat1 DTU��������Ҫ�ȴ�5-6s�������� */
        {   
            ret = dtu_config_init(DTU_WORKMODE_NET);    /*��ʼ��DTU��������*/
            if( ret == 0 )
                break;
            timeout++;
            delay_ms(1000);
        }
        while( timeout > 10 )   /* ��ʱ */
        {
            printf("**************************************************************************\r\n");
            printf("ATK-DTU Init Fail ...\r\n");
            printf("�밴�����²�����м��:\r\n");
            printf("1.ʹ�õ�����λ������������DTU�ܷ񵥶���������\r\n");
            printf("2.���DTU���ڲ�����STM32ͨѶ�Ĵ��ڲ����Ƿ�һ��\r\n");
            printf("3.���DTU��STM32���ڵĽ����Ƿ���ȷ\r\n");
            printf("4.���DTU�����Ƿ�������DTU�Ƽ�ʹ��12V/1A��Դ���磬��Ҫʹ��USB��5V��ģ�鹩�磡��\r\n");
            printf("**************************************************************************\r\n\r\n");
            delay_ms(3000);
        }
    }
    printf("Cat1 DTU Init Success \r\n");
    dtu_rxlen = 0;
    RingBuffer_Reset(p_uart3_rxbuf);
	
	
    while (1)
    {
		mode=0;//Ĭ���ֶ����Ʒ���
//**************************************����Ϊ�ֶ�ģʽ************************************************************//
		while(mode==0)
		{			
			LCD_ShowString(90,64,240,16,16,"Manual   ");
			
			t++;
		if((t%10)==0)	//ÿ100ms����һ����ʾ����
		{
			RTC_GetTime(RTC_Format_BIN,&RTC_TimeStruct);	
			RTC_GetDate(RTC_Format_BIN, &RTC_DateStruct);	
			sprintf((char*)tbuf,"20%02d.%02d.%02d %02d:%02d",RTC_DateStruct.RTC_Year,RTC_DateStruct.RTC_Month,RTC_DateStruct.RTC_Date,RTC_TimeStruct.RTC_Hours,RTC_TimeStruct.RTC_Minutes); 
			LCD_ShowString(90,39,240,24,16,tbuf);	

		}
			key = KEY_Scan(0);								   
			switch(key)
			{				 
				case KEY0_PRES:	//������°���0
					mode=1;//�Զ����Ʒ���
					key=5;				
					break;
				case KEY1_PRES:	//������°���1
					LED0=!LED0;//���״̬��ת
					if(LED0==1)LCD_ShowString(80,85,240,24,24,"OFF");
					if(LED0==0)LCD_ShowString(80,85,240,24,24,"ON ");
					break;
				case KEY2_PRES:	//������°���2 
					mode=2;//�˳�ϵͳ
					break;
				case WKUP_PRES:	//������°���3
					RS485_Send_Data(Command,8);//���Ͳ�ѯָ�� 
					sprintf((char *)dtbuf,"Temp:   %.1fC\nHumi:   %.1fRH\nPM2.5: %uug/m3\nCO2:    %u ppm\nTVOC:  %u ppb  ",(double)temp/10,(double)humi/10,PM2_5,CO2,TVOC);
					send_data_to_dtu((uint8_t *)dtbuf, strlen(dtbuf));
					num=0;
					break;
			}
	
			if(num==42)//������42����һ�β�ѯָ��
			{
				RS485_Send_Data(Command,8);
				num=0;
				sprintf((char *)dtbuf,"Temp:   %.1fC\nHumi:   %.1fRH\nPM2.5: %uug/m3\nCO2:    %u ppm\nTVOC:  %uppb  ",(double)temp/10,(double)humi/10,PM2_5,CO2,TVOC);
				send_data_to_dtu((uint8_t *)dtbuf, strlen(dtbuf));
			}
			RS485_Receive_Data(Rxbuf,&m);//�õ����������ص�����
			dispaly_data();
			num++;
			
			
			/*���յ�DTU���͹����ķ���������*/
        if (RingBuffer_Len(p_uart3_rxbuf) > 0)      //��ȡ��ǰFIFO���ж�����Ч����    
        {
            RingBuffer_Out(p_uart3_rxbuf, &buf, 1);//��FIFO��ȡ����
            dtu_rxbuf[dtu_rxlen] = buf;
			
			
			
			if(dtu_rxbuf[0]==0X30){LED0=1;LCD_ShowString(80,85,240,24,24,"OFF");}//��������0����ʾ�رշ���
			if(dtu_rxbuf[0]==0X31){LED0=0;LCD_ShowString(80,85,240,24,24,"ON ");}//��������1����ʾ��������
			if(dtu_rxbuf[0]==0X41){mode=1;}//������ĸA����ʾ׼���Զ�ģʽ
			if(dtu_rxbuf[0]==0X4F){mode=2;}//������ĸO����ʾ׼���˳�ϵͳ
			
			
			
                usart1_send_data(dtu_rxbuf, 1); /*���յ���DTU���������������ݣ�ת�������Դ���1���*/
                dtu_rxlen = 0;
        }
		LED1 = !LED1;
		delay_ms(100);
	}
		
	
//*************************����Ϊ�Զ�ģʽ**********************************************************//
		while(mode==1)	
		{
			LCD_ShowString(90,64,240,16,16,"Automatic");
			
		t++;
		if((t%10)==0)	//ÿ100ms����һ����ʾ����
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
			if(num==42)//������42����һ�β�ѯָ��
			{
				RS485_Send_Data(Command,8);//���1���ӷ���8���ֽڵ���ѯ֡ 
				num=0;
				sprintf((char *)dtbuf,"Temp:   %.1fC\nHumi:   %.1fRH\nPM2.5: %uug/m3\nCO2:    %u ppm\nTVOC:  %uppb  ",(double)temp/10,(double)humi/10,PM2_5,CO2,TVOC);
				send_data_to_dtu((uint8_t *)dtbuf, strlen(dtbuf));
			}
			RS485_Receive_Data(Rxbuf,&m);//�õ����������ص�����
			dispaly_data();
			num++;
			//delay_ms(10);
			
		key=KEY_Scan(0);	
		switch(key)
		{				 
			case KEY0_PRES:	//����Ϊ�Զ����Ʒ���
				mode=0;//�Զ����Ʒ���
				key=5;				
				break;
			case KEY1_PRES:	//�ֶ����Ʒ��ȵ�������ֹͣ

				break;
			case KEY2_PRES:	//����LED1��ת	 
				mode=2;
				break;
			case WKUP_PRES:	//�ֶ����Ͳ�ѯָ�� 
				RS485_Send_Data(Command,8);//���Ͳ�ѯָ�� 
				sprintf((char *)dtbuf,"Temp:   %.1fC\nHumi:   %.1fRH\nPM2.5: %uug/m3\nCO2:    %u ppm\nTVOC:  %u ppb  ",(double)temp/10,(double)humi/10,PM2_5,CO2,TVOC);
				send_data_to_dtu((uint8_t *)dtbuf, strlen(dtbuf));
				num=0;
				break;
		}			
		   if (RingBuffer_Len(p_uart3_rxbuf) > 0)      //��ȡ��ǰFIFO���ж�����Ч����    
			{
				RingBuffer_Out(p_uart3_rxbuf, &buf, 1);//��FIFO��ȡ����
				dtu_rxbuf[dtu_rxlen] = buf;
				
				
				
				if( dtu_rxbuf[0]==0X4D){mode=0;}//������ĸM����ʾ׼�������ֶ�ģʽ
				if( dtu_rxbuf[0]==0X4F){mode=2;}//������ĸO����ʾ׼���˳�ϵͳ
				
				
				
                usart1_send_data(dtu_rxbuf, 1); /*���յ���DTU���������������ݣ�ת�������Դ���1���*/
                dtu_rxlen = 0;
			}
			LED1 = !LED1;
			delay_ms(100);
		}
		
		
		
//**********************�˳��·�ϵͳ*********************************************************************//
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
		modbus_hundle(Rxbuf,m);//���ݴ����õ�ÿһ��������16����
		POINT_COLOR=RED;//��������Ϊ��ɫ 
		LCD_ShowxNum(100,170,temph,2,24,0X80);//��ʾ��ֵ���¶�����λ
		LCD_ShowString(130,170,240,24,24,".");//С����
		LCD_ShowxNum(145,170,templ,1,24,0X80);//�¶�С��λ
		
		LCD_ShowxNum(100,200,humih,2,24,0X80);//ʪ������λ�����ʪ�ȵ���70%�����ڱ�׼ʪ��
		LCD_ShowString(130,200,240,24,24,".");//С����
		LCD_ShowxNum(145,200,humil,1,24,0X80);//ʪ��С��λ
		
		LCD_ShowxNum(100,230,PM2_5,3,24,0X80);//��ʾPM2.5Ũ�ȣ�pm2.5��ֵС��35��˵�����������ǳ���
		LCD_ShowxNum(100,260, CO2, 4, 24,0X80);//��ʾCO2Ũ�ȣ���Χ��350��1000ppm��˵���������£�����˳��
		LCD_ShowxNum(100,290, TVOC, 3, 24,0X80);//TVOCŨ�ȣ�TVOC������0.6mg/m3
		/*TVOC �����λΪ ppb������ת��Ϊppm���뽫���ֵ����1000������ת��Ϊmg/m3���뽫���ֵ����1000�ٳ���1.79*/
}
