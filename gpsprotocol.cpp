#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <linux/serial.h>
#include <termios.h>
#include <unistd.h>
#include "PublicFun.h"


float getDecimalPart(float num)
{
    int i=0;
    num = num-(int)num;
    for(i=0;i<5;i++){
        num *=10;
        if(num-(int)num==0){
            break;
        }
    }
    num /= 60;
    for(;i>=0;i--){
        num /= 10;
    }
    return num;
}

//参数 1  类型
//参数 2  设备地址
//参数 3  地址
//参数 4  读取长度
//参数 5  回复buf
int GpsReadData(int type, int DevAdder, int devBeginAdder, int len,char* fhBuf, int s_sock)
{
    int buflen, timeover=0, ret=0;
    char messBuf[1024];
    int deserveGNGGA = 1, deserveGNRMC = 1, receivedBytes=0;

    char* stPrefixGNGGA = "GNGGA";
    char* stPrefixGNRMC = "GNRMC";
    int  heightOfGNGGA = 9;
    int  statusOfGNRMC = 2;
    int  latitudeOfGNRMC = 3;
    int  NSOfGNRMC = 4;
    int  longitudeOfGNRMC = 5;
    int  EWOfGNRMC = 6;
    int  speedOfGNRMC = 7;
    int  courseOfGNRMC = 8;
    int  lenOfGNRMC = 13;//!GNGGA 内部信息个数
    int  lenOfGNGGA = 15;//!GNGGA 内部信息个数
    QString GNGGA, GNRMC;
    QStringList GNGGAList, GNRMCList;

    //! 标准gps数据信息
    //! GNGGA,085128.000,3748.20337,N,11233.22108,E,1,08,1.47,800.2,M,-19.9,M,,*5E
    //! GNRMC,085129.000,A,3748.20339,N,11233.22115,E,0.739,191.18,250917,,,A*46

    timeover = 10; //1000ms
    while(deserveGNGGA || deserveGNRMC)
    {
        char *pMsg = NULL, *pEnter = NULL;

        buflen=read(s_sock, messBuf, sizeof(messBuf));
        if(buflen <= 0 || NULL == strstr(messBuf, "\r\n")){
            usleep(100000);
            if(--timeover <= 0)
                break;
            else
                continue;
        }

        if(NULL != (pMsg = strstr(messBuf, stPrefixGNGGA)) && NULL != (pEnter = strstr(pMsg, "\r\n")))
        {
            *pEnter = 0;
            GNGGA = pMsg;
            GNGGAList = GNGGA.split(',');
            deserveGNGGA = 0;
            receivedBytes = 0;
        }
        if(NULL != (pMsg = strstr(messBuf, stPrefixGNRMC)) && NULL != (pEnter = strstr(pMsg, "\r\n")))
        {
            *pEnter = 0;
            GNRMC = pMsg;
            GNRMCList = GNRMC.split(',');
            deserveGNRMC = 0;
            receivedBytes = 0;
        }
    }
    //qDebug()<< GNGGA;
    //qDebug()<< GNRMC;

    if(0==deserveGNGGA && 0==deserveGNRMC)
    {
        int status=0;
        float speed = 0.0;
        float longitude = 0.0;
        float latitude = 0.0;
        if(lenOfGNRMC != GNRMCList.size() || lenOfGNGGA != GNGGAList.size()){
            MyWriteLog(commRdFailed, "ERROR: GPS info length invalid.");
            return 1;
        }

        speed = GNRMCList.at(speedOfGNRMC).toFloat() * 1.852;//1节 == 1.852km/h

        //qDebug()<<GNRMCList.at(longitudeOfGNRMC);
        //qDebug()<<GNRMCList.at(latitudeOfGNRMC);

        QString longitudeInt = GNRMCList.at(longitudeOfGNRMC).section(".", 0, 0).left(3);
        QString longitudeDecimal = GNRMCList.at(longitudeOfGNRMC).section(".", 0, 0).right(2) + "." + GNRMCList.at(longitudeOfGNRMC).section(".", 1, 1);//.mid(0, 4);
        QString latitudeInt = GNRMCList.at(latitudeOfGNRMC).section(".", 0, 0).left(2);
        QString latitudeDecimal = GNRMCList.at(latitudeOfGNRMC).section(".", 0, 0).right(2) + "."  + GNRMCList.at(latitudeOfGNRMC).section(".", 1, 1);//.mid(0, 4);

        longitude = longitudeInt.toFloat() + longitudeDecimal.toFloat()/60.0;
        latitude  = latitudeInt.toFloat()  + latitudeDecimal.toFloat()/60.0;

        //qDebug()<< longitudeInt<<longitudeDecimal<<longitude <<latitudeInt<< latitudeDecimal<<latitude;
        if(0 == strcmp("A", GNRMCList.at(statusOfGNRMC).toStdString().c_str()))
            status = 1;
        snprintf(fhBuf, len, "%d;%.6f;%s;%.6f;%s;%s;%.2f;%s",
            status,
            longitude,
            GNRMCList.at(EWOfGNRMC).toStdString().c_str(),
            latitude,
            GNRMCList.at(NSOfGNRMC).toStdString().c_str(),
            GNGGAList.at(heightOfGNGGA).toStdString().c_str(),
            speed,
            GNRMCList.at(courseOfGNRMC).toStdString().c_str());
        ret = 1;
        MyWriteLog(commRdSuccess, "GPS read success: %s", fhBuf);
    }
    else{
        strcpy(fhBuf, "0;\r\n");
       ret = 0;
    }
    return ret;
}

int GpsInit(int s_sock)
{
    int ret = 1;
    struct termios opt;
    try
    {
        if (tcgetattr(s_sock, &opt)<0) {
            ret = 0;
        }
        opt.c_lflag |= (ICANON | ECHO | ECHOE);
        if (tcsetattr(s_sock, TCSANOW, &opt)<0) {
            ret = 0;
        }
    }
    catch(...)
    {
        MyWriteLog("GpsProtocol-Init");
        ret = 0;
    }
    return ret;
}
