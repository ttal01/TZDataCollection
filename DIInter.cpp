#include <linux/serial.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <QtEndian>
#include <QDebug>
#include "PublicFun.h"


int InitSocketDIConnect(int &sockfd)
{
    sockfd = 1;
    return 1;
}


int DIReadData(int type,int DevAdder,int devBeginAdder,int len,char *fhBuf,int sockfd)
{
	char buf[128];
	int fd,ret;
	int buf1[18]={110,111,112,113,2,3,8,9,10,11,60,75,87,89,114,115,116,117};
	char buf2[19];
	char *pt;
	
    char buf3[32];
    unsigned int diValue = 0;

    if((devBeginAdder+len)>=0&&(devBeginAdder+len)<=18)
    {
        for(int i=devBeginAdder;i<(devBeginAdder+len);i++)
        {
            sprintf(buf,"/sys/class/gpio/gpio%d/value",buf1[i]);
            fd = open (buf,O_RDONLY);
            usleep(200000);
            if(fd<0)
            {
                MyWriteLog("ERROR: open DI%d error:%s\n", i, strerror(errno));
                continue;
            }
            char rdChar;
            ret=read (fd,&rdChar,sizeof(char));
            if(sizeof(char) == ret && '1' == rdChar){
                diValue |= (1<<i);
            }
            if(ret<0)
            {
                MyWriteLog("ERROR: read DI%d error:%s\n", i, strerror(errno));
            }
            close(fd);
        }
        memcpy(fhBuf,&diValue,sizeof(diValue));

    }
    else
    {
        MyWriteLog("ERROR: DI read invalid: BeginAdder=%d len=%d\n",devBeginAdder,len);
    }

    return len;
}

