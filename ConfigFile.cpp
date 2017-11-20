#include <stdio.h>
#include <string.h>
#include <unistd.h>  //系统调用
#include <stdlib.h>  //标准库头文件
#include <fcntl.h>   //文件操作
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include "EDAType.h"
#include <sys/time.h>
#include <QString>
#include <QDebug>
#include <QTextCodec>
#include <QStringList>
#include <netdb.h>
#include "PublicFun.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern int resetWlanInterval;

void LoadChannelConfigFile(void)//解析channel.dll文件
{
    char name[32];
    memset(name,0x00,32);
    strcpy(name,"./configure/channel.dll");
    FILE *f=NULL;

    char s[1024];
    f = fopen(name,"a+");
    if(f!=NULL)
    {
        char* wz;
        int js=0;
        char nr[32];
        int temp1=0;
        int xzzz=0;
        while((fgets(s,1024,f))!=NULL)
        {
            if(NULL == strstr(s,",") || 0 == strncmp(s,"//", 2))
                continue;
            js=0;
            xzzz=0;
            if(strlen(s)<=1)
                break;
            channelConfig* tstrcut=new channelConfig;
            if(tstrcut!=NULL)
                memset(tstrcut,0x00,sizeof(channelConfig));
            else
                break;
           tstrcut->dataBuf=NULL;
           tstrcut->devNext=NULL;
           tstrcut->next=NULL;
            wz=strstr(s+js,",");
            while(wz!=NULL)
            {
                temp1=wz-s;
                temp1=temp1-js;
                memset(nr,0x00,32);
                memcpy(nr,s+js,temp1);
                if(xzzz==0)
                    tstrcut->channelID=atoi(nr);
                else if(xzzz==1)
                    tstrcut->devID=atoi(nr);
                else if(xzzz==2)
                    tstrcut->devGeneralAdder=atoi(nr);
                else if(xzzz==3)
                    tstrcut->devBeginAdder=atoi(nr);
                else if(xzzz==4)
                    tstrcut->devDataLength=atoi(nr);
                else if(xzzz==5)
                    tstrcut->devDataType=atoi(nr);
                xzzz++;
                js=wz-s;
                js++;
                if(strlen(s)<=js)
                    break;
                wz=strstr(s+js,",");
            }
            if(p_channelConfig==NULL)
            {
                p_channelConfig=tstrcut;
                p_channelConfig_t=p_channelConfig;
            }
            else
            {
                p_channelConfig_t->next=tstrcut;
                p_channelConfig_t=p_channelConfig_t->next;
            }
        }
        fclose(f);
    }
}
void LoadVarConfigFile(void)//解析var.dll文件
{
    char name[32];
    memset(name,0x00,32);
    strcpy(name,"./configure/var.dll");
    FILE *f=NULL;

    char s[1024];
    f = fopen(name,"a+");
    if(f!=NULL)
    {
        char* wz;
        int js=0;
        char nr[32];
        int temp1=0;
        int xzzz=0;
        while((fgets(s,1024,f))!=NULL)
        {
            js=0;
            xzzz=0;
            if(strlen(s)<=1)
                break;
            if(NULL == strstr(s,",") || 0 == strncmp(s,"//", 2))
                continue;
            varConfig* tstrcut=new varConfig;
            if(tstrcut!=NULL)
                memset(tstrcut,0x00,sizeof(varConfig));
            else
                break;
           tstrcut->next=NULL;
           tstrcut->memoryData=NULL;
           tstrcut->iVar=0;
           tstrcut->fVar=0;
           tstrcut->uiVar=0;
           gettimeofday(&tstrcut->data_Time,0);
           gettimeofday(&tstrcut->data_Compare_Time,0);
            wz=strstr(s+js,",");
            while(wz!=NULL)
            {
                temp1=wz-s;
                temp1=temp1-js;
                memset(nr,0x00,32);
                memcpy(nr,s+js,temp1);
                if(xzzz==0)
                    tstrcut->myid=atoi(nr);
                else if(xzzz==1)
                    tstrcut->channelID=atoi(nr);
                else if(xzzz==2)
                    tstrcut->memoryBeginAdder=atoi(nr);
                else if(xzzz==3)
                    tstrcut->memoryDataLeng=atoi(nr);
                else if(xzzz==4)
                    tstrcut->dataType=atoi(nr);
                else if(xzzz==5)
                    tstrcut->dataBit=atoi(nr);
                else if(xzzz==6)
                    tstrcut->storeType=atoi(nr);
                else if(xzzz==7)
                    tstrcut->storeData=atoi(nr);
                else if(xzzz==8)
                    tstrcut->storeCondition=atoi(nr);
                else if(xzzz==9)
                    tstrcut->storeVar=atoi(nr);
                else if(xzzz==10)
                    tstrcut->isCompression=atoi(nr);
                else if(xzzz==11)
                    tstrcut->isValid=atoi(nr);
                else if(xzzz==12)
                    tstrcut->isStore=atoi(nr);
                else if(xzzz==13)
                    strncpy(tstrcut->varName,nr, sizeof(tstrcut->varName));
                else if(xzzz==14)
                    strncpy(tstrcut->varAddrName,nr, sizeof(tstrcut->varAddrName));

                tstrcut->isStore=0;
                xzzz++;
                js=wz-s;
                js++;
                if(strlen(s)<=js)
                    break;
                if(xzzz==15)
                    tstrcut->varGroupId=atoi(s+js);
                wz=strstr(s+js,",");
            }
            if(p_varConfig==NULL)
            {
                p_varConfig=tstrcut;
                p_varConfig_t=p_varConfig;
            }
            else
            {
                p_varConfig_t->next=tstrcut;
                p_varConfig_t=p_varConfig_t->next;
            }
        }
        fclose(f);
    }
}
void InitLoadChannelConfig(void)//分配通道存储空间
{
    channelConfig* temp=p_channelConfig;
    char buf[32];
    int maxNum=0;
    while(temp!=NULL)
    {
        if(temp->channelID>=maxNum)
            maxNum=temp->channelID;
        temp->dataBuf=new unsigned char[temp->devDataLength];
        memset(temp->dataBuf,0,temp->devDataLength);
        temp=temp->next;
    }
    memset(buf,0,32);

    g_channelBuf=new channelBuf[maxNum+1];
    g_channelBuf_NUM=maxNum+1;
    for(int i=0;i<maxNum+1;i++)
    {
        g_channelBuf[i].pChannel=NULL;
    }
    temp=p_channelConfig;
    while(temp!=NULL)
    {
        g_channelBuf[temp->channelID].pChannel=temp;
        temp=temp->next;
    }
}



void InitVarConfig(void)//指定变量存储位置
{
    channelConfig* temp=p_channelConfig;
    varConfig* t_var=p_varConfig;
    while(temp!=NULL)
    {
        t_var=p_varConfig;
        while(t_var!=NULL)
        {
            if(temp->channelID==t_var->channelID)
            {
                if(temp->dataBuf!=NULL&&temp->devDataLength>=t_var->memoryBeginAdder+t_var->memoryDataLeng)
                    t_var->memoryData=temp->dataBuf+t_var->memoryBeginAdder;
            }
            t_var=t_var->next;
        }
        temp=temp->next;
    }
}
void LoadFTPConfigFile(void)//解析ftp.dll文件
{
    char name[32];
    memset(name,0x00,32);
    strcpy(name,"./configure//ftp.dll");
    FILE *f=NULL;

    char s[1024];
    f = fopen(name,"a+");
    if(f!=NULL)
    {
        char* wz;
        int js=0;
        char nr[32];
        int temp1=0;
        int xzzz=0;
        g_ftpConfig.isRun=0;
        g_ftpConfig.fileNum=0;
        g_ftpConfig.isZIP=0;
        if((fgets(s,1024,f))!=NULL)
        {
            js=0;
            xzzz=0;
            if(strlen(s)<=1)
                return;
            wz=strstr(s+js,",");
            while(wz!=NULL)
            {
                temp1=wz-s;
                temp1=temp1-js;
                memset(nr,0x00,32);
                memcpy(nr,s+js,temp1);
                if(xzzz==0)
                    g_ftpConfig.uploadTime=atoi(nr);
                else if(xzzz==1)
                    g_ftpConfig.uploadNum=atoi(nr);
                else if(xzzz==2)
                    strcpy(g_ftpConfig.serverIP,nr);
                else if(xzzz==3)
                    strcpy(g_ftpConfig.lockIP,nr);
                else if(xzzz==4)
                    strcpy(g_ftpConfig.ftpUser,nr);
                else if(xzzz==5)
                    strcpy(g_ftpConfig.ftpPass,nr);
                xzzz++;
                js=wz-s;
                js++;
                if(strlen(s)<=js)
                    return ;
                wz=strstr(s+js,",");
            }
        }
        fclose(f);
    }
}

void LoadDevConfigFile(void)//解析device.dll文件
{
    char name[32];
    memset(name,0x00,32);
    strcpy(name,"./configure/device.dll");
    FILE *f=NULL;

    char s[1024];
    f = fopen(name,"a+");
    if(f!=NULL)
    {
        char* wz;
        int js=0;
        char nr[32];
        int temp1=0;
        int xzzz=0;
        while((fgets(s,1024,f))!=NULL)
        {
            js=0;
            xzzz=0;
            if(strlen(s)<=1)
                break;
            devConfig* tstrcut=new devConfig;
            if(tstrcut!=NULL)
                memset(tstrcut,0x00,sizeof(devConfig));
            else
                break;
           tstrcut->devnext=NULL;
           tstrcut->dataBuf=NULL;
           tstrcut->next=NULL;
            wz=strstr(s+js,",");
            while(wz!=NULL)
            {
                temp1=wz-s;
                temp1=temp1-js;
                memset(nr,0x00,32);
                memcpy(nr,s+js,temp1);
                if(xzzz==0)
                    tstrcut->devID=atoi(nr);
                else if(xzzz==1)
                    tstrcut->interfaceID=atoi(nr);
                else if(xzzz==2)
                    tstrcut->devNum=atoi(nr);
                else if(xzzz==3)
                    tstrcut->isValid=atoi(nr);
                else
                    strcpy(tstrcut->devName,nr);
                xzzz++;
                js=wz-s;
                js++;
                if(strlen(s)<=js)
                    break;
                wz=strstr(s+js,",");
            }
            if(p_devConfig==NULL)
            {
                p_devConfig=tstrcut;
                p_devConfig_t=p_devConfig;
            }
            else
            {
                p_devConfig_t->next=tstrcut;
                p_devConfig_t=p_devConfig_t->next;
            }
        }
        fclose(f);
    }
}
void LoadnetworkConfigFile(void)//解析network.dll文件
{
    char name[32];
    memset(name,0x00,32);
    strcpy(name,"./configure/network.dll");
    FILE *f=NULL;

    char s[1024];
    f = fopen(name,"a+");
    if(f!=NULL)
    {
        char* wz;
        int js=0;
        char nr[128];
        int temp1=0;
        int xzzz=0;
        while((fgets(s,1024,f))!=NULL)
        {
            js=0;
            xzzz=0;
            if(strlen(s)<=1)
                break;
            networkConfig* tstrcut=new networkConfig;
            if(tstrcut!=NULL)
                memset(tstrcut,0x00,sizeof(networkConfig));
            else
                break;
           tstrcut->next=NULL;
            wz=strstr(s+js,",");
            while(wz!=NULL)
            {
                temp1=wz-s;
                temp1=temp1-js;
                memset(nr,0x00,32);
                memcpy(nr,s+js,temp1);
                if(xzzz==0)
                    tstrcut->interfaceID=atoi(nr);
                else if(xzzz==1)
                    tstrcut->devPort=atoi(nr);
                else if(xzzz==2)
                    strcpy(tstrcut->devIP,nr);
                else
                    strcpy(tstrcut->lockIP,nr);
                xzzz++;
                js=wz-s;
                js++;
                if(strlen(s)<=js)
                    break;
                wz=strstr(s+js,",");
            }

            if(p_networkConfig==NULL)
            {
                p_networkConfig=tstrcut;
                p_networkConfig_t=p_networkConfig;
            }
            else
            {
                p_networkConfig_t->next=tstrcut;
                p_networkConfig_t=p_networkConfig_t->next;
            }
        }
        fclose(f);
    }
}

void LoadSerialConfigFile(void)//解析serial.dll文件
{
    char name[32];
    memset(name,0x00,32);
    strcpy(name,"./configure/serial.dll");
    FILE *f=NULL;

    char s[1024];
    f = fopen(name,"a+");
    if(f!=NULL)
    {
        char* wz;
        int js=0;
        char nr[32];
        int temp1=0;
        int xzzz=0;
        while((fgets(s,1024,f))!=NULL)
        {
            js=0;
            xzzz=0;
            if(strlen(s)<=1)
                break;
            serialConfig* tstrcut=new serialConfig;
            if(tstrcut!=NULL)
                memset(tstrcut,0x00,sizeof(serialConfig));
            else
                break;
           tstrcut->next=NULL;
            wz=strstr(s+js,",");
            while(wz!=NULL)
            {
                temp1=wz-s;
                temp1=temp1-js;
                memset(nr,0x00,32);
                memcpy(nr,s+js,temp1);
                if(xzzz==0)
                    tstrcut->interfaceID=atoi(nr);
                else if(xzzz==1)
                    tstrcut->nBaudRate=atol(nr);
                else if(xzzz==2)
                    tstrcut->nParity=*nr;
                else if(xzzz==3)
                    tstrcut->nByteSize=atoi(nr);
                else
                    tstrcut->nStopBits=atoi(nr);
                xzzz++;
                js=wz-s;
                js++;
                if(strlen(s)<=js)
                    break;
                wz=strstr(s+js,",");
            }
            if(p_serialConfig==NULL)
            {
                p_serialConfig=tstrcut;
                p_serialConfig_t=p_serialConfig;
            }
            else
            {
                p_serialConfig_t->next=tstrcut;
                p_serialConfig_t=p_serialConfig_t->next;
            }
        }
        fclose(f);
    }
}

void LoadCanConfigFile(void)//解析canbus.dll文件
{
    char name[32];
    memset(name,0x00,32);
    strcpy(name,"./configure/canbus.dll");
    FILE *f=NULL;

    char s[1024];
    f = fopen(name,"a+");
    if(f!=NULL)
    {
        char* wz;
        int js=0;
        char nr[32];
        int temp1=0;
        int xzzz=0;
        while((fgets(s,1024,f))!=NULL)
        {
            js=0;
            xzzz=0;
            if(strlen(s)<=1)
                break;
            canBusConfig* tstrcut=new canBusConfig;
            if(tstrcut!=NULL)
                memset(tstrcut,0x00,sizeof(canBusConfig));
            else
                break;
           tstrcut->next=NULL;
            wz=strstr(s+js,",");
            while(wz!=NULL)
            {
                temp1=wz-s;
                temp1=temp1-js;
                memset(nr,0x00,32);
                memcpy(nr,s+js,temp1);
                if(xzzz==0)
                    tstrcut->interfaceID=atoi(nr);
                else if(xzzz==1)
                    tstrcut->nBaudRate=atol(nr);
                xzzz++;
                js=wz-s;
                js++;
                if(strlen(s)<=js)
                    break;
                wz=strstr(s+js,",");
            }
            if(p_canBusConfig==NULL)
            {
                p_canBusConfig=tstrcut;
                p_canBusConfig_t=p_canBusConfig;
            }
            else
            {
                p_canBusConfig_t->next=tstrcut;
                p_canBusConfig_t=p_canBusConfig_t->next;
            }
        }
        fclose(f);
    }
}

void InitChannelConfig(void)//确定同一设备上的所有通道
{
    channelConfig* temp=p_channelConfig;
    channelConfig* temp_t=p_channelConfig;
    channelConfig* temp_temp=p_channelConfig;
    while(temp!=NULL)
    {
        temp_t=temp->next;
        temp_temp=temp;

        while(NULL == temp_temp->devNext && temp_t!=NULL)
        {
            if(temp_t->devNext==NULL&&temp->devID==temp_t->devID)
            {
                temp_temp->devNext=temp_t;
                temp_temp=temp_t;
            }
            temp_t=temp_t->next;
        }
        temp=temp->next;
    }
}

void InitDevConfig(void)//确定同一接口下的所有设备
{
    devConfig* temp=p_devConfig;
    devConfig* temp_t=p_devConfig;
    devConfig* temp_temp=p_devConfig;
    while(temp!=NULL)
    {
        temp_t=temp->next;
        temp_temp=temp;
        while(temp_t!=NULL)
        {
            if(temp_t->devnext==NULL&&temp->interfaceID==temp_t->interfaceID)
            {
                temp_temp->devnext=temp_t;
                temp_temp=temp_t;
            }
            temp_t=temp_t->next;
        }

        channelConfig* temp11=p_channelConfig;
        while(temp11!=NULL)
        {
            if(temp->devID==temp11->devID)
            {
                temp->dataBuf=temp11;
                break;
            }
            temp11=temp11->next;
        }

        temp=temp->next;
    }
}

void InitInterfaceConfig(void)//解析Interface.dll文件
{
    char name[32];
    memset(name,0x00,32);
    strcpy(name,"./configure/Interface.dll");
    FILE *f=NULL;

    char s[1024];
    f = fopen(name,"a+");
    if(f!=NULL)
    {
        char* wz;
        int js=0;
        char nr[32];
        int temp1=0;
        int xzzz=0;
        while((fgets(s,1024,f))!=NULL)
        {
            js=0;
            xzzz=0;
            if(strlen(s)<=1)
                break;
            interfaceConfig* tstrcut=new interfaceConfig;
            if(tstrcut!=NULL)
                memset(tstrcut,0x00,sizeof(interfaceConfig));
            else
                break;
           tstrcut->netWork=NULL;
           tstrcut->serial=NULL;
           tstrcut->pDev=NULL;
           tstrcut->next=NULL;
            wz=strstr(s+js,",");
            while(wz!=NULL)
            {
                temp1=wz-s;
                temp1=temp1-js;
                memset(nr,0x00,32);
                memcpy(nr,s+js,temp1);
                if(xzzz==0)
                    tstrcut->interfaceID=atoi(nr);
                else if(xzzz==1)
                    tstrcut->interface=atoi(nr);
                else
                    strcpy(tstrcut->driveName,nr);
                xzzz++;
                js=wz-s;
                js++;
                if(strlen(s)<=js)
                    break;
                wz=strstr(s+js,",");
            }
            if(p_interfaceConfig==NULL)
            {
                p_interfaceConfig=tstrcut;
                p_interfaceConfig_t=p_interfaceConfig;
            }
            else
            {
                p_interfaceConfig_t->next=tstrcut;
                p_interfaceConfig_t=p_interfaceConfig_t->next;
            }
        }
        fclose(f);
    }
    interfaceConfig* ttttt=p_interfaceConfig;
    while(ttttt!=NULL)
    {
        devConfig* aaaaa=p_devConfig;
        networkConfig* b=p_networkConfig;
        serialConfig* c=p_serialConfig;
        canBusConfig* canBus=p_canBusConfig;
        while(aaaaa!=NULL)
        {
            if(ttttt->interfaceID==aaaaa->interfaceID)
            {
                ttttt->pDev=aaaaa;
                break;
            }
            aaaaa=aaaaa->next;
        }

        while(b!=NULL)
        {
            if(ttttt->interfaceID==b->interfaceID)
            {
                ttttt->netWork=b;
                b->ethPortNum = ttttt->interface;
                break;
            }
            b=b->next;
        }
        while(c!=NULL)
        {
            if(ttttt->interfaceID==c->interfaceID)
            {
                ttttt->serial=c;
                c->comPortNum = ttttt->interface;
                break;
            }
            c=c->next;
        }
        while(canBus!=NULL)
        {
            if(ttttt->interfaceID==canBus->interfaceID)
            {
                ttttt->canBus=canBus;
                canBus->canPortNum = ttttt->interface;
                break;
            }
            canBus=canBus->next;
        }
        ttttt=ttttt->next;
    }
}
void LoadServerConfigFile(void)//解析ftp.dll文件 服务器配置
{
    char name[32];
    memset(name,0x00,32);
    strcpy(name,"./configure/ftp.dll");
    FILE *f=NULL;

    char s[1024];
    f = fopen(name,"a+");
    if(f!=NULL)
    {
        char* wz;
        int js=0;
        char nr[128];
        int temp1=0;
        int xzzz=0;
        g_ServerConfig.Num=0;
        fgets(s,1024,f);
        if((fgets(s,1024,f))!=NULL)
        {
            js=0;
            xzzz=0;
            if(strlen(s)<=1)
                return;
            wz=strstr(s+js,",");
            while(wz!=NULL)
            {
                temp1=wz-s;
                temp1=temp1-js;
                memset(nr,0x00,sizeof(nr));
                memcpy(nr,s+js,temp1);
                if(xzzz==0)
                    strcpy(g_ServerConfig.serverIP,nr);
                else if(xzzz==1)
                    g_ServerConfig.Num=atoi(nr);
                else if(xzzz==2)
                    strcpy(g_ServerConfig.lockIP,nr);
                else if(xzzz==3){
                    g_ServerConfig.devId = atoi(nr);
                    strncpy(g_ServerConfig.iotDevName, nr, sizeof(g_ServerConfig.iotDevName));
                }else if(xzzz==4){
                    strncpy(g_ServerConfig.iotDevName, nr, sizeof(g_ServerConfig.iotDevName));
                    MyWriteLog("iot Dev Name: %s.", g_ServerConfig.iotDevName);
                }else if(xzzz==5){
                    resetWlanInterval = atoi(nr);
                }
                xzzz++;
                js=wz-s;
                js++;
                if(strlen(s)<=js)
                    return ;
                wz=strstr(s+js,",");
            }
        }

        g_ServerConfigAuxiliary.Num=0;
        if((fgets(s,1024,f))!=NULL)
        {
            js=0;
            xzzz=0;
            if(strlen(s)<=1)
                return;
            wz=strstr(s+js,",");
            while(wz!=NULL)
            {
                temp1=wz-s;
                temp1=temp1-js;
                memset(nr,0x00,sizeof(nr));
                memcpy(nr,s+js,temp1);
                if(xzzz==0)
                    strcpy(g_ServerConfigAuxiliary.serverIP,nr);
                else if(xzzz==1)
                    g_ServerConfigAuxiliary.Num=atoi(nr);
                else if(xzzz==2)
                    strcpy(g_ServerConfigAuxiliary.lockIP,nr);
                else if(xzzz==3){
                    g_ServerConfigAuxiliary.devId = atoi(nr);
                    strncpy(g_ServerConfigAuxiliary.iotDevName, nr, sizeof(g_ServerConfigAuxiliary.iotDevName));
                }
                if(xzzz==4){
                    strncpy(g_ServerConfigAuxiliary.iotDevName, nr, sizeof(g_ServerConfigAuxiliary.iotDevName));
                }
                xzzz++;
                js=wz-s;
                js++;
                if(strlen(s)<=js)
                    return ;
                wz=strstr(s+js,",");
            }
        }
        fclose(f);
    }
}

//row和column 从1开始
int replaceConfigColumn(char *file, int row, int column, int setValue)
{
    int ret = 0;
    QString filePath("./configure/");
    filePath += file;
    QFile qf(filePath);

    if(row < 1 || column < 1)
        return -1;

    if(qf.open(QIODevice::ReadWrite))
    {
        QTextStream in(&qf);
        QStringList configTxt;
        int lineRow=0;
        while(!in.atEnd())
        {
           QString line = in.readLine();
           if(++lineRow == row)
           {
               QStringList strSecList = line.split(",");
               if(column-1 < strSecList.count())
                    strSecList.replace(column-1, QString::number(setValue, 10));
               QString newLine = strSecList.join(",");
               //qDebug()<<"new line: "<<newLine;
               configTxt << newLine+"\r\n";
           }
           else
               configTxt << line+"\r\n";
        }
        qf.resize(0);
        QTextStream out(&qf);
        foreach (QString var, configTxt) {
            out << var;
        }
        qf.close();
    }
    else{
        ret = -1;
    }

    return ret;
}
int replaceConfigColumn(char *file, int row, int column, char* setString)
{
    int ret = 0;
    QString filePath("./configure/");
    filePath += file;
    QFile qf(filePath);

    if(row < 1 || column < 1)
        return -1;

    if(qf.open(QIODevice::ReadWrite))
    {
        QTextStream in(&qf);
        QStringList configTxt;
        int lineRow=0;
        while(!in.atEnd())
        {
           QString line = in.readLine();
           if(++lineRow == row)
           {
               QStringList strSecList = line.split(",");
               if(column-1 < strSecList.count())
                    strSecList.replace(column-1, QString(setString));
               QString newLine = strSecList.join(",");
               
               configTxt << newLine+"\r\n";
           }
           else
               configTxt << line+"\r\n";
        }
        qf.resize(0);
        QTextStream out(&qf);
        foreach (QString var, configTxt) {
            out << var;
        }
        qf.close();
    }
    else{
        ret = -1;
    }

    return ret;
}


int replaceConfigswitchServer(QString serverIp, QString serverPort)
{
    replaceConfigColumn("ftp.dll", 2, 1, (char*)serverIp.toStdString().c_str());
    replaceConfigColumn("ftp.dll", 2, 2, (char*)serverPort.toStdString().c_str());

    return 0;
}

int extractVarTable(QString &varTab)
{
    int varTabSecLen = 0;
    QString varInf, varName;
    if(NULL == p_varConfig)
        return 0;
    varConfig* pvar=p_varConfig;
    while(pvar){

        varName = GBK2UTF8(pvar->varName);
        int storeType = pvar->storeType==4?0:pvar->storeType;

        switch (pvar->dataType) {
        case 1:
            varInf = QString("%1,UINT%2,%3,%4,%5,%6\r\n").arg(pvar->myid).arg(pvar->memoryDataLeng*8).
                    arg(varName).arg(pvar->varAddrName).arg(storeType).arg(pvar->storeData);
            break;
        case 2:
            varInf = QString("%1,INT%2,%3,%4,%5,%6\r\n").arg(pvar->myid).arg(pvar->memoryDataLeng*8).arg(varName)
                    .arg(pvar->varAddrName).arg(storeType).arg(pvar->storeData);
            break;
        case 3:
            varInf = QString("%1,FLOAT,%2,%3,%4,%5\r\n").arg(pvar->myid).arg(varName)
                    .arg(pvar->varAddrName).arg(storeType).arg(pvar->storeData);
            break;
        case 4:
            varInf = QString("%1,BOOL,%2,%3,%4,%5\r\n").arg(pvar->myid).arg(varName)
                    .arg(pvar->varAddrName).arg(storeType).arg(pvar->storeData);
            break;
        case 5:
            varInf = QString("%1,CString,%2,%3,%4,%5\r\n").arg(pvar->myid).arg(varName)
                    .arg(pvar->varAddrName).arg(storeType).arg(pvar->storeData);
            break;
        default:
            MyWriteLog("WARN: detect invalid var type %s.", pvar->dataType);
            return 0;
            break;
        }

        if(varTabSecLen + varInf.size() > 800){
            varTabSecLen=varInf.size();
            varTab+=";";
        }
        varTab += varInf;
        varTabSecLen += varInf.size();
        pvar = pvar->next;
    }
    //QStringList strSecList = varTab.split(";");
    //qDebug() << strSecList.size() << strSecList.at(0) << strSecList.at(strSecList.size()-1);
    return 1;
}

void flushVarToFile()
{
    QFile varFile("./configure/var.dll");
    QString varInf;
    if(varFile.open(QIODevice::WriteOnly|QFile::Truncate))
    {
        varFile.seek(0);
        QTextStream out(&varFile);
        varConfig* pvar = p_varConfig;
        while(pvar) {
            varInf = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,\r\n")
                    .arg(pvar->myid).arg(pvar->channelID).arg(pvar->memoryBeginAdder).arg(pvar->memoryDataLeng)
                    .arg(pvar->dataType).arg(pvar->dataBit).arg(pvar->storeType).arg(pvar->storeData)
                    .arg(pvar->storeCondition).arg(pvar->storeVar).arg(pvar->isCompression).
                    arg(pvar->isValid).arg(pvar->isStore).arg(pvar->varName).arg(pvar->varAddrName).arg(pvar->varGroupId);
            out << varInf;
            pvar = pvar->next;
        }
        varFile.close();
    }
    else
    {
        MyWriteLog("ERROR: open varFile failed.");
    }

    return;
}

channelConfig* getChanlConfig(int varAddrParam1, int varAddrParam2, int dataLength)
{
    channelConfig* pchal=p_channelConfig;
    while(pchal!=NULL)
    {
        if(pchal->devBeginAdder == varAddrParam1 &&
                pchal->devGeneralAdder <= varAddrParam2 &&
                pchal->devGeneralAdder + pchal->devDataLength >= varAddrParam2 + dataLength)
            return pchal;
        pchal=pchal->next;
    }
    return NULL;
}

int parseNetConfigDomain()//域名解析
{
    char ip[16]={0};
    struct hostent *hptr;
    struct sockaddr_in adr_inet;

    if(inet_aton(g_ServerConfig.serverIP, &adr_inet.sin_addr))
    {
        return 0;
    }

    if(g_ServerConfig.serverIP)
    {
        if((hptr = gethostbyname(g_ServerConfig.serverIP)) == NULL)
        {
            MyWriteLog("ERROR: gethostbyname error for host: %s.", g_ServerConfig.serverIP);
            return 0;
        }
        else{
            inet_ntop(AF_INET, hptr->h_addr_list[0], ip, sizeof(ip));
            MyWriteLog("gethost %s: ip %s.", g_ServerConfig.serverIP, ip);
            strncpy(g_ServerConfig.serverIP, ip, sizeof(ip));
        }
    }
    return 1;
}

char *getChanlTypeStr(int chanlType)//获取通道数据类型
{
    char *chanlTypeStr = NULL;
    //0-i 1-q 2-m 3-db 4-di 5-d0 6-ai 7-ao
    switch (chanlType) {
    case 0:
        chanlTypeStr = "I";
        break;
    case 1:
        chanlTypeStr = "Q";
        break;
    case 2:
        chanlTypeStr = "M";
        break;
    case 3:
        chanlTypeStr = "DB";
        break;
    case 4:
        chanlTypeStr = "DI";
        break;
    case 5:
        chanlTypeStr = "DO";
        break;
    case 6:
        chanlTypeStr = "AI";
        break;
    case 7:
        chanlTypeStr = "AO";
        break;
    case 8:
        chanlTypeStr = "C-String";
        break;
    default:
        break;
    }
    return chanlTypeStr;
}

QByteArray readJsonFile()
{
    QByteArray byte_array;
    QString strFileName="topo.json";

    return byte_array;
}

void decodeJson(QByteArray jsonArray_)
{

}
void freeAllConfig()//释放配置空间
{
    channelConfig* pchal = p_channelConfig;
    while(NULL != pchal){
        pchal=pchal->next;
        free(pchal->dataBuf);
        free(pchal);
    }
    varConfig* pvar = p_varConfig;
    while(NULL != pvar){
        pvar=pvar->next;
        free(pvar);
    }
    if(NULL != g_channelBuf)
        free(g_channelBuf);

    devConfig* pdev = p_devConfig;
    while(NULL != pdev){
        pdev=pdev->next;
        free(pdev);
    }
    networkConfig* pnetwork = p_networkConfig;
    while(NULL != pnetwork){
        pnetwork=pnetwork->next;
        free(pnetwork);
    }

    interfaceConfig* pinterface = p_interfaceConfig;
    while(NULL != pinterface){
        pinterface=pinterface->next;
        free(pinterface);
    }
    serialConfig* pserial = p_serialConfig;
    while(NULL != pserial){
        pserial=pserial->next;
        free(pserial);
    }
    canBusConfig* pcanBus = p_canBusConfig;
    while(NULL != pcanBus){
        pcanBus=pcanBus->next;
        free(pcanBus);
    }
    return;
}

int readConfigFile(QString strFileName,QList<QStringList> &strColumnList)
{
    int ret = 0;
    QString strPath=QCoreApplication::applicationDirPath();
    QDir tempDir(strPath+"/configure/");

    if (tempDir.exists())
    {
        QFile file(tempDir.absolutePath() + "/" + strFileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            ret = -1;
            return ret;
        }
        QStringList strLineList;   ///行里的数据存入List中
        QTextStream in(&file);
        while(!in.atEnd()) {
            QString line = in.readLine();
            if(line.contains("\/\/")) ///跳过注释行
                continue;
            strLineList=line.split(',');
            if(strLineList.isEmpty())   ///?去掉空白行
                continue;

            strLineList.removeLast();
            strColumnList.append(strLineList);
        }
        file.close();
    }

    return ret;
}

int loadStatsConfigFile()//解析statsEvent.dll statsVar.dll文件
{
    int ret = 0;
    QList<QStringList> strColumnList;

    ret = readConfigFile("statsEvent.dll", strColumnList);
    if(ret < 0)
        return ret;
    for(int i=0; i<strColumnList.count(); i++){
        QStringList node = strColumnList.at(i);
        eventConfig *newNode        = new eventConfig;
        newNode->id                 = QString(node.at(0)).toUInt();
        strncpy(newNode->eventName, QString(node.at(1)).toStdString().c_str(), sizeof(newNode->eventName));
        newNode->interCycleMs       = QString(node.at(2)).toUInt();
        newNode->uploadCycleS       = QString(node.at(3)).toUInt();
        strncpy(newNode->eventTriggerCond, QString(node.at(4)).toStdString().c_str(), sizeof(newNode->eventTriggerCond));

        qDebug()<<newNode->id<<newNode->eventName<<newNode->interCycleMs<<newNode->uploadCycleS<<newNode->eventTriggerCond;

        g_eventConfig.insert(newNode->id, newNode);
    }
    strColumnList.clear();

    ret = readConfigFile(QString("statsVar.dll"), strColumnList);
    if(ret < 0)
        return ret;

    for(int i=0; i<strColumnList.count(); i++){
        QStringList node = strColumnList.at(i);
        statsVarConfig *newNode = new statsVarConfig;

        newNode->id         = QString(node.at(0)).toUInt();
        newNode->eventId    = QString(node.at(1)).toUInt();
        newNode->type       = statsType_e(QString(node.at(2)).toUInt());
        strncpy(newNode->varName, QString(node.at(3)).toStdString().c_str(), sizeof(newNode->varName));
        strncpy(newNode->varTriggerCond, QString(node.at(4)).toStdString().c_str(), sizeof(newNode->varTriggerCond));
        newNode->elapsedMs  = 0;
        newNode->enStats    = false;
        g_statsVarConfig.insert(newNode->id, newNode);

        eventConfig* eventNode = g_eventConfig.value(newNode->eventId);
        if(eventNode){
            eventNode->varGroup.insert(newNode->id, newNode);
            qDebug()<<newNode->id<<newNode->eventId<<newNode->varName;

        }
    }

    strColumnList.clear();

    return ret;
}

//加载配置文件
int LoadConfigFile(void)
{
    p_channelConfig=NULL;
    p_channelConfig_t=NULL;
    p_varConfig=NULL;
    p_varConfig_t=NULL;
    g_channelBuf=NULL;
    g_channelBuf_NUM=0;
    p_devConfig=NULL;
    p_devConfig_t=NULL;
    p_networkConfig=NULL;
    p_networkConfig_t=NULL;
    p_interfaceConfig=NULL;
    p_interfaceConfig_t=NULL;
    p_serialConfig=NULL;
    p_serialConfig_t=NULL;
    p_canBusConfig=NULL;
    p_canBusConfig_t=NULL;

    freeAllConfig();

    LoadChannelConfigFile();//解析channel.dll文件 --liut
    LoadVarConfigFile();//解析var.dll文件
    InitLoadChannelConfig();//分配通道存储空间
    InitVarConfig();//指定变量存储位置
    LoadFTPConfigFile();//解析ftp.dll文件
    LoadDevConfigFile();//解析device.dll文件
    LoadnetworkConfigFile();//解析network.dll文件
    InitChannelConfig();//确定同一设备上的所有通道
    InitDevConfig();//确定同一接口下的所有设备
    LoadSerialConfigFile();//解析serial.dll文件
    LoadCanConfigFile();//解析canbus.dll文件
    InitInterfaceConfig();//解析Interface.dll文件
    LoadServerConfigFile();//解析ftp.dll文件 服务器配置
    loadStatsConfigFile();//解析statsEvent.dll statsVar.dll文件 --liut

    parseNetConfigDomain();//域名解析


    varConfig* pvar= p_varConfig;
    while(pvar){
        g_varsHashTable.insert(pvar->myid, pvar);
        pvar = pvar->next;
    }

    return 1;
}
