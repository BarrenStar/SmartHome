#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <iconv.h>
#include "uart_device.h"

int voc_fd;//mo串口文件描述符

/******************************************************************* 
* 名称：			UTF8toGBK
* 功能：			把UTF-8格式的字符串转换为GBK
* 入口参数：		char *source         待转换的字符串
* 				char *destination    转换后的字符串
* 出口参数：		正确返回为0，错误返回为-1
*******************************************************************/ 
int UTF8toGBK(char *source,char *destination)
{
	char *encFrom = "UTF-8";
	char *encTo = "GBK";
	int length;
  	iconv_t cd = iconv_open (encTo, encFrom);
	if (cd == (iconv_t)-1)
	{
	    printf("iconv_open error!\n");
	    return -1;
	}
	length = strlen(source);
	memset(destination,0,length);
	//char *in = source;
	//char *out = destination;
	int maxlength = 4000;
	int ret = iconv (cd, &source, &length, &destination, &maxlength);
	if (ret == -1)
	{
	    printf("iconv error\n");
	}
	iconv_close (cd);
	return 0;
}

/******************************************************************* 
* 名称：			send_data
* 功能：			发送一个不超过4K的字符串
* 入口参数：		char *data     存放待发送的字符串
* 出口参数：		正确返回为0，错误返回为-1
*******************************************************************/ 
int send_data(char *data)
{
	char dest[4001];
	if(UTF8toGBK(data,dest)!=0)
	{
		printf("send_data: Tran_code error!\n");
		return -1;
	}
	
	unsigned char Frame_Info[4006];
	int data_length,length;
	data_length = strlen(dest);
	length = data_length + 2;  //包含命令字和文本编码格式
	
	Frame_Info[0] = 0xFD ; 			//构造帧头FD
	Frame_Info[1] = length / 256 ; 			//构造数据区长度的高字节
	Frame_Info[2] = length % 256 ; 		//构造数据区长度的低字节
	Frame_Info[3] = 0x01 ; 			//构造命令字：合成播放命令		 		 
	Frame_Info[4] = 0x01;       //文本编码格式：GBK 
	memcpy(&Frame_Info[5], dest, data_length);
	unsigned char state_check[4];
	state_check[0] = 0xFD;
	state_check[1] = 0x00;
	state_check[2] = 0x01;
	state_check[3] = 0x21;
	unsigned char state_ret[2];
	unsigned char ready[2];
	ready[0] = 0x4F;
	ready[1] = '\0';
	do{
		UART0_Send(voc_fd,state_check,4);
		UART0_Recv(voc_fd,state_ret,1);
		printf("%s",state_ret);
	}while(strcmp(state_ret,ready)!=0);
	if(UART0_Send(voc_fd,Frame_Info,data_length+5)==0)
	{
		printf("send_data: send error!\n");
		return -1;
	}
	return 0;
}

/******************************************************************* 
* 名称：			init_voc 
* 功能：			初始化voc连接的串口 
* 入口参数：		char *uartFileName		voc所连串口设备的文件路径 
										/dev/(ttyUSB0,ttyUSB1,ttyUSB2) 
* 出口参数：		正确返回为0，错误返回为-1 
*******************************************************************/  
int init_voc(char *uartFileName)
{
	if(UART0_Open(&voc_fd,uartFileName) == FALSE) //打开串口，返回文件描述符
		return -1;
	if(UART0_Init(voc_fd,9600,0,8,1,'N')==FALSE) //文件描述符fd 波特率9600 不用流控0 数据位8位 停止位1位 不用校验位
		return -1;
	printf("Set Voice Port Exactly!\n");  
	return 0;
}
