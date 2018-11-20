#include "v4l2.h"

int camera_fd=0;
int bufnum=0;
VideoBuffer pic;  //用以读取数据,每次read,pic会更新
VideoBuffer *buffers;  //用户空间缓存队列

int read_camera(pthread_mutex_t *mutex_video)
{
	struct v4l2_buffer buf;
	
	/* 读取数据 */
	memset(&buf,0,sizeof(buf));
	buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory=V4L2_MEMORY_MMAP;
	if (ioctl(camera_fd, VIDIOC_DQBUF, &buf) == -1)
	{
		perror("read data error:");
    	return -1;  //读取1帧数据
    }
    
    /* 处理数据 */
	if (pthread_mutex_lock(mutex_video)==0)  //返回0 成功 -1错误
	{
		pic.length=buf.bytesused;
		memcpy(pic.start,buffers[buf.index].start,pic.length);
	}
	else
	{
		printf("voc lock failed\n");
		pthread_exit(NULL);
	}
	pthread_mutex_unlock(mutex_video);

    

	/* 放回缓存 */
	if (ioctl(camera_fd, VIDIOC_QBUF, &buf) == -1)
	{
		perror("put buf error:");
    	return -1;  //解锁队列
    }
    
}

int init_camera(int bufNum)
{
	/* 打开设备 */
	camera_fd = open("/dev/video0", O_RDWR , 0);
	if(camera_fd < 0)
	{
		printf("open video device error!\n");
		return -1;
	}
	pic.start = (char *)malloc(sizeof(char)*101376);
	/* 显示设备信息 */
	struct v4l2_capability cap;
	ioctl(camera_fd,VIDIOC_QUERYCAP,&cap);
	printf("Driver Name:%s\nCard Name:%s\nBus info:%s\nDriver Version:%u.%u.%u\n", \
	cap.driver,cap.card,cap.bus_info,(cap.version>>16)&0xFF, (cap.version>>8)&0xFF,cap.version&0xFF);
	
	/* 显示支持格式 */
	struct v4l2_fmtdesc fmtdesc;
	fmtdesc.index=0;
	fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	printf("Supportformat:\n");
	while(ioctl(camera_fd,VIDIOC_ENUM_FMT,&fmtdesc)!=-1)
	{
		printf("\t%d.%s\n",fmtdesc.index+1,fmtdesc.description);
		fmtdesc.index++;
	}

	/* 显示当前格式 */
	struct v4l2_format fmt;
	memset ( &fmt, 0, sizeof(fmt) );
	fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(camera_fd,VIDIOC_G_FMT,&fmt);  //VIDIOC_G_FMT获取当前格式,VIDIOC_S_FMT设置格式,VIDIOC_TRY_FMT检查是否支持某格式
	printf("The old format information:\n\twidth:%d\n\theight:%d\n",fmt.fmt.pix.width,fmt.fmt.pix.height);
	
	/* 设置格式 */
	fmt.fmt.pix.width = 360; //640
	fmt.fmt.pix.height = 270;  //480
	//fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  //视频数据存储类型为YUYV
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;  //视频数据存储类型为MJPEG
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
	
	if (ioctl(camera_fd, VIDIOC_S_FMT, &fmt) == -1)
	{
		perror("VIDIOC_S_FMT");
  		return -1;
	}
	printf("The current format information:\n\twidth:%d\n\theight:%d\n",fmt.fmt.pix.width,fmt.fmt.pix.height);
	
	/* 申请缓冲区 */
	struct v4l2_requestbuffers req;
	req.count=bufNum;  //缓存数量为n,在缓存队列里保持n张照片
	req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;  //数据流类型,必须永远是V4L2_BUF_TYPE_VIDEO_CAPTURE
	req.memory=V4L2_MEMORY_MMAP;  //从系统空间到用户空间的数据交换方法是内存映射(memory mapping)
	                              //还有用户指针(V4L2_MEMORY_USERPTR)以及直接read/write的方法
	if (ioctl(camera_fd, VIDIOC_REQBUFS, &req) == -1)
	{
		printf("request buffer error!\n");
  		return -1;
  	}
  	bufnum=bufNum;
  	
  	/* 获取缓存地址 */
  	buffers = calloc( req.count, sizeof(*buffers) );
	if (!buffers) 
	{
		printf ("Out of memory/n");
		return -1;
	}
	
	/* 把缓存地址映射到用户空间 */
	struct v4l2_buffer buf;
	int numBufs;
	for(numBufs = 0; numBufs < req.count; numBufs++) 
	{
    	memset( &buf, 0, sizeof(buf) );
    	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    	buf.memory = V4L2_MEMORY_MMAP;
    	buf.index = numBufs;
    
    	if (ioctl(camera_fd, VIDIOC_QUERYBUF, &buf) == -1)
    	{
    		perror("QueryBuf error:"); 
        	return -1;  //读取缓存
        }

    	buffers[numBufs].length = buf.length;
    	buffers[numBufs].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, \
    								  MAP_SHARED,camera_fd, buf.m.offset);  // 转换成相对地址
    	if (buffers[numBufs].start == MAP_FAILED)
        	return -1;
        	
       	if (ioctl(camera_fd, VIDIOC_QBUF, &buf) == -1) 
       	{
       		perror("QBuf error:");
        	return -1;  //把缓冲帧放入队列
        }
	}
	
	/* 启动数据流 */
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl (camera_fd, VIDIOC_STREAMON, &type) < 0)
	{
		printf("VIDIOC_STREAMON error\n");
		return -1;
	}
    return 0;
}

int close_camera()
{
	free(pic.start);
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl (camera_fd, VIDIOC_STREAMOFF, &type) < 0)
	{
		printf("VIDIOC_STREAMOFF error\n");
		return -1;
	}
	int i;
	for(i=0;i<bufnum;i++)
	{
		if(munmap(buffers[i].start, buffers[i].length) == -1)
		{
			printf("munmap error! \n");
			return -1;
		}
	}

	close(camera_fd);
	return 0;
}
