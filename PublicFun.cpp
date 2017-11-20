#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>   //文件操作
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdarg.h>
#include <sys/reboot.h>
#include <qglobal.h>
#include <QDebug>
#include <QTextCodec>
#include <QFile>
#include <QTime>
#include <sys/time.h>
#include "PublicFun.h"
#include "log4qt/logger.h"
#include "log4qt/logmanager.h"
#include "log4qt/varia/listappender.h"
#include "log4qt/basicconfigurator.h"
#include "log4qt/propertyconfigurator.h"

int Log_fd=0;
extern int g_rmtLogLevel;//远程日志流log

Log4Qt::Logger * sysLog;
Log4Qt::ListAppender* mpLoggingEvents;


int getLogList(QString &logStr)
{
    int ret = 0;
    QList<Log4Qt::LoggingEvent> list = mpLoggingEvents->list();
    ret = list.count();
    if(ret)
    {
        //qDebug()<<"MSG:"<<list.first().message();
        logStr = list.first().message();
        mpLoggingEvents->removeFirst();
    }
    return ret;
}

//日志初始化
void LogFileInit(void)
{
     Log_fd=open("/media/mmcblk0p1/mainLog",O_RDWR|O_CREAT|O_APPEND);
     if(Log_fd==-1)
     {
         printf("file 'mainLog' open Error\r\n");
         Log_fd=0;
     }
     //Log4Qt::BasicConfigurator::configure();
     Log4Qt::PropertyConfigurator::configure("log.properties");
     sysLog = Log4Qt::LogManager::qtLogger();
     //!调试工具日志流先关闭，调试好再打开
//     mpLoggingEvents = new Log4Qt::ListAppender();
//     mpLoggingEvents->retain();
//     mpLoggingEvents->setName("Log4QtTest");
//     mpLoggingEvents->setConfiguratorList(true);
//     sysLog->addAppender(mpLoggingEvents);
}

//关闭日志文件
void CloseLogFile(void)
{
    close(Log_fd);
}

//写日志函数
void MyWriteLog(char* buf, ...)
{
    char sbuf[1024*6];
    memset(sbuf,0x00,1024*6);
    char buffer[1024*6];
    memset(buffer,0x00,1024*6);

    Q_ASSERT(buf!=NULL);

    va_list vp;
    va_start(vp, buf);
    vsnprintf(sbuf, sizeof(sbuf), buf, vp);
    va_end(vp);

    time_t now;
    struct tm *timenow;
    time(&now);
    timenow=localtime(&now);

    if(Log_fd>0)
    {
        if(sysLog)
        {
            sysLog->info(sbuf);
        }
        //qDebug()<<mpLoggingEvents->list().count();
        strftime (buffer,200,"[%F %T] ",timenow );
        strcat(buffer, sbuf);
        strcat(buffer, "\r\n");
        //write(Log_fd,buffer,strlen(buffer));
        //printf(buffer);

    }
    else{
        strftime ( buffer,200,"[%F %T] ",timenow );
        strcat(buffer, sbuf);
        strcat(buffer, "\r\n");
        printf(buffer);
    }
}

void MyWriteLog(logLevelType level, char* buf, ...)
{
    Q_ASSERT(buf!=NULL);

    char sbuf[1024*6];
    memset(sbuf,0x00,1024*6);
    char buffer[1024*6];
    memset(buffer,0x00,1024*6);

    va_list vp;
    va_start(vp, buf);
    vsnprintf(sbuf, sizeof(sbuf), buf, vp);
    va_end(vp);

    time_t now;
    struct tm *timenow;
    time(&now);
    timenow=localtime(&now);

    if(sysLog)
    {
        if(g_rmtLogLevel & level)
            sysLog->info(sbuf);
    }
    else
    {
        strftime (buffer,200,"[%F %T] ",timenow );
        strcat(buffer, sbuf);
        strcat(buffer, "\r\n");
        printf(buffer);
    }
}

void MyDebug(char* buf,...)
{
    va_list vp;
    Q_ASSERT(buf!=NULL);

    va_start(vp, buf);
    int result = vprintf(buf, vp);
    va_end(vp);
    printf("\r\n");
}
#if 0
void MyWriteLog(int fd,char* buf, ...)
{
    char sbuf[1024];
    memset(sbuf,0x00,1024);

    Q_ASSERT(buf!=NULL);

    va_list vp;
    va_start(vp, buf);
    vsnprintf(sbuf, sizeof(sbuf), buf, vp);
    va_end(vp);

    char buffer[1024];
    memset(buffer,0x00,1024);
    time_t now;
    struct tm *timenow;

    time(&now);
    timenow=localtime(&now);

    if(fd>0)
    {
        if(sysLog)
        {
            sysLog->info(sbuf);
        }
        strftime ( buffer,200,"[%F %T] ",timenow );
        strcat(buffer, sbuf);
        strcat(buffer, "\r\n");
        write(fd,buffer,strlen(buffer));
        //printf(buffer);
    }
    else
    {
        strftime (buffer,200,"[%F %T] ",timenow);
        strcat(buffer, sbuf);
        strcat(buffer, "\r\n");
        printf(buffer);
    }
}
#endif

long Get16Data2(unsigned char* data)
{
    long re=0;
    int i,j,m;
    if((data[0]&128)==128)
    {
        m=-1;
        if(data[1]>0)
        {
                i=data[0]^255;
                j=(data[1]-1)^255;
        }
        else
        {
                i=(data[0]-1)^255;
                j=0;
        }
    }
    else
    {
        m=1;
        i=data[0];
        j=data[1];
    }
    re=(i*256+j)*m;
    return re;
}
unsigned long UNGet16Data2(unsigned char* data)
{
    long re=0;
    int i,j,m;
    m=1;
    i=data[0];
    j=data[1];
    re=(i*256+j)*m;
    return re;
}
long Get16Data4(unsigned char* data)
{
    long re=0;
    int i,j,m,k,l;
    if((data[0]&128)==128)
    {
        m=-1;
        if(data[3]>0)
        {
            i=data[0]^255;
            j=data[1]^255;
            k=data[2]^255;
            l=(data[3]-1)^255;
        }
        else if(data[2]>0)
        {
            i=data[0]^255;
            j=data[1]^255;
            k=(data[2]-1)^255;
            l=0;
        }
        else if(data[1]>0)
        {
            i=data[0]^255;
            j=(data[1]-1)^255;
            k=0;
            l=0;
        }
        else
        {
            i=(data[0]-1)^255;
            j=0;
            k=0;
            l=0;
        }
    }
    else
    {
        m=1;
        i=data[0];
        j=data[1];
        k=data[2];
        l=data[3];
    }
    re=( i*16777216+j*65536+k*256+l)*m;
    return re;
}
unsigned long UNGet16Data4(unsigned char* data)
{
    unsigned long re=0;
    int i,j,m,k,l;
    m=1;
    i=data[0];
    j=data[1];
    k=data[2];
    l=data[3];
    re=( i*16777216+j*65536+k*256+l)*m;
    return re;
}
float GetFloatData4(unsigned char* data)
{
    float fltemp=0;
    u_char tt[4];
    memset(tt,0x00,4);
    tt[0]=data[3];
    tt[1]=data[2];
    tt[2]=data[1];
    tt[3]=data[0];
    memcpy(&fltemp,tt,4);
    fltemp=fltemp+0.00001;
    return fltemp;
}

QString GBK2UTF8(char* inStr)
{
    QTextCodec* gbCodec = QTextCodec::codecForName("GBK");
    QString strUnicode= gbCodec->toUnicode(inStr);

    return strUnicode.toUtf8();
}
int SetSystemTime(char *dt)
{
    struct tm tm;
    struct tm _tm;
    struct timeval tv;
    time_t timep;
    sscanf(dt, "%d-%d-%d %d:%d:%d", &tm.tm_year,
        &tm.tm_mon, &tm.tm_mday,&tm.tm_hour,
        &tm.tm_min, &tm.tm_sec);
    _tm.tm_sec = tm.tm_sec;
    _tm.tm_min = tm.tm_min;
    _tm.tm_hour = tm.tm_hour;
    _tm.tm_mday = tm.tm_mday;
    _tm.tm_mon = tm.tm_mon - 1;
    _tm.tm_year = tm.tm_year - 1900;

    timep = mktime(&_tm);
    tv.tv_sec = timep;
    tv.tv_usec = 0;
    if(settimeofday (&tv, (struct timezone *) 0) < 0)
    {
        printf("Set system datatime error!/n");
        return -1;
    }
    return 0;
}
int setCorrectTime(unsigned char *dataTime)
{
    int ret = 1;
    char mcd[128];
    unsigned char *p = dataTime;
    sprintf(mcd,"%04d-%02d-%02d %02d:%02d:%02d",2000+p[0],p[1],p[2],p[3],p[4],p[5]);
    SetSystemTime(mcd);
    system("hwclock -w");
    return ret;
}

void sdErrReadOnlyHandler()
{
    QFile qf("/opt/rebootCmd");

    int hourNow = QTime::currentTime().hour();
    if(qf.open(QIODevice::WriteOnly) && (1 || hourNow >= 21 || hourNow <= 5))
    {
        MyWriteLog("ERROR: detected fileSystem readonly error, try to dosfsck repair.");
        QTextStream out(&qf);
        out<<"umount /media/mmcblk0p1;dosfsck -a /dev/mmcblk0p1;mount /dev/mmcblk0p1 /media/mmcblk0p1";
        qf.close();
        system("sync");
        sleep(1);
        reboot(RB_AUTOBOOT);
    }
}
