#include <stdio.h>
#include <string.h>
#include <pthread.h>  //线程头文件
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QDebug>
#include <QSslSocket>
#include <QtEndian>
#include <QtScript/QScriptEngine>
#include <unistd.h>  //系统调用
#include <stdlib.h>  //标准库头文件
#include <fcntl.h>   //文件操作
#include <sys/ioctl.h> //IO
#include <linux/watchdog.h>
#include <time.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <signal.h>
#include <execinfo.h>
#include <QtNetwork>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include  <sys/wait.h>
#include "EDAType.h"
#include "PublicFun.h"
#include "ConfigFile.h"
#include "TCPClent.h"
#include "S7Protocol.h"
#include "serialport.h"
#include "socketcan.h"
#include "zip.h"
#include <dirent.h>
#include "MyFTP.h"
#include "sys/stat.h"
#include "zmq.h"
#include "FtpCLient.h"

extern int infoDbgLevel;
extern bool g_onFlushMemCache;

extern charDataList RtDateList;
extern charDataList statsRtDateList;
extern charDataList DbgRtDateList;
void MyFileWrite(int &filed,char* filePath,char* buffer,int len);//针对内存访问错误导致线程退出问题 --liut 171018


int upLinkInit(void* fd,char* lockIP,char* serverIp,int serverPort)//连接远程服务器 --liut
{
#if USEMQ
    int ret;
    char serverUrl[256];
    sprintf(serverUrl, "tcp://%s:%d", serverIp, serverPort);

    ret = zmq_connect(fd, serverUrl);
    if(ret<0)
    {
        MyWriteLog (commInfo,"ERROR: Failed to connect to the host %s: %s",serverUrl, strerror(errno));
        return 0;
    }
    else
    {
        //MyWriteLog(commInfo,"connect to %s successful.\n", serverUrl);
        return 1;
    }

#else
    return  InitSocketConnect(*((int*)fd), lockIP, serverIp, serverPort);
#endif

}

int upLinkClose(void* fd)
{
#if USEMQ
    int ret;
    ret = zmq_close(fd);
    if(ret)
    {
        return 0;
    }
    else
    {
        return 1;
    }
#else
    CloseSocketConnect(*((int*)fd));

    return 1;
#endif
}
int upLinkWrite(void *wd, const void *buf, size_t len, int flags)
{
#if USEMQ
    return zmq_send(wd, buf, len, flags);
#else
    return send(*(int*)wd, buf, len, flags);
#endif
}


int upLinkRead(void *rd, void *buf, size_t len, int flags)
{
#if USEMQ
    return zmq_recv(rd, buf, len, 0);//ZMQ_NOBLOCK
#else
    return recv(*(int*)rd, buf, len, flags);
#endif
}
int ctrlCmdRecv(void* sockfd)
{
    u_char buffer[2048];
    int recLen, ret = -1, datalenRemained, analysised=0;

    memset(buffer,0x00,2048);
    recLen=upLinkRead(sockfd,buffer,1024, 0);
    if(recLen>0){
        //printf("reclen %d 0x%02x\n[", recLen, recLen);
        //for(int i=0; i<recLen; i++)
        //    printf("%02x ", buffer[i]);
        //printf("]\n");
        datalenRemained = recLen;
        do{
            ret = Analysis(sockfd, buffer+analysised,recLen-analysised,analysised);
            datalenRemained -= analysised;
            //qDebug()<<"Analysis ret : "<<datalenRemained<<analysised;
        }while(datalenRemained && ret >= 0);
        //MyWriteLog(commRdSuccess,"INFO: ctrlCmdRecv success: recLen %d.", recLen);
    }
    else if(recLen<=0){
        if(errno == EAGAIN||errno == EWOULDBLOCK||errno == EINTR)
        {
            MyWriteLog("WARNING: ctrlCmd recv timeout. %d: %s. thread:%ld", errno, strerror(errno),pthread_self());
            ret = -2;
        }
        else
        {
            MyWriteLog(commRdFailed,"ERROR: ctrlCmdRecv failed: recLen %d. %d: %s", recLen, errno, strerror(errno));
            ret = -1;
        }
    }
    return ret;
}


int ctrlCmdSendMore(void* sockfd, u_char code, u_char *cmd, unsigned short cmdlen, unsigned short dataType, unsigned short packageNum)
{
    int i=0, ret = 0;
    unsigned short postLen=0,packageLen=0;
    u_char buf[2048];
    kdIotApdu *pWrApdu = (kdIotApdu*)buf;
    memset(buf,0,sizeof(buf));

    if(NULL != cmd && 0 == cmdlen || cmdlen > sizeof(buf) - sizeof(kdIotApdu))
    {
        MyWriteLog("ERROR: ctrlCmdSend cmd invalid.");
        return -2;
    }
    if(cmdlen)
        postLen = PACKAGE_HEAD_LENGTH + offsetof(dataUnit,dataPtr) + cmdlen;
    else
        postLen = PACKAGE_HEAD_LENGTH;

    packageLen = postLen - offsetof(kdIotApdu, packageNum);
    //qDebug()<<PACKAGE_HEAD_LENGTH<<offsetof(dataUnit,dataPtr)<<cmdlen<<packageLen<<offsetof(kdIotApdu, packageNum);

    pWrApdu->funcId = code;
    pWrApdu->length = packageLen;
    pWrApdu->packageNum = packageNum;
    strcpy(pWrApdu->devName, g_ServerConfig.iotDevName);

    //sprintf(pWrApdu->devName, "%d", g_ServerConfig.devId);
    if(cmdlen && cmd)
    {
        if(realDataUpload == code && 0 == cmd[0]){
            printf("error realDataUpload package:\n");
            for(int i=0; i<cmdlen; i++)
                printf("%02x ", cmd[i]);
            exit(1);
        }
        if(cmdlen>1024)
        {
            printf("error realDataUpload packagecmdlen:%d\n", cmdlen);
            exit(1);
        }
        pWrApdu->dUnit.dataType = (dataType);
        pWrApdu->dUnit.dataLength = (cmdlen);
        memcpy(pWrApdu->dUnit.dataPtr, cmd, cmdlen);
    }
    if(pWrApdu->length +3 != postLen){
        MyWriteLog("ERROR: package total data len error.") ;
        MyWriteLog("send postLen total %d, data %d .\n<", postLen, cmdlen);
        char errMsg[1024*4];
        for(i=0;i<postLen;i++)
        {
            sprintf(errMsg+i*3, "%02x ", buf[i]);
        }
        MyWriteLog(errMsg);
        MyWriteLog(">\n");
        //exit;
    }
    if(cmdlen & cmdlen+12 !=  pWrApdu->length)
    {
        MyWriteLog("error package: data len error.");
        MyWriteLog("send postLen total %d, data %d .\n<", postLen, cmdlen);
        char errMsg[1024*4];
        for(i=0;i<postLen;i++)
        {
            sprintf(errMsg+i*3, "%02x ", buf[i]);
        }
        MyWriteLog(errMsg);
        MyWriteLog(">\n");
        //exit;
    }
#if 1
    if(infoDbgLevel & 4){
        char dbgMsg[2048] = {0};
        MyWriteLog("send postLen total %d, data %d .", postLen, cmdlen);
        for(i=0;i<postLen;i++)
        {
            sprintf(dbgMsg+i*3, "%02x ", buf[i]);
        }
        sprintf(dbgMsg+i*3, ">");
        MyWriteLog(dbgMsg);
    }
#endif
    ret=upLinkWrite(sockfd,buf,postLen,0);

    if(ret < 0 ||  ret != postLen)
    {
#if USEMQ
        MyWriteLog(commWrFailed,"ERROR: func %d send to Server failed. ret %d. %s.",
                code, ret, zmq_strerror(errno));
#else
        MyWriteLog(commWrFailed,"ERROR: func %d send to Server failed. ret %d. %s.",
                code, ret, strerror(errno));
#endif
        ret = -1;
    }
    return ret;
}

int ctrlCmdSend(void* sockfd, u_char code, u_char *cmd, int cmdlen)
{
    return ctrlCmdSendMore(sockfd, code, cmd, cmdlen, basicType, 0);
}


int realTimeDataTransport(void* sockfd, ManagerFunctions func, QFile &rtCacheFile, int fileOffset, unsigned short& packageNum)
{
    static int s_compressionRatio=1;
    int ret = 0, errRetry=0, enCompress=ENABLE_RTDATA_COMPRESS;
    unsigned short dataType = cStringType;
    unsigned short orginalDataLen = 0, packageDataLen=0, compressdSize=0;
    uchar outBuf[1024]={0};
    uchar* pOutBuf = outBuf;
    QString offsetFlag ="fileOffset=";
    const char *streamTitle = func==realDataUpload?"realTime":"statsRealTime";

    if (FALSE == rtCacheFile.open(QIODevice::ReadWrite))
    {
        MyWriteLog("ERROR: open cache file %s failed: %s.", rtCacheFile.fileName().toStdString().c_str(), strerror(errno));
        return -2;
    }

    QTextStream in(&rtCacheFile);
    QString textData;
    QString firstLen = in.readLine();

    if(firstLen.contains(offsetFlag))
    {
        if(firstLen.at(offsetFlag.size()).isNumber()){
            char * offset = firstLen.toLatin1().data() + offsetFlag.size();
            fileOffset = atol(offset);
        }
    }

    if(0 == enCompress)
        s_compressionRatio = 1;

    while(!in.atEnd())
    {
        orginalDataLen = 0;
        textData.clear();
        in.seek(fileOffset);

        while(!in.atEnd())
        {
            QString s = in.readLine();
            s += "\r\n";
            if(s.contains("packageEnd.")){
                orginalDataLen += s.size();
                break;
            }
            if(textData.size() + s.size() >= s_compressionRatio * sizeof(outBuf))
                break;

            textData += s;
            orginalDataLen += s.size();
        }

        //qDebug()<<fileOffset<<textData.mid(0,20);
        if(0 < textData.size())
        {
            if(infoDbgLevel & 16){
                MyWriteLog("cache file:\r\n%s",textData.toStdString().c_str());
            }
            memset(outBuf, 0, sizeof(outBuf));

            //qDebug() << rtCacheFile.fileName() << " " << rtCacheFile.size() << " " << fileOffset;
            //qDebug() << "[ " <<textData.size() << "]\n"<< textData ;
            if(enCompress){
                compressdSize = zip_compress_mem_to_mem((void*)(outBuf), sizeof(outBuf), textData.toStdString().c_str(), textData.size());
                if(compressdSize <= 0){
                    MyWriteLog("ERROR: compress m2m failed.");
                    if(0 && s_compressionRatio > 1 ) {
                        s_compressionRatio--;
                        continue;
                    }
                }else{
                    if(0 &&  sizeof(outBuf) - compressdSize > compressdSize && s_compressionRatio < 10){
                        s_compressionRatio++;
                        continue;
                    }
                }

                packageDataLen = compressdSize;
                dataType = zipCompress * 256 + cStringType;
            }
            else{
                QByteArray ba = textData.toLatin1();
                packageDataLen = textData.size();
                memcpy(pOutBuf, (uchar*)ba.data(), packageDataLen);
                //qDebug()<<ba.mid(0,20).toHex().toUpper();
                //MyWriteLog(ba.mid(0,20).toHex().toUpper().toStdString().c_str());
            }

            if(packageDataLen > 0)
            {
                errRetry = 0;
                do{
                    ret = ctrlCmdSendMore(sockfd, func, pOutBuf, packageDataLen, dataType, packageNum);

                }while(ret<0 && EAGAIN == errno && errRetry++<3);
receiveagain:
                if(ret > 0)
                {
                    errRetry = 0;
                    do{
                        ret = ctrlCmdRecv(sockfd);
                    }while(ret<0 && EAGAIN == errno && errRetry++<3);


                    if(ret < 0){
                        MyWriteLog("ERROR: %s upload reply failed, ret:%d, errno:%d, %s(file)", streamTitle,ret,errno,strerror(errno));
                        break;
                    }
                    else
                    {
                        if(ret != packageNum){
                            MyWriteLog("ERROR: %s upload reply packageNum invalid : %d. packageNum: %d.(file)", streamTitle, ret,packageNum);
                            if(ret+1 == packageNum)
                            {
                                MyWriteLog("ERROR: internet problem, receive again.");
                                goto receiveagain;   //避免网络突然延迟导致无限发送同一个数据2次 ---liut 171025
                            }
                        }
                        else{
                            packageNum += 1;
                            fileOffset += orginalDataLen;

                        }
                        in.seek(0);
                        in << offsetFlag + QString("%1").arg(fileOffset) + "\r\n";
                    }
                }
                else{
#if USEMQ
                    MyWriteLog("ERROR: %s upload failed. errno %d. %s.", streamTitle, errno, zmq_strerror(errno));
#else
                    MyWriteLog("ERROR: %s upload failed. errno %d. %s.", streamTitle, errno, strerror(errno));
#endif
                    break;
                }
            }
            else{
                MyWriteLog("ERROR: %s upload invalid data.", streamTitle);
            }
        }
        if(func!=realDataUpload)
        {
            sleep(1);   //发送完1包统计数据 停顿1秒 用于等待服务器端数据入库，否则数据库报唯一约束错误 --liut 171013
        }
    }
    rtCacheFile.close();
    if(fileOffset == rtCacheFile.size())
    {
        MyWriteLog("WARNING: remove cache %s.", rtCacheFile.fileName().toStdString().c_str());
        if(false == rtCacheFile.remove())
        {
            if(EROFS == errno)
            {
                sdErrReadOnlyHandler();
            }
        }
    }

    return ret;
}

int realTimeDataTransport(void* sockfd, ManagerFunctions func, charDataList& dataList, unsigned short& packageNum)
{
    int ret = 0, errRetry=0, enCompress=ENABLE_RTDATA_COMPRESS, filed=0;//针对内存访问错误导致线程退出问题 增加变量filed --liut 171018
    unsigned short dataType = cStringType;
    unsigned short orginalDataLen = 0,packageDataLen = 0, compressdSize = 0;
    char *listData;
    uchar outBuf[1024]={0};
    uchar* pOutBuf = outBuf;
    const char *streamTitle = func==realDataUpload?"realTime":"statsRealTime";

    if(dataList.list.isEmpty())
    {
        int retryCount=0;
        g_onFlushMemCache = true;
        while(true == g_onFlushMemCache && retryCount++<5)
            usleep(100000);
    }
    dataList.rtlMutex.lock();   //针对内存访问错误导致线程退出问题 将锁移动到此处 --liut 171018
    if(dataList.list.count()>0)
    {
        listData = dataList.list.front();
        dataList.list.pop_front();  //针对内存访问错误导致线程退出问题 从链表中删除已取出节点 --liut 171018
        dataList.rtlMutex.unlock();   //针对内存访问错误导致线程退出问题 将此处锁关闭 --liut 171018

        //!判断是否立即发送报文，需要截掉标志部分
        char *findPackend = strstr(listData, "packageEnd.\r\n");
        if(findPackend){
            orginalDataLen = strlen((char*)listData)-strlen("packageEnd.\r\n");
        }else{
            orginalDataLen = strlen((char*)listData);
        }

        if(infoDbgLevel & 16){
            MyWriteLog("%s cache:\n%s",streamTitle,listData);
        }
        if(enCompress){
            compressdSize = zip_compress_mem_to_mem((void*)(outBuf), sizeof(outBuf), listData, orginalDataLen);
            if(compressdSize <= 0){
                MyWriteLog("ERROR: compress m2m failed.");
            }
            packageDataLen = compressdSize;
            dataType += zipCompress *256 + cStringType;
        }
        else{
            pOutBuf = (uchar*)listData;
            packageDataLen = orginalDataLen;
            dataType = cStringType;
        }


        if(packageDataLen > 0)
        {
            errRetry = 0;
            do{
                ret = ctrlCmdSendMore(sockfd, func, pOutBuf, packageDataLen, dataType, packageNum);
            }while(ret<0 && EAGAIN == errno && errRetry++<3);
receiveagain:
            if(ret > 0){
                errRetry = 0;
                do{
                    ret = ctrlCmdRecv(sockfd);
                }while(ret<0 && EAGAIN == errno && errRetry++<3);

                if(ret < 0){
                    MyWriteLog("ERROR: %s upload reply failed.errno:%d, %s(list)", streamTitle,errno,strerror(errno));
                }
                else{
                    if(ret != packageNum){
                        MyWriteLog("ERROR: %s upload reply packageNum invalid : %d.(list)", streamTitle, ret);
                        if(ret+1 == packageNum)
                        {
                            MyWriteLog("ERROR: internet problem, receive again.");
                            goto receiveagain;   //避免网络突然延迟导致无限发送同一个数据2次 ---liut 171025
                        }
                    }
                    else
                    {
                        packageNum += 1;
                        ret = orginalDataLen;
                        delete listData;
//                        dataList.rtlMutex.lock();     //针对内存访问错误导致线程退出问题 将此处锁关闭 --liut 171018
//                        dataList.list.pop_front();    //针对内存访问错误导致线程退出问题 将此处移到前面 --liut 171018
//                        dataList.rtlMutex.unlock();   //针对内存访问错误导致线程退出问题 将此处锁关闭 --liut 171018
                    }
                }
            }
            else{
#if USEMQ
                MyWriteLog("ERROR: %s upload failed. errno %d. %s.", streamTitle, errno, zmq_strerror(errno));
#else
                MyWriteLog("ERROR: %s upload failed. errno %d. %s.", streamTitle, errno, strerror(errno));
#endif
            }
        }
        else{
            MyWriteLog(commInfo,"ERROR: %s upload invalid data.", streamTitle);
        }

        if(ret != orginalDataLen)
        {
            MyFileWrite(filed,NULL,listData,strlen(listData));//针对内存访问错误导致线程退出问题 发送失败时重新加入链表中 --liut 171018
        }
    }
    else
    {
        dataList.rtlMutex.unlock();   //针对内存访问错误导致线程退出问题 将锁移动到此处 --liut 171018
    }

    return ret;
}
static unsigned short s_realTimeDataPackageNum;
static unsigned short s_statsDataPackageNum;

int remoteDataTransport(void* sockfd, ManagerFunctions func, QFile &rtCacheFile, int fileOffset)//上传历史文件 --liut
{
    int ret = 0;
    if(realDataUpload == func){
        ret = realTimeDataTransport(sockfd, func, rtCacheFile, fileOffset, s_realTimeDataPackageNum);
    }
    else{
        ret = realTimeDataTransport(sockfd, func, rtCacheFile, fileOffset, s_statsDataPackageNum);
    }

    return ret;
}
int remoteDataTransport(void* sockfd, ManagerFunctions func)//上传实时数据 --liut
{
    int ret = 0;
    if(realDataUpload == func){
        ret = realTimeDataTransport(sockfd, func, RtDateList, s_realTimeDataPackageNum);
    }
    else{
        ret = realTimeDataTransport(sockfd, func, statsRtDateList, s_statsDataPackageNum);
    }

    return ret;
}
