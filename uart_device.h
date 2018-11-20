#ifndef UART_DEVICE_H
#define UART_DEVICE_H
//串口相关的头文件  
#include<stdio.h>      /*标准输入输出定义*/  
#include<stdlib.h>     /*标准函数库定义*/  
#include<unistd.h>     /*Unix 标准函数定义*/  
#include<sys/types.h>   
#include<sys/stat.h>     
#include<fcntl.h>      /*文件控制定义*/  
#include<termios.h>    /*PPSIX 终端控制定义*/  
#include<errno.h>      /*错误号定义*/  
#include<string.h>  
/******************************************************************* 
* 名称：			UART0_Open 
* 功能：			打开串口 
* 入口参数：		int *fd    :文件描述符     port :串口号(ttyUSB0,ttyUSB1,ttyUSB2) 
* 出口参数：		正确返回为1，错误返回为0 
*******************************************************************/  
extern int UART0_Open(int *fd,char* port);

/******************************************************************* 
* 名称：			UART0_Close 
* 功能：			关闭串口 
* 入口参数：		fd			:文件描述符     			
* 出口参数：		void 
*******************************************************************/   
extern void UART0_Close(int fd);

/******************************************************************* 
* 名称：         UART0_Init() 
* 功能：         串口初始化 
* 入口参数：      fd			文件描述符    
*               speed		串口速度 
*               flow_ctrl	数据流控制 
*               databits	数据位   取值为 7 或者8 
*               stopbits	停止位   取值为 1 或者2 
*               parity 		效验类型 取值为N,E,O,,S 
*                       
* 出口参数：      正确返回为1，错误返回为0 
*******************************************************************/ 
extern int UART0_Init(int fd,int speed,int flow_ctrl,int databits,int stopbits,char parity);

/******************************************************************* 
* 名称：			UART0_Recv 
* 功能：			接收串口数据 
* 入口参数：		fd			:文件描述符     
*				cv_buf		:接收串口中数据存入rcv_buf缓冲区中 
*				data_len    :一帧数据的长度 
* 出口参数：		正确返回为1，错误返回为0 
*******************************************************************/  
extern int UART0_Recv(int fd, char *rcv_buf,int data_len);

/******************************************************************** 
* 名称：			UART0_Send 
* 功能：			发送数据 
* 入口参数：		fd			:文件描述符     
* 				send_buf    :存放串口发送数据 
* 				data_len    :一帧数据的个数 
* 出口参数：		正确返回为1，错误返回为0 
*******************************************************************/  
extern int UART0_Send(int fd, char *send_buf,int data_len);
//宏定义
#define FALSE  -1  
#define TRUE   0  



#endif
