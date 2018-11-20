#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/types.h> 
#include <pthread.h>
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>    
#include <arpa/inet.h> 
#include "v4l2.h"
#include "m0.h"
#include "voc.h"

#define SERVER_PORT 8888
#define BUFF_LEN 1027
#define CMD_PORT 8801
#define VIDEO_MANNAGER_PORT 8800
#define LIVE_TIME 50
typedef struct videolist           //保存clinet的video socket信息及状态
{
	struct sockaddr_in video_addr;   //clinet的video socket
	pthread_t          video_thread;
	int                port;
	int	               live;		//clinet的存活时间以发送的帧数记 10帧记一次    不进行线程同步竞争冒险不会有严重危害			
	struct videolist*  next;
}videolist;


pthread_mutex_t mutex_m0;
pthread_mutex_t mutex_voc;
pthread_mutex_t mutex_video;
char cmd = 'b';
char voc = 0;
char voc_buf[1400] ={0};

int  pic_write_lock=0;
int  pic_read_lock=0;

static void sleep_ms(unsigned int secs)
{
    struct timeval tval;
    tval.tv_sec=secs/1000;
    tval.tv_usec=(secs*1000)%1000000;
    select(0,NULL,NULL,NULL,&tval);
}

void *thread_camera()          //摄像头读取线程
{
	if(init_camera(2) == -1)
	{
		printf("init camera error!\n");
		pthread_exit(NULL);
	}
	int i = 0;
	while(1)
	{
		//锁住voc_buf 与voc标志位 
		if(read_camera(&mutex_video) == -1)
		{
			printf("read camera error!\n");
			pthread_exit(NULL);
		}
		i++;
		if(i%100 == 0)
			printf("i   :%d\n",i);
		sleep_ms(10);
	}
	
	if(close_camera() == -1)
	{
		printf("close camera error!\n");
		pthread_exit(NULL);
	}
	pthread_exit(NULL);
}



void *server_video(void *arg) //视频发送线程
{
	videolist *mythread = (videolist*) (arg);   //
	int thread_state = 0;
    int server_fd, ret;							//
    struct sockaddr_in ser_addr; 
	char videoready[10] = "ready";
    server_fd = socket(AF_INET, SOCK_DGRAM, 0); //AF_INET:IPV4;SOCK_DGRAM:UDP
    if(server_fd < 0)
    {
        printf("create socket fail!\n");
        pthread_exit(NULL);
    }

    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY); //IP地址，需要进行网络序转换，INADDR_ANY：本地地址
    ser_addr.sin_port = htons(mythread->port);    //端口号，需要网络序转换

    ret = bind(server_fd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
    if(ret < 0)
    {
        printf("socket bind fail!\n");
        pthread_exit(NULL);
    }
    
    unsigned char buf[BUFF_LEN];  //接收缓冲区，1025字节 第一个字节表示udp包在一帧中的位子。
    socklen_t len;
    int count;
    struct sockaddr_in clent_addr;  //clent_addr用于记录发送方的地址信息

    memset(buf, 0, BUFF_LEN);
    len = sizeof(clent_addr);
		printf("video port:%d\n",mythread->port);
		count = recvfrom(server_fd, buf, BUFF_LEN, 0, (struct sockaddr*)&clent_addr, &len);  //recvfrom是拥塞函数，没有数据就一直拥塞
        printf("client:%s\n",buf);  //打印client发过来的信息
		printf("server_fd:%d\n",server_fd);
        if(count==5&&strncmp(buf,"login",5)==0) //收到login开始发送视频数据
		{
			printf("videostart\n"); 
			if(-1==connect(server_fd, (struct sockaddr*)&clent_addr,sizeof(clent_addr)))
			{
				printf("connect error!\n");
				pthread_exit(NULL);
			}
			//printf("send %s %d",videoready,*(int *)(videoready+5));
			//write(server_fd,videoready,10);
			int i=0;
			int j=0;
			int n=0;
			VideoBuffer VBuffer;
			VBuffer.start = (char *)malloc(sizeof(char)*101376);
			while(1)
			{
				//锁住voc_buf 与voc标志位
				if (pthread_mutex_lock(&mutex_video)==0)  //返回0 成功 -1错误
				{
					VBuffer.length = pic.length;
					memcpy(VBuffer.start,pic.start,pic.length);
				}
				else
				{
					printf("voc lock failed\n");
					pthread_exit(NULL);
				}
				pthread_mutex_unlock(&mutex_video);
				//一帧大小为101376 需要分99个包 0～98
				n = VBuffer.length/1024;
				//if(VBuffer.length%1024!=0) n++;
				for(i=0;i<n;i++)
				{
					buf[0] = i;
					*(short int*)(buf+1) = VBuffer.length;
					memcpy(buf+3,VBuffer.start+i*1024,1024);
					write(server_fd,buf,BUFF_LEN);
				}
				int yu = VBuffer.length%1024;
				if(yu!=0)
				{ 
					buf[0] = i;
					*(short int*)(buf+1) = VBuffer.length;
					memcpy(buf+3,VBuffer.start+i*1024,yu);
					write(server_fd,buf,yu+3); 
				}
				if(j%50 == 0) printf("%d %d\n",j,VBuffer.length);
				if(++j%10 == 0)
				{
					if(--mythread->live <= 0)
					{
						if(thread_state==1)     //线程关闭
						{
							break;
						}
						thread_state =1;
					}
				}
				sleep_ms(5);
			}
		}
        close(server_fd);
        pthread_exit(NULL);
}


void *server_video_manager(void *arg) //视频线程管理
{
	int server_fd, ret;
    struct sockaddr_in ser_addr; 
	void *thread_ret = NULL;
    server_fd = socket(AF_INET, SOCK_DGRAM, 0); //AF_INET:IPV4;SOCK_DGRAM:UDP 
    if(server_fd < 0)							//socket是否创建成功
    {
        printf("create socket fail!\n");
        pthread_exit(NULL);
    }

    // 设置server_fd阻塞时间
    struct timeval timeout;
    timeout.tv_sec = 5;//秒
    timeout.tv_usec = 0;//微秒
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        printf("set server_fd timeout failed:");
    }

    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY); //IP地址，需要进行网络序转换，INADDR_ANY：本地地址
    ser_addr.sin_port = htons(VIDEO_MANNAGER_PORT); //端口号，需要网络序转换

    ret = bind(server_fd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
    if(ret < 0)
    {
        printf("socket bind fail!\n");
        pthread_exit(NULL);
    }
    
    unsigned char buf[100];  //接收缓冲区，200字节
    socklen_t len;
    int count;
	int i = 0;
    struct sockaddr_in clent_addr;  //clent_addr用于记录发送方的地址信息

	videolist* head = NULL;         //videolist的头
	videolist* ptr1,*ptr2,*temp;          

	char videofaild[] = "faild"; 
	char videoready[10] = "    ready"; //前4个空格用来传送port
	int  portcount = 8880;
	while(1)
	{
        memset(buf, 0, 100);
        len = sizeof(clent_addr);
        count = recvfrom(server_fd, buf, 100, 0, (struct sockaddr*)&clent_addr, &len);  //recvfrom是拥塞函数，已设置超时
        if(count == -1||count > 0) //清理超时线程
        {	ptr2 = NULL;
			ptr2 = ptr1 = head;
			while(ptr1)
			{
				if(ptr1->live <= 0)
				{
					if (!pthread_join(ptr1->video_thread, &thread_ret))		//结束该线程
						printf("video thread:port%d joined\n",ptr1->port);
					if(ptr1!=head)
					{
						ptr2->next = ptr1->next; 	//取下该结点
						free(ptr1);					//释放该结点
						ptr1 = ptr2->next;			
					}else{
						head = ptr1->next;
						free(ptr1);
						ptr1 = head;
					}	
					continue;						//已经移动到下一个节点，结束本次循环
				}
				ptr2 = ptr1;
				ptr1=ptr1->next;
			}
            printf("recv timeout flash videolisht!\n");
			if(count == -1)
				continue;
            //pthread_exit(NULL);
        }

        printf("%s:%dclient:%s %d\n",inet_ntoa(clent_addr.sin_addr),ntohs(clent_addr.sin_port),buf,i++);  //打印client socket信息与client发过来的信息
        if(count==5&&strncmp(buf,"video",5)==0) //收到video请求 开启一个新的video线程
		{ 
			printf("creating video thread!\n");
			ptr1 = head;
			while(ptr1)
			{
				ptr1 = head;
				if(++portcount>=65535) portcount = 8880;
				while(ptr1)
				{
					if(ptr1->port == portcount) //该port已经被使用，请重新分配
						break;
					ptr1=ptr1->next;
				}
			}
			ptr1 = (videolist*)malloc(sizeof(videolist));               	   //加入videolist        
			ptr1->video_addr = clent_addr;
			ptr1->live = LIVE_TIME;             //100帧的存活时间 10帧减一次 约等于秒
			ptr1->port = portcount;
			if(head == NULL) 
				ptr1->next = NULL;
			else
				ptr1->next = head;
			head = ptr1;
			if(0!=pthread_create(&ptr1->video_thread, NULL, server_video,(void*)ptr1))
			{
				printf("%s:%d create server_video faild:\n",inet_ntoa(clent_addr.sin_addr),ntohs(clent_addr.sin_port));  //线程创建失败信息
				sendto(server_fd, videofaild, sizeof(videofaild), 0, (struct sockaddr*)&clent_addr, len);     //发送线程创建失败信息给client，注意使用了clent_addr结构体指针
			}
			else
			{
				memcpy(videoready,&ptr1->port,4);
				printf("send %s %d\n",videoready+4,*(int *)videoready);
				sendto(server_fd, videoready, sizeof(videoready), 0, (struct sockaddr*)&ptr1->video_addr, sizeof(ptr1->video_addr));     //发送port给client
				printf("create video thread success!\n");
			}
		}
        else if(count==4&&strncmp(buf,"live",4)==0) //收到live video线程报活
		{ 
			ptr1 = head;
			while(ptr1)
			{
				if(ptr1->video_addr.sin_addr.s_addr==clent_addr.sin_addr.s_addr&&ptr1->video_addr.sin_port==clent_addr.sin_port&&ptr1->live>0) //该socket在videolist中无需开启新线程
					ptr1->live =LIVE_TIME;
				ptr1=ptr1->next;
			}
		}			
	}
	pthread_exit(NULL);
}


void *thread_M0(void *arg)   //M0 voc 驱动线程
{
	init_m0("/dev/ttyUSB0");
	init_voc("/dev/ttyUSB1");
	int op = 'b';
	int voc_flag = 0;
	int n =0;
	while(1)
	{	
		op = cmd;
		if(op != 'b') 
		{
			if(sendMsg_m0(op)==0)
			{
				printf("send error\n");
			}
			cmd = 'b';
		} 
		//锁住voc_buf 与voc标志位
		if (pthread_mutex_lock(&mutex_voc))  //返回0 成功 -1错误
		{
			printf("voc lock failed\n");
			pthread_exit(NULL);
		}		
		if(voc == 1)
		{
			send_data(voc_buf);
			voc = 0;
		}
		pthread_mutex_unlock(&mutex_voc);

		if(n++==50)
		{
			recvMsg_m0(&mutex_m0);//对m0解锁
			n=0;
		}	
		sleep_ms(50);
	}
	close_m0();
	pthread_exit(NULL);
}

void *server_cmd(void *arg)  //请求应答线程
{
	int server_fd, ret;
    struct sockaddr_in ser_addr; 

    server_fd = socket(AF_INET, SOCK_DGRAM, 0); //AF_INET:IPV4;SOCK_DGRAM:UDP
    if(server_fd < 0)
    {
        printf("create socket fail!\n");
        pthread_exit(NULL);
    }

    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY); //IP地址，需要进行网络序转换，INADDR_ANY：本地地址
    ser_addr.sin_port = htons(CMD_PORT);              //端口号，需要网络序转换

    ret = bind(server_fd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
    if(ret < 0)
    {
        printf("socket bind fail!\n");
        pthread_exit(NULL);
    }
    
    unsigned char buf[2048];  //接收缓冲区，2048字节
    socklen_t len;
    int count;
	int i = 0;
    struct sockaddr_in clent_addr;  //clent_addr用于记录发送方的地址信息
	while(1)
	{
        memset(buf, 0, 2048);
        len = sizeof(clent_addr);
        count = recvfrom(server_fd, buf, 2048, 0, (struct sockaddr*)&clent_addr, &len);  //recvfrom是拥塞函数，没有数据就一直拥塞
        if(count == -1)
        {
            printf("recieve data fail!\n");
            pthread_exit(NULL);
        }
        printf("client:%s %d\n",buf,i++);  //打印client发过来的信息
        if(count==3&&strncmp(buf,"get",3)==0) //收到get 开始回发m0数据
		{ 
			//锁住m0
			if (pthread_mutex_lock(&mutex_m0))  //返回0 成功 -1错误
			{
				printf("Thread_M0 lock failed\n");
				pthread_exit(NULL);
			}
			memcpy(buf,&data,sizeof(Data_m0));
			pthread_mutex_unlock(&mutex_m0);
        	sendto(server_fd, &buf, sizeof(Data_m0), 0, (struct sockaddr*)&clent_addr, len);  //发送信息给client，注意使用了clent_addr结构体指针
		}
        else if(count==4&&strncmp(buf,"cmd",3)==0) //收到cmd 执行命令
		{ 
			cmd = *(buf+3);
		}	
		else if(strncmp(buf,"els",3)==0) //收到els 开启语音
		{ 
			memset(voc_buf,0,1400);
			//锁住voc_buf 与voc标志位
			if (pthread_mutex_lock(&mutex_voc))  //返回0 成功 -1错误
			{
				printf("voc lock failed\n");
				pthread_exit(NULL);
			}
			memcpy(voc_buf,buf+3,count-3);
			voc = 1;							//语音标志置1
			pthread_mutex_unlock(&mutex_voc);
		}	
		memset(&clent_addr,0,sizeof(clent_addr));
	}
	pthread_exit(NULL);
}

int main() 
{
	int res; 
	void *thread_ret = NULL;
	pthread_t device_camera;// device_m0 is M0
	pthread_t device_m0;    // device_m0 is M0
	pthread_t server_main;  //server[0] is m0
	pthread_t server_video;

	//char m0[]="/dev/ttyUSB0";
	pthread_mutex_init(&mutex_m0, NULL); 
	pthread_mutex_init(&mutex_voc, NULL); 
	pthread_mutex_init(&mutex_video, NULL); 
	
	res = pthread_create(&device_camera, NULL, thread_camera,NULL);
	if(res!=0)
		puts("create thread_camera faild");
	//res = pthread_create(&device_m0, NULL, thread_M0,NULL);   //m0读取线程
	//if(res!=0)
	//	puts("create thread_M0 faild");
	//res = pthread_create(&server_main, NULL, server_cmd,NULL); //控制线程 port:8801
	//if(res!=0)
	//	puts("create server_cmd faild");	
	res = pthread_create(&server_video, NULL, server_video_manager,NULL);
	if(res!=0)
		puts("create server_camera faild");
		

	res = pthread_join(server_video, &thread_ret);
	if (!res)
		puts("Thread_net joined");
	return 0;
}
