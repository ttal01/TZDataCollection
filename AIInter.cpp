#include <linux/serial.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <QDebug>
#include <QtEndian>
#include <stdlib.h>
#include "PublicFun.h"

typedef union{
    float f;
    char c[4];
}FLOAT_CONV;

static float __ltobf1(float data)
{
    FLOAT_CONV d1, d2;

    d1.f = data;

    d2.c[0] = d1.c[3];
    d2.c[1] = d1.c[2];
    d2.c[2] = d1.c[1];
    d2.c[3] = d1.c[0];
    return d2.f;
}

int InitSocketAIConnect(int &sockfd)
{
    sockfd = 1;
    return 1;
}
//!注意fhBuf必须要大端字节序
int AIReadData(int type,int DevAdder,int devBeginAdder,int len,char *fhBuf,int sockfd)
{
    char buff[128];
    int i;
    char buf[8]="0";
	short nbuf[8];
	static float sw_num[8];

    if((devBeginAdder+len)>0&&(devBeginAdder+len)<=32)
    {
        for(i=0;i<len/4;i++)
        {
            sprintf(buff,"/sys/bus/iio/devices/iio:device0/in_voltage%d_raw",i);
            usleep(100000);
            int fd=-1;
            int ret, numLoops=5;

            do{
                fd=open(buff,O_RDONLY| O_NONBLOCK);
                if(fd < 0)
                {
                    MyWriteLog("ERROR: open in_voltage%d_raw error:%s\n", i, strerror(errno));
                    sleep(1);
                }
                else{
                        //printf("open fd = %d success. errno %d, %s\n", fd, errno, strerror(errno));
                        //printf("open %d\n", fd);
                }
            }while(fd<0 && errno == EAGAIN && numLoops--);
            if(fd < 0 )
                continue;

            numLoops=5;
            do{
                ret=read(fd,buf,32);
                if(ret < 0)
                {
                    
                    usleep(500000);
                }else{	
                    nbuf[i]=atoi(buf);
					sw_num[i]=(1.8*nbuf[i]*1000)/(4095*83.4);				
					sw_num[i]=__ltobf1(sw_num[i]);
                }
            }while(ret < 0 && errno == EAGAIN && numLoops--);
            if(numLoops == 0)
            {
                MyWriteLog("ERROR: read in_voltage%d_raw error:%s\n", i, strerror(errno));
            }

            numLoops=5;
            do{
                ret = close(fd);
                if(ret <0){
                    MyWriteLog("close in_voltage%d_raw error:%s\n", i, strerror(errno));
                    usleep(500000);
                }
            }while(ret < 0 && numLoops--);

        }
        memcpy(fhBuf,sw_num,len);
    }
    return len;
}
