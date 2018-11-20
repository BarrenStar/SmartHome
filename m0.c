#include"m0.h"
#define UARTDATA_SIZE 36

int m0_fd;//mo串口文件描述符
int uart_state_m0 = 0;//串口状态 0已关闭 1已打开
Data_m0 data;  //存储M0状态信息

int showHET(char *buf,int len)
{
	int i = 0;
	for(i=0;i<len;i++)
	{
		printf("%02X ", buf[i]);
	}
	printf("\n");
	return 0;
}

int showMsg(Data_m0* data)
{
	printf("showMsg:");
	showHET((char*)data,36);
	printf("id:%d tmp:%d hum:%d light:%d\n",data->id,data->tmp[1],data->hum[1],data->light_num);
	printf("x:%d y:%d z:%d\n",data->acc[0],data->acc[1],data->acc[2]);
	printf("state_led:%d state_fan:%d state_buzz:%d state_7led:%d\n", 
			data->state_led,data->state_fan,data->state_buzz,data->state_7led);
	return 0;
}
/******************************************************************* 
* 名称：			recvMsg_m0
* 功能：			接受一条串口数据并将其写入Data_m0 data
* 入口参数：		 
* 出口参数：		正确返回为1，错误返回为0
*******************************************************************/
int recvMsg_m0(pthread_mutex_t *mutex_m0)
{
	//串口未打开返回FALSE
	if(uart_state_m0==0){
		printf("uart m0 is not opened\n");
		return FALSE;
	}
	int len = 0;
	int size =0;
	char * index=NULL;
	char recv_buf[50];
	char buf[36]={0};
	//if(buf_format == NULL){
	//	printf("recvMsg_m0:buf_format can't be NULL\n");
	//	return FALSE;
	//}
	Data_m0 *buf_format = &data;
	while(1)
	{
		size = UART0_Recv(m0_fd,recv_buf,sizeof(recv_buf));
		index = memchr(recv_buf,0xBB,size);
		if(index)
		{
			if((recv_buf+size)-index>=36)
			{
				memcpy(buf,index,36);
				printf("point1\n");
				//对m0加锁
				if (pthread_mutex_lock(mutex_m0))//返回0 成功 -1错误
				{
					printf("Thread_M0 lock failed\n");
					pthread_exit(NULL);
				}
				memcpy(buf_format,buf,sizeof(Data_m0));
				buf_format->light_num = buf_format->light[1];		//high
				buf_format->light_num = (buf_format->light_num<<8)+buf_format->light[0]; //low
				pthread_mutex_unlock(mutex_m0);
				return TRUE;
			}
			memcpy(buf,index,(recv_buf+size)-index);
			len +=recv_buf+size-index;
			break;
		}
	}
	while(len != UARTDATA_SIZE)
	{
		size = UART0_Recv(m0_fd,recv_buf,sizeof(recv_buf));
		if(len+size>=36)
		{
			memcpy(buf+len,recv_buf,36-len);
			printf("point2\n");
			//对m0加锁
			if (pthread_mutex_lock(mutex_m0))//返回0 成功 -1错误
			{
				printf("Thread_M0 lock failed\n");
				pthread_exit(NULL);
			}
			memcpy(buf_format,buf,sizeof(Data_m0));
			buf_format->light_num = buf_format->light[1];			//high
			buf_format->light_num = (buf_format->light_num<<8)+buf_format->light[0];//low
			pthread_mutex_unlock(mutex_m0);
			return TRUE;
		}
		memcpy(buf+len,recv_buf,size);
		len +=size;
	}
	printf("point3\n");
	memcpy(buf_format,buf,sizeof(Data_m0));
	buf_format->light_num = buf_format->light[1];					//high
	buf_format->light_num = (buf_format->light_num<<8)+buf_format->light[0];//low
	return TRUE;
}
/******************************************************************* 
* 名称：			sendMsg_m0
* 功能：			向串口发送定义好的控制数据，通过op来选择不同的控制信号
* 入口参数：		int op 
* 出口参数：		正确返回为1，错误返回为0
*******************************************************************/
int sendMsg_m0(int op)
{
	if(uart_state_m0==0){
		printf("uart m0 is not opened\n");
		return FALSE;
	}
	char send_buf[5]={0xDD,0x03,0x24,0x00,0x00};//
	switch(op)
 	{
 		case  '0':send_buf[4] = 0x00;break;	//开灯
 		case  '1':send_buf[4] = 0x01;break;	//关开
 		case  '2':send_buf[4] = 0x02;break;	//关蜂鸣器
 		case  '3':send_buf[4] = 0x03;break;	//开蜂鸣器
 		case  '4':send_buf[4] = 0x04;break;	//开风扇
 		case  '5':send_buf[4] = 0x05;break;	//低速
 		case  '6':send_buf[4] = 0x06;break;	//中速
 		case  '7':send_buf[4] = 0x07;break;	//高速
 		case  '8':send_buf[4] = 0x08;break;	//关风扇
 		case  '9':send_buf[4] = 0x09;break;	//开数码管
 		case  'a':send_buf[4] = 0x0a;break;	//关数码管
 		default:return FALSE;
 	}
	printf("send_ctn%d\n",send_buf[4]);
	return UART0_Send(m0_fd,send_buf,sizeof(send_buf));
}
/******************************************************************* 
* 名称：			init_m0 
* 功能：			初始化m0连接的串口 
* 入口参数：		char *uartFileName		m0所连串口设备的文件路径 
										/dev/(ttyUSB0,ttyUSB1,ttyUSB2) 
* 出口参数：		正确返回为1，错误返回为0 
*******************************************************************/  
int init_m0(char *uartFileName)
{
	if(UART0_Open(&m0_fd,uartFileName) == FALSE) //打开串口，返回文件描述符
		return FALSE;
	if(UART0_Init(m0_fd,115200,0,8,1,'N')==FALSE) //文件描述符m0_fd 波特率115200 不使用流控0 数据位8位 停止位1位 不使用奇偶校验‘N’  
		return FALSE;
	uart_state_m0 = 1;
	printf("Set Port Exactly!\n");  
	return TRUE;
}
/******************************************************************* 
* 名称：			close_m0
* 功能：			关闭m0链接的串口
* 入口参数：		
* 出口参数：		 
*******************************************************************/
void  close_m0()
{
	UART0_Close(m0_fd);
	uart_state_m0 = 0;
}

/*
int main(int argc, char *argv[])  
{  
         
    if(argc < 2)  
	{  
		printf("Usage: %s /dev/ttyUSB 0(send data)/1 (receive data) \n",argv[0]);  
		return FALSE;  
	} 
	init_m0(argv[1]);//
	Data_m0 data;	
	unsigned char op = 0;
	while((op=getchar())!='#')
	{	
		if(op=='\n') continue;
		sendMsg_m0(op);
		recvMsg_m0(&data);
		//showHET(buf,36);
		showMsg(&data);
	}
	LED_DISPLAY_OPEN;
	close_m0();
	return 1;
}  */ 
