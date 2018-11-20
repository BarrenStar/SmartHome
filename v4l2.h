#ifndef _V4L2_H_
#define _V4L2_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <pthread.h>

typedef struct VideoBuffer {
	//用以映射缓冲帧
    void   *start;
    unsigned int  length;
}VideoBuffer;

//int process_image(void *start,unsigned int length);
int read_camera(pthread_mutex_t *mutex_video);
int init_camera(int bufNum);
int close_camera();

extern int camera_fd;  //摄像头文件
extern VideoBuffer pic;  //用以读取数据,每次read,pic会更新

#endif
