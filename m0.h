#ifndef M0_H
#define M0_H
//相关的头文件  
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include"uart_device.h"

typedef struct data_m0
{
	unsigned char tag;		//tag:0xBB				
	unsigned char id;		//id:03
	unsigned char len;		//len:24
	unsigned char fill_1;
	unsigned char tmp[2];		//tmp[0] low tmp[1] hight 高位为整数，低位为小数 ！只有高位有数据
	unsigned char hum[2];		//hum[0] low hum[1] hight 高位为整数，低位为小数 ！只有高位有数据
			 char acc[3];		//X acc[0] y acc[1] z acc[2] 可为负数
	unsigned char fill_2;
	unsigned char adc_ch0[4];	//没有使用
	unsigned char adc_ch1[4];	//开启中
	unsigned char light[2];		//light_low = light[0] light_high = light[1]
	u_int16_t     light_num;	//转化后的关照强度
	unsigned char state_led;	//1开启状态 0关闭状态
	unsigned char state_fan;
	unsigned char state_buzz;
	unsigned char state_7led;
	unsigned char fill_3[8];
}Data_m0;
/******************************************************************* 
* 名称：			init_m0 
* 功能：			初始化m0连接的串口 
* 入口参数：		char *uartFileName		m0所连串口设备的文件路径 
										/dev/(ttyUSB0,ttyUSB1,ttyUSB2) 
* 出口参数：		正确返回为1，错误返回为0 
*******************************************************************/  
extern int init_m0(char *uartFileName);
/******************************************************************* 
* 名称：			close_m0
* 功能：			关闭m0链接的串口
* 入口参数：		
* 出口参数：		 
*******************************************************************/  
extern void  close_m0();
/******************************************************************* 
* 名称：			sendMsg_m0
* 功能：			向串口发送定义好的控制数据，通过op来选择不同的控制信号
* 入口参数：		int op 
* 出口参数：		正确返回为1，错误返回为0
*******************************************************************/  
extern int sendMsg_m0(int op);
/******************************************************************* 
* 名称：			recvMsg_m0
* 功能：			接受一条串口数据并将其写入Data_m0 data
* 入口参数：		
* 出口参数：		正确返回为1，错误返回为0
*******************************************************************/  
extern int recvMsg_m0(pthread_mutex_t *mutex_m0);

extern int m0_fd;           //mo串口文件描述符
extern Data_m0 data;        //存储M0状态信息

#define LED_OPEN 		sendMsg_m0('0')
#define LED_CLOSE 		sendMsg_m0('1')
#define BUZZ_OPEN 		sendMsg_m0('2')
#define BUZZ_CLOSE 		sendMsg_m0('3')
#define FAN_OPEN 		sendMsg_m0('4')
#define FAN_LOW 		sendMsg_m0('5')
#define LED_MID 		sendMsg_m0('6')
#define LED_HIGH 		sendMsg_m0('7')
#define FAN_CLOSE 		sendMsg_m0('8')
#define LED_DISPLAY_OPEN sendMsg_m0('9')
#define LED_DISPLAY_CLOSE sendMsg_m0('a')


#endif
