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
#include <ucontext.h>
#include  <sys/wait.h>
#include "EDAType.h"
#include "DAType.h"

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


void* dataAcquisition_Thread(void* arg);
void* Store_Thread(void* arg);
void* FileZIP_Thread(void* arg);
void* FTP_Thread(void* arg);
void* Manage_Thread(void* arg);
void* realTimeDataUpload_Thread(void* arg);
void* udpManagerServer_Thread(void* arg);
void* tcpManagerServer_Thread(void* arg);
void* statsEventDataUpload_Thread(void* arg);
void* statsVarAcquisition_Thread(void* arg);
void* statsEventStore_Thread(void* arg);
int on_extract_entry(const char *filename, void *arg);

int ctrlCmdRecv(void* sockfd);
int ctrlCmdSend(void* sockfd, u_char code, u_char *cmd, int cmdlen);

int time_cz;
int g_isStopFtpUpload=true;
int g_ManageThreadIsStarted=false;
bool g_onSwitchServer=false;
bool g_onFlushMemCache=false;
int infoDbgLevel=0;
int g_rmtDbgLevel=0;//1->变量流 2->通道流 4->日志流 7->全部
quint32 g_rmtLogLevel=(sysExecFlow|commInfo|sysErrInfo|commRdFailed);//logNone;//远程日志流log
QMutex sdCardRWMutex; //sd卡缓存读写操作互斥
//QStateMachine machine;
SysStateMachine_e g_sysState=State_INVALID;

charDataList RtDateList;
charDataList statsRtDateList;
charDataList DbgRtDateList;
//路由重启时间间隔(s 秒) --liut170923
int resetWlanInterval;

extern int parseNetConfigDomain();

void onSIGSEGV(int signum, siginfo_t *info, void *ptr)
{
    void *array[10];
    size_t size;
    char **strings;
    size_t i;
    MyWriteLog("get signal %d. %s. threadid:%d.",signum, strerror(errno),pthread_self());
    size = backtrace (array, 10);
    strings = backtrace_symbols (array, size);
    if (NULL == strings)
    {
        perror("backtrace_synbols");
        exit(EXIT_FAILURE);
    }

    MyWriteLog ("Obtained %zd stack frames.", size);

    for (i = 0; i < size; i++)
        MyWriteLog ("%s", strings[i]);

    free (strings);
    strings = NULL;

    exit(EXIT_FAILURE);
}
int checkLicense()//证书核查
{
    system("uname -mnps > /tmp/.sam");
    QFile pubSam("License");
    QFile priSam("/tmp/.sam");
    if(pubSam.open(QIODevice::ReadOnly) && priSam.open(QIODevice::ReadOnly))
    {
        QByteArray ba,bb,bc;
        quint8 opCode[] = {0x3c,0x21,0x4b,0x40,0x44,0x23,0x7a,0x24,0x6b,0x21,0x3e,0x0a};
        ba.resize(sizeof(opCode));
        memcpy(ba.data(), opCode, sizeof(opCode));
        QCryptographicHash md(QCryptographicHash::Md5);
        QByteArray data = priSam.readAll() + ba;
        md.addData(data);
        bb = md.result();
        bc = pubSam.readLine();
        if(32!=bb.toHex().size() || 33!=bc.size() || memcmp(bb.toHex().data(), bc.data(), 32))
        {
            MyWriteLog("ERROR: invalid License.");
            exit (-1);
        }
        pubSam.close();
        priSam.close();
    }
    else{
        MyWriteLog("ERROR: invalid License.");
        exit (-1);
    }
    system("rm /tmp/.sam");
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    if(argc > 1){
        infoDbgLevel = strtol(argv[1], NULL, 10);
        MyWriteLog("infoDbgLevel: %d",infoDbgLevel);
    }

    int maxThread=100;
    int js=0;
    pthread_attr_t attr;
    pthread_t th_array[100]={0},
            eventThreadArray[20]={0},eventAcq=0,statVarsAcq=0,
            th_Store=0,th_filezip=0,th_ftp=0,
            th_Manage=0,th_ManageBack=0,th_DataUpload=0,udpManagerServer=0,
            tcpManagerServer=0;
    struct statfs diskInfo;
    unsigned long long blocksize;
    unsigned long long freeDisk;
    unsigned long long availableDisk;

    char supportedDriver[][64]={
        "PLC-200",
        "PLC-300",
        "PLC-400",
        "PLC-1200",
        "PLC-1500",
        "S7-PLC300",
        "S7-PLC200",
        "S7-PLC400",
        "S7-PLC1200",
        "S7-PLC1500",
        "MODBUS-TCP",
        "MODBUS-TCPRTU",
        "MODBUS-RTU",        
        "CANBUS",
        "GPS",
        "AI",
        "DI"
    };

    checkLicense();//md5证书校验

    g_sysState = State_START;
    system("hwclock -s");//时区问题，需要使用指令保持date和hwclock 一致

    //! 超级指令执行，用于特殊的需要在主程序最开始运行
    QFile qf("/opt/rebootCmd");
    if(qf.open(QIODevice::ReadOnly))
    {
        char cmdBuf[1024]={0};
        QByteArray ba = qf.readAll();

        system(ba.data());
        qf.close();
        qf.remove();
    }

    struct sigaction act;
    int sig = SIGSEGV;
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = onSIGSEGV;
    act.sa_flags = SA_SIGINFO;
    if(sigaction(sig, &act, NULL)<0)
    {
          perror("sigaction:");
    }
    for(int j=SIGHUP; j<SIGRTMAX; j++ ){
        if(j != SIGCHLD)
            sigaction(j, &act, NULL);
    }
    LogFileInit();  //初始化日志文件

sysStart:
    MyWriteLog("--- KDZK IOT SYSTEM v%s #%s ---", SOFTVERSION, __TIMESTAMP_ISO__);
    QFile versionFile("/opt/version");
    if(versionFile.open(QIODevice::ReadOnly))
    {
        MyWriteLog("upgrade version: %s", (char*)versionFile.readAll().data());
    }

    LoadConfigFile(); //加载配置文件
    time_cz=0;    
    statfs("/media/mmcblk0p1/", &diskInfo);
    blocksize = diskInfo.f_bsize;    //每个block里包含的字节数
    freeDisk = diskInfo.f_bfree * blocksize; //剩余空间的大小
    availableDisk = diskInfo.f_bavail * blocksize;   //可用空间大小
    MyWriteLog("Disk_free = %llu MB(%llu GB) Disk_available = %llu MB(%llu GB).",
            freeDisk>>20, freeDisk>>30, availableDisk>>20, availableDisk>>30);
    if(availableDisk>>20 < 100)
        system("DISK_PATH=/media/mmcblk0p1/RTData/;cd $DISK_PATH ;rm -rf $(ls  ${DISK_PATH} | head -n1)");
    MyWriteLog("Main pid %d. threadid:%d.", getpid(), pthread_self());


    interfaceConfig* ttttt=p_interfaceConfig;
    devConfig* pdev=NULL;

    while(ttttt!=NULL)
    {
        pdev=ttttt->pDev;
        while(pdev!=NULL)
        {
            for(int i = 0; i< sizeof(supportedDriver)/sizeof(supportedDriver[0]); i++)
            {
                if(strcmp(ttttt->driveName,supportedDriver[i])==0)
                {
                    threadPar* t=new threadPar;
                    t->pInter=ttttt;
                    t->pDev=pdev;
                    pthread_attr_init(&attr);
                    pthread_attr_setdetachstate(&attr,   PTHREAD_CREATE_DETACHED);
                    pthread_create(&(th_array[js++]),&attr,dataAcquisition_Thread,(void*)t);//数据采集线程 --liut
                    pthread_attr_destroy(&attr);
                }
            }

            if(maxThread<=js)
            {
                MyWriteLog("is total threads > maxThread.");
                break;
            }
            pdev=pdev->devnext;
        }
        ttttt=ttttt->next;
    }

    js = 0;
    foreach (eventConfig* eventNode, g_eventConfig) {
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
        pthread_create(&(eventThreadArray[js++]),&attr,statsEventStore_Thread,(void*)eventNode);//统计数据存储线程 --liu
        pthread_attr_destroy(&attr);
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    pthread_create(&eventAcq,&attr,statsEventDataUpload_Thread,NULL);//统计数据上传线程 --liu
    pthread_attr_destroy(&attr);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    pthread_create(&statVarsAcq,&attr,statsVarAcquisition_Thread,NULL);//统计数据采集线程 --liu
    pthread_attr_destroy(&attr);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,   PTHREAD_CREATE_DETACHED);
    pthread_create(&th_Store,&attr,Store_Thread,NULL);//数据存储线程 --liu
    pthread_attr_destroy(&attr);
#if ENABLE_ZIP_STORE
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,   PTHREAD_CREATE_DETACHED);
    pthread_create(&th_filezip,&attr,FileZIP_Thread,NULL);
    pthread_attr_destroy(&attr);



    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,   PTHREAD_CREATE_DETACHED);
    pthread_create(&th_ftp,&attr,FTP_Thread,NULL);
    pthread_attr_destroy(&attr);
#endif

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,   PTHREAD_CREATE_DETACHED);
    pthread_create(&th_Manage,&attr,Manage_Thread,NULL);//主管理线程 --liu
    pthread_attr_destroy(&attr);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,   PTHREAD_CREATE_DETACHED);
    pthread_create(&th_ManageBack,&attr,Manage_Thread,&th_ManageBack);//备管理线程 --liu
    pthread_attr_destroy(&attr);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,   PTHREAD_CREATE_DETACHED);
    pthread_create(&th_DataUpload,&attr,realTimeDataUpload_Thread,NULL);//实时数据上传线程 --liu
    pthread_attr_destroy(&attr);

    if(State_RUNNING_DBG != g_sysState)
    {
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr,   PTHREAD_CREATE_DETACHED);
        pthread_create(&udpManagerServer,&attr,udpManagerServer_Thread,NULL);//udp 服务器管理线程 --liu
        pthread_attr_destroy(&attr);

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr,   PTHREAD_CREATE_DETACHED);
        pthread_create(&tcpManagerServer,&attr,tcpManagerServer_Thread,NULL);//tcp 服务器管理线程 --liu
        pthread_attr_destroy(&attr);
    }

    while(1)
    {
        if(State_STOP_DBG == g_sysState){
            g_sysState = State_RUNNING;
        }
        else if(State_START_DBG == g_sysState)
        {
            for(int i=0;i<sizeof(th_array);i++)
            {
                pthread_cancel(th_array[i]);
                pthread_join(th_array[i],NULL);
            }

            pthread_cancel(th_Store);
            pthread_join(th_Store,NULL);

            pthread_cancel(th_DataUpload);
            pthread_join(th_DataUpload,NULL);

            pthread_cancel(th_Manage);
            pthread_join(th_Manage,NULL);

            pthread_cancel(th_ManageBack);
            pthread_join(th_ManageBack,NULL);
            g_sysState = State_RUNNING_DBG;
            goto sysStart;
        }
        else
            g_sysState = State_RUNNING;

        sleep(5);
        parseNetConfigDomain();

        statfs("/media/mmcblk0p1/", &diskInfo);
        blocksize = diskInfo.f_bsize;    //每个block里包含的字节数
        freeDisk = diskInfo.f_bfree * blocksize; //剩余空间的大小
        availableDisk = diskInfo.f_bavail * blocksize;   //可用空间大小
        //MyWriteLog("Disk_free = %llu MB(%llu GB) Disk_available = %llu MB(%llu GB).",
        //        freeDisk>>20, freeDisk>>30, availableDisk>>20, availableDisk>>30);
        if(availableDisk>>20 < 100){ // 100M
            MyWriteLog("INFO: try to clear cache : RTData.");
            system("DISK_PATH=/media/mmcblk0p1/RTData/;cd $DISK_PATH ;rm -rf $(ls  ${DISK_PATH} | head -n1)");
        }
    }
    g_sysState = State_STOP;
    //th_array[], th_Store,th_filezip,th_ftp,th_Manage,th_ManageBack,udpManagerServer;
    for(int i=0;i<sizeof(th_array);i++)
    {
        pthread_cancel(th_array[i]);
        pthread_join(th_array[i],NULL);
    }

    pthread_cancel(th_Store);
    pthread_join(th_Store,NULL);

    pthread_cancel(th_DataUpload);
    pthread_join(th_DataUpload,NULL);

    pthread_cancel(th_Manage);
    pthread_join(th_Manage,NULL);

    pthread_cancel(th_ManageBack);
    pthread_join(th_ManageBack,NULL);

    MyWriteLog(sysExecFlow,"progect exit...");
    g_sysState = State_EXIT;

    CloseLogFile();
    return a.exec();
}

void writeToChannel(varConfig* pvar)
{
    unsigned char var[8];

    if(pvar->dataType==1)  //1-无符号整数，2-有符号整数，3-小数，4-位
    {qDebug() << " UINT: "<< pvar->wuiVar;
        qbswap_helper((uchar *)&pvar->wuiVar, (uchar*)var, pvar->memoryDataLeng);
        memcpy(pvar->memoryData, (uchar *)var, pvar->memoryDataLeng);
    }
    else if(pvar->dataType==2)  //1-无符号整数，2-有符号整数，3-小数，4-位
    {qDebug()  << " INT: " << pvar->wiVar;
        qbswap_helper((uchar *)&pvar->wiVar, (uchar*)var, pvar->memoryDataLeng);
        memcpy(pvar->memoryData, (uchar *)var, pvar->memoryDataLeng);
        qDebug() << var[0] << var[1]<< var[2]<< var[3];
        pvar->memoryData[pvar->memoryDataLeng-1] |= var[4];
    }
    else if(pvar->dataType==3)  //1-无符号整数，2-有符号整数，3-小数，4-位
    {
        qDebug() << " FLOAT: " << pvar->wfVar;
        qbswap_helper((uchar *)&pvar->wfVar, (uchar*)var, pvar->memoryDataLeng);
        qDebug() << var[0] << var[1]<< var[2]<< var[3];
        memcpy(pvar->memoryData, (uchar *)&pvar->wfVar, pvar->memoryDataLeng);
    }
    else if(pvar->dataType==4)  //位
    {
        qDebug() << " BIT: " << pvar->wiVar;
        if(pvar->wiVar)
            pvar->memoryData[0] |= 1<<pvar->dataBit;
        else
            pvar->memoryData[0] &= (~(1<<pvar->dataBit));
    }
#if 1
        for(int i=0; i<pvar->memoryDataLeng;i++)
            printf("%d ", pvar->memoryData[i]);
        printf("\n");
#endif
}
void* dataAcquisition_Thread(void* arg)//数据采集线程 --liut
{
    try
    {
        threadPar* t=(threadPar*)arg;
        char buffer[1024];
        char myPront[1024];
        int sockfd=0;
        FUNCREAD MyReadData = NULL;
        FUNCWRITE MyWriteData = NULL;
        FUNCINIT MyInit = NULL;
        int re=0, errCount=0;
        int fd;
        QMutex mutex;
        if(t==NULL)
            return arg;
        MyWriteLog("%s: threadid: %d. %s", __func__, pthread_self(), t->pInter->driveName);

        if(strcmp(t->pInter->driveName,"S7-PLC400")==0 ||
                strcmp(t->pInter->driveName,"PLC-400")==0)
        {
            MyReadData  = PLC400ReadData;
            MyWriteData = PLC400WriteData;
            MyInit      = PLC400Init;
            MyWriteLog("Load %s driver successful.", t->pInter->driveName);
        }
        else if(strcmp(t->pInter->driveName,"S7-PLC300")==0 ||
                strcmp(t->pInter->driveName,"PLC-300")==0)
        {
            MyReadData  = PLC300ReadData;
            MyWriteData = PLC300WriteData;
            MyInit      = PLC300Init;
            MyWriteLog("Load %s driver successful.", t->pInter->driveName);
        }
        else if(strcmp(t->pInter->driveName,"S7-PLC200")==0 ||
                strcmp(t->pInter->driveName,"PLC-200")==0   ||
                strcmp(t->pInter->driveName,"S7-PLC1200")==0 ||
                strcmp(t->pInter->driveName,"S7-PLC1500")==0)
        {
            MyReadData  = PLC200ReadData;
            MyWriteData = PLC200WriteData;
            MyInit      = PLC200Init;
            MyWriteLog("Load %s driver successful.", t->pInter->driveName);
        }
        else if(strcmp(t->pInter->driveName,"MODBUS-TCP")==0)
        {
            MyReadData  = ModBusTcpReadData;
            MyInit      = ModBusTcpInit;
            MyWriteLog("Load %s driver successful.", t->pInter->driveName);
        }
        else if(strcmp(t->pInter->driveName,"MODBUS-RTU")==0
                || strcmp(t->pInter->driveName,"MODBUS-TCPRTU")==0)
        {
            MyReadData  = ModBusRtuReadData;
            MyInit      = ModBusTcpInit;
            MyWriteLog("Load %s driver successful.", t->pInter->driveName);
        }
        else if(strcmp(t->pInter->driveName,"CANBUS")==0)
        {
            MyReadData  = socketCanRead;
            MyInit      = SocketCanInit;
            MyWriteLog("Load %s driver successful.", t->pInter->driveName);
        }
        else if(strcmp(t->pInter->driveName,"GPS")==0)
        {
            MyReadData  = GpsReadData;
            MyInit      = GpsInit;
            MyWriteLog("Load %s driver successful.", t->pInter->driveName);
        }
        else if(strcmp(t->pInter->driveName,"AI")==0)
        {
            MyReadData  = AIReadData;
            MyWriteLog("Load %s driver successful.", t->pInter->driveName);
        }
        else if(strcmp(t->pInter->driveName,"DI")==0)
        {
            MyReadData  = DIReadData;
            MyWriteLog("Load %s driver successful.", t->pInter->driveName);
        }
        else
        {
            MyWriteLog("ERROR: load driver %s failed.", t->pInter->driveName);
            return arg;
        }


        memset(myPront,0x00,1024);
        sprintf(myPront,"/media/mmcblk0p1/%sThread%02d%02d",t->pInter->driveName,t->pInter->interfaceID,t->pDev->devID);
        fd=open(myPront,O_RDWR|O_CREAT|O_APPEND);

        MyWriteLog(sysExecFlow,"dataAcquisition_Thread run: devId %d, InterfaceId %d, %s",
                   t->pDev->devID, t->pInter->interfaceID,t->pInter->driveName);

        channelConfig* pChannel=t->pDev->dataBuf;
        channelConfig* t1;
        varConfig* pvar=NULL;
        while(pChannel!=NULL)
        {
            mutex.lock();

            if(sockfd==0)
            {
                if(t->pInter->netWork)//以太 --liut
                {
                    if(InitSocketConnect(sockfd,t->pInter->netWork->lockIP,t->pInter->netWork->devIP,t->pInter->netWork->devPort)>0)
                    {                        
                        MyWriteLog("net%d: Connect to %s:%d successful.",
                                   t->pInter->netWork->ethPortNum, t->pInter->netWork->devIP,t->pInter->netWork->devPort);
                        if(MyInit(sockfd)<=0)
                        {
                            MyWriteLog("%s %s Connect failed.",t->pInter->driveName, t->pInter->netWork->devIP);
                            CloseSocketConnect(sockfd);
                            sockfd=0;
                            sleep(2);
                        }
                        else
                            MyWriteLog("%s Connect successful.", t->pInter->driveName);
                    }
                    else
                    {
                        MyWriteLog("ERROR: net%d: Connect to %s:%d failed.",
                                t->pInter->netWork->ethPortNum, t->pInter->netWork->devIP,t->pInter->netWork->devPort);
                        sockfd=0;
                        sleep(2);
                    }

                }
                else if(t->pInter->serial)//串口 --liut
                {
                    if(InitSerialConnect(sockfd,t->pInter->serial->comPortNum,
                                         t->pInter->serial->nBaudRate,
                                         t->pInter->serial->nParity,
                                         t->pInter->serial->nByteSize,
                                         t->pInter->serial->nStopBits))
                    {
                        if(MyInit(sockfd)<=0)
                        {
                            MyWriteLog("Com%d connect failed.", t->pInter->serial->comPortNum);
                            sockfd=0;
                            sleep(2);
                        }
                        else
                            MyWriteLog("Com%d Connect successful.", t->pInter->serial->comPortNum);
                    }
                    else
                    {
                        MyWriteLog("ERROR: Com%d connect failed.", t->pInter->serial->comPortNum);
                        sockfd=0;
                        sleep(2);
                    }

                }
                else if(t->pInter->canBus)//can总线 --liut
                {
                    if(InitSocketCanConnect(sockfd,t->pInter->canBus->canPortNum,t->pInter->canBus->nBaudRate))
                    {
                        MyWriteLog("can%d connect successful.", t->pInter->canBus->canPortNum);
                    }
                    else
                    {
                        MyWriteLog("ERROR: can%d connect failed.", t->pInter->canBus->canPortNum);
                        sockfd=0;
                        sleep(2);
                    }
                }
                else if(strcmp(t->pInter->driveName,"AI")==0)
                {
                    if(InitSocketAIConnect(sockfd))
                    {
                        sockfd = 1;
                        MyWriteLog("AI connect successful.");
                    }
                    else
                    {
                        MyWriteLog("ERROR: AI connect failed.");
                        sockfd=0;
                        sleep(2);
                    }
                }
                else if(strcmp(t->pInter->driveName,"DI")==0)
                {
                    if(InitSocketDIConnect(sockfd))
                    {
                        sockfd = 1;
                        MyWriteLog("DI connect successful.");
                    }
                    else
                    {
                        MyWriteLog("ERROR: DI connect failed.");
                        sockfd=0;
                        sleep(2);
                    }
                }
            }
            t1=pChannel;
            memset(buffer,0x00,1024);
            while(sockfd!=0&&t1!=NULL)
            {
                /*
                memset(myPront,0x00,1024);
                sprintf(myPront,"ReadData DataType:%d,GeneralAdder:%d,BeginAdder:%d,DataLength:%d",t1->devDataType,t1->devGeneralAdder, t1->devBeginAdder, t1->devDataLength);
                if(t1->devID==2&&t1->devDataType==2)
                    MyDebug(myPront);
                    */

                re=MyReadData(t1->devDataType,t1->devGeneralAdder, t1->devBeginAdder, t1->devDataLength,buffer,sockfd);                
                if(re > 0)//数据采集完成 --liut
                {
                    t1->statsReadSuccess++;
                    t1->statsReadFailed = 0;
                    memcpy(t1->dataBuf,buffer,t1->devDataLength);
                    MyWriteLog(commRdSuccess, "chanl [%d] ReadData DataType:%d,GeneralAdder:%d,BeginAdder:%d,DataLength:%d",t1->channelID,t1->devDataType,t1->devGeneralAdder, t1->devBeginAdder, t1->devDataLength);
#if 1
                    if(infoDbgLevel & 1)//调试 --liut
                    {
                        char dbgMsg[2048] = {0};
                        MyWriteLog("[%d] ReadData DataType:%d,GeneralAdder:%d,BeginAdder:%d,DataLength:%d :",t1->channelID,t1->devDataType,t1->devGeneralAdder, t1->devBeginAdder, t1->devDataLength);
                        for(int i=0;i<t1->devDataLength;i++)
                            sprintf(dbgMsg+i*3, "%02x ", buffer[i]);
                        MyWriteLog(dbgMsg);
                    }
                    if(g_rmtDbgLevel)
                    {
                        if(DbgRtDateList.list.count() <= 100){
                            DbgRtDateList.rtlMutex.lock();
                            char dbgMsg[2048] = {0};
                            sprintf(dbgMsg, "channel:%d\r\n", t1->channelID);
                            int offset = strlen(dbgMsg);
                            for(int i=0;i<t1->devDataLength;i++)
                                sprintf(dbgMsg+offset+i*2, "%02x", buffer[i]);
                            strcat(dbgMsg,"\r\n");
                            char *p = strdup(dbgMsg);
                            DbgRtDateList.list.push_back(p);
                            DbgRtDateList.rtlMutex.unlock();
                        }
                    }
#endif
                    for(pvar= p_varConfig; pvar; pvar=pvar->next)
                    {                        
                        if(pvar->channelID == t1->channelID &&1 == pvar->isWriteCmdValid)//写入设备数据 --liut
                        {
                            writeToChannel(pvar);
                            if(MyWriteData)
                            {
                                re=MyWriteData(t1->devDataType,t1->devGeneralAdder+pvar->memoryBeginAdder, t1->devBeginAdder, pvar->memoryDataLeng,(char*)pvar->memoryData,sockfd);
                                if(re < 0){
                                    pvar->isWriteCmdValid = -1;
                                    MyWriteLog(commWrFailed,"WriteData ERROR: [channnel %d] var %d dataType:%d,generalAdder:%d,beginAdder:%d,dataLength:%d.",
                                            t1->channelID, pvar->myid, t1->devDataType,t1->devGeneralAdder+pvar->memoryBeginAdder, t1->devBeginAdder, pvar->memoryDataLeng);
                                }
                                else{
                                    pvar->isWriteCmdValid = 0;
                                    MyWriteLog(commWrSuccess,"WriteData SUCCESS: [channnel %d] var %d dataType:%d,generalAdder:%d,beginAdder:%d,dataLength:%d.",
                                            t1->channelID, pvar->myid, t1->devDataType,t1->devGeneralAdder+pvar->memoryBeginAdder, t1->devBeginAdder, pvar->memoryDataLeng);
                                }
                            }
                        }
                    }
                }
                else if(re<0)
                {
                    if(t1->statsReadFailed++ > 50)
                        memset(t1->dataBuf, 0, t1->devDataLength);
                    MyWriteLog(commRdFailed,"ReadData ERROR: ret %d rdErrCnt %d.[channnel %d] dataType:%d,generalAdder:%d,beginAdder:%d,dataLength:%d.",
                            re, t1->statsReadFailed, t1->channelID, t1->devDataType,t1->devGeneralAdder, t1->devBeginAdder, t1->devDataLength);
                    errCount = 0;
                    //sleep(1);
                    //usleep(200000);
                    if(t->pInter->netWork){
                        CloseSocketConnect(sockfd);
                        sockfd=0;
                    }
                }else if(re == 0)
                {
                    MyWriteLog(commRdFailed,"ReadData ERROR: ret = 0 [channnel %d] dataType:%d,generalAdder:%d,beginAdder:%d,dataLength:%d.",
                            t1->channelID, t1->devDataType,t1->devGeneralAdder, t1->devBeginAdder, t1->devDataLength);

                }                

                if(t->pInter->serial)
                    usleep(20000);
                t1=t1->devNext;
            }
            mutex.unlock();
            sleep(1);//usleep(20000);
        }

        MyWriteLog(sysExecFlow,"%s: threadid: %d exit.", __func__, pthread_self());

        if(fd>0)
        {
            close(fd);
        }
        return arg;
    }
    catch(...)
    {
        MyWriteLog(sysExecFlow,"dataAcquisition_Thread ERROR");
        return arg;
    }
}

int IsStoreCondition(varConfig* pvar)
{
    int isStore=0;
    ////存储条件  1-始终存储 2-某个变量大于等于1存储
    if(pvar->storeCondition==1)
        isStore=1;
    else
    {
        if(pvar->storeVar>0)
        {
            varConfig* temp_pvar= p_varConfig;
             while(temp_pvar!=NULL)
             {
                 if(temp_pvar->myid==pvar->storeVar)
                     break;
                 temp_pvar=temp_pvar->next;
             }
             if(temp_pvar!=NULL)
             {
                 if(pvar->dataType==1&&pvar->uiVar>=1)
                     isStore=1;
                 else if(pvar->dataType==2&&pvar->iVar>=1)
                     isStore=1;
                  else if(pvar->dataType==3&&pvar->fVar>=1)
                     isStore=1;
                 else if(pvar->dataType==4&&pvar->iVar>=1)
                     isStore=1;
                 else
                     isStore=0;
             }
             else
                 isStore=1;
        }
        else
            isStore=1;
    }
    return isStore;
}
void mywritefile(varConfig* pvar,char* buffer)
{
    gettimeofday(&pvar->data_Time,0);
    time_t t;
    struct tm * a;
    time(&t);
    a=localtime(&t);
    if(pvar->dataType==1)  //1-无符号整数，2-有符号整数，3-小数，4-位
    {
        unsigned long sz=0;
        unsigned long m=1;
        for(int o=pvar->memoryDataLeng-1;o>=0;o--)
        {if(pvar->isWriteCmdValid) printf("[%d]%d ", o, pvar->memoryData[o]);
            sz=sz+pvar->memoryData[o]*m;
            m=m*256;
        }

        pvar->uiVar=sz;
#if !USE_TIME_SINCE_EPOCH
        sprintf(buffer,"%d,%u,%02d-%02d-%02d %02d:%02d:%02d\r\n",pvar->myid,pvar->uiVar,
                1900+a->tm_year,1+a->tm_mon,a->tm_mday, a->tm_hour,a->tm_min,a->tm_sec);
#else
        sprintf(buffer,"%d,%u,%lld\r\n", pvar->myid,pvar->uiVar,QDateTime::currentMSecsSinceEpoch());
#endif
    }
    else if(pvar->dataType==2)  //1-无符号整数，2-有符号整数，3-小数，4-位
    {
        if(pvar->memoryDataLeng==1)
            pvar->iVar=pvar->memoryData[0];
        else if(pvar->memoryDataLeng==2)
            pvar->iVar=Get16Data2(pvar->memoryData);
        else if(pvar->memoryDataLeng==4)
            pvar->iVar=Get16Data4(pvar->memoryData);
#if !USE_TIME_SINCE_EPOCH
        sprintf(buffer,"%d,%ld,%02d-%02d-%02d %02d:%02d:%02d\r\n",pvar->myid,pvar->iVar,
                1900+a->tm_year,1+a->tm_mon,a->tm_mday, a->tm_hour,a->tm_min,a->tm_sec);
#else
        sprintf(buffer,"%d,%ld,%lld\r\n",pvar->myid,pvar->iVar,QDateTime::currentMSecsSinceEpoch());
#endif
    }
    else if(pvar->dataType==3)  //1-无符号整数，2-有符号整数，3-小数，4-位
    {
        unsigned char tt[4];
        memset(tt,0x00,4);
        tt[0]=pvar->memoryData[3];
        tt[1]=pvar->memoryData[2];
        tt[2]=pvar->memoryData[1];
        tt[3]=pvar->memoryData[0];
        memcpy(&pvar->fVar,tt,4);
        pvar->fVar=pvar->fVar+0.00001;
#if !USE_TIME_SINCE_EPOCH
        sprintf(buffer,"%d,%.2f,%02d-%02d-%02d %02d:%02d:%02d\r\n",pvar->myid,pvar->fVar,
                1900+a->tm_year,1+a->tm_mon,a->tm_mday, a->tm_hour,a->tm_min,a->tm_sec,pvar->data_Time.tv_usec/1000);
#else
        sprintf(buffer,"%d,%.2f,%lld\r\n",pvar->myid,pvar->fVar, QDateTime::currentMSecsSinceEpoch());
#endif
    }
    else if(pvar->dataType==4)//位
    {
        pvar->iVar=pvar->memoryData[0];
        if(pvar->dataBit>0)
            pvar->iVar=(pvar->iVar>>pvar->dataBit);
        if((pvar->iVar&1)==1)
            pvar->iVar=1;
        else
            pvar->iVar=0;
#if !USE_TIME_SINCE_EPOCH
        sprintf(buffer,"%d,%u,%02d-%02d-%02d %02d:%02d:%02d\r\n",pvar->myid,pvar->iVar,
                1900+a->tm_year,1+a->tm_mon,a->tm_mday, a->tm_hour,a->tm_min,a->tm_sec);
#else
        sprintf(buffer,"%d,%u,%lld\r\n",pvar->myid,pvar->iVar, QDateTime::currentMSecsSinceEpoch());
#endif
    }
    else if(pvar->dataType==5)//字符串
    {
#if !USE_TIME_SINCE_EPOCH
        snprintf(buffer,pvar->memoryDataLeng,"%d,%s,%02d-%02d-%02d %02d:%02d:%02d\r\n",pvar->myid,(char*)pvar->memoryData,
                1900+a->tm_year,1+a->tm_mon,a->tm_mday, a->tm_hour,a->tm_min,a->tm_sec);
#else
        snprintf(buffer,pvar->memoryDataLeng,"%d,%s,%lld\r\n",pvar->myid,(char*)pvar->memoryData,
                QDateTime::currentMSecsSinceEpoch());
#endif
     }
//    qDebug() << "------------"<<buffer;
}

int timeval_subtract(struct timeval* result, struct timeval* x, struct timeval* y)
{
    if ( x->tv_sec>y->tv_sec )
              return -1;

    if ( (x->tv_sec==y->tv_sec) && (x->tv_usec>y->tv_usec) )
              return -1;

    result->tv_sec = ( y->tv_sec-x->tv_sec );
    result->tv_usec = ( y->tv_usec-x->tv_usec );

    if (result->tv_usec<0)
    {
              result->tv_sec--;
              result->tv_usec+=1000000;
    }

    return 0;
}
//返回1 写入文件，返回0，不写
int  zgzmywritefile(varConfig* pvar,char* buffer)
{
    int iss=IsStoreCondition(pvar);
    int re=0;
    struct timeval start,diff;
    if(iss>0)
    {
        mywritefile(pvar,buffer);
        if(2 == pvar->isWriteCmdValid){
            qDebug() <<buffer;
            pvar->isWriteCmdValid=0;
        }
        if(pvar->isStore==1)
        {
            re=1;
            pvar->isStore=0;
        }
        else
        {
            //存储类型（1-差异存储，2-比例存储（百分比），
            //3-比例存储（数值） 4-定期存储，单位是毫秒）
            if(pvar->storeType==1)
            {
                if(pvar->dataType==1)
                {
                    if( pvar->uiVar!= pvar->huiVar)
                    {
                        re=1;
                        pvar->huiVar=pvar->uiVar;
                    }
                }
                else if(pvar->dataType==2)
                {
                    if( pvar->iVar!= pvar->hiVar)
                    {
                        re=1;
                        pvar->hiVar=pvar->iVar;
                    }
                }
                 else if(pvar->dataType==3)
                {
                    if( pvar->fVar!= pvar->hfVar)
                    {
                        re=1;
                        pvar->hfVar=pvar->fVar;
                    }
                }
                else if(pvar->dataType==4)
                {
                    if( pvar->iVar!= pvar->hiVar)
                    {
                        re=1;
                        pvar->hiVar=pvar->iVar;
                    }
                }
                else
                    re=0;
                if(pvar->storeData > 500)
                {
                    gettimeofday(&start,0);
                    timeval_subtract(&diff,&(pvar->data_Compare_Time),&start);
                    if(diff.tv_sec*1000 + diff.tv_usec/1000 >= pvar->storeData)
                    {
                        gettimeofday(&pvar->data_Compare_Time,0);
                        re=1;
                    }
                }
            }
            else if(pvar->storeType==2)
            {
                if(pvar->dataType==1)
                {
                    unsigned int temp=pvar->uiVar*(pvar->storeData/100.00);
                    if(abs(pvar->uiVar-pvar->huiVar)>=temp)
                    {
                        re=1;
                        pvar->huiVar=pvar->uiVar;
                    }
                }
                else if(pvar->dataType==2)
                {
                    int temp=pvar->iVar*(pvar->storeData/100.00);
                    if( abs(pvar->iVar-pvar->hiVar)>=temp)
                    {
                        re=1;
                        pvar->hiVar=pvar->iVar;
                    }
                }
                 else if(pvar->dataType==3)
                {
                    float temp=pvar->fVar*(pvar->storeData/100.00);
                    if( abs(pvar->fVar-pvar->hfVar)>=temp)
                    {
                        re=1;
                        pvar->hfVar=pvar->fVar;
                    }
                }
                else if(pvar->dataType==4)
                {
                    if( pvar->iVar!= pvar->hiVar)
                    {
                        re=1;
                        pvar->hiVar=pvar->iVar;
                    }
                }
                else
                    re=0;
            }
            else if(pvar->storeType==3)
            {
                if(pvar->dataType==1)
                {
                    unsigned int temp=pvar->storeData;
                    if(abs(pvar->uiVar-pvar->huiVar)>=temp)
                    {
                        re=1;
                        pvar->huiVar=pvar->uiVar;
                    }
                }
                else if(pvar->dataType==2)
                {
                    int temp=pvar->storeData;
                    if( abs(pvar->iVar-pvar->hiVar)>=temp)
                    {
                        re=1;
                        pvar->hiVar=pvar->iVar;
                    }
                }
                 else if(pvar->dataType==3)
                {
                    float temp=pvar->storeData;
                    if( abs(pvar->fVar-pvar->hfVar)>=temp)
                    {
                        re=1;
                        pvar->hfVar=pvar->fVar;
                    }
                }
                else if(pvar->dataType==4)
                {
                    if( pvar->iVar!= pvar->hiVar)
                    {
                        re=1;
                        pvar->hiVar=pvar->iVar;
                    }
                }
                else
                    re=0;
            }
            else if(pvar->storeType==4)
            {
                gettimeofday(&start,0);
                timeval_subtract(&diff,&(pvar->data_Compare_Time),&start);
                if(diff.tv_sec*1000 + diff.tv_usec/1000 >= pvar->storeData)
                {
                    gettimeofday(&pvar->data_Compare_Time,0);
                    re=1;
                }
            }
        }
    }
    return re;
}


//数据存储函数
void MyFileWrite(int &filed,char* filePath,char* buffer,int len)
{
    static char forwordBuf[1024];
    int cacheListItemsMax = 50;
    bool isSDcardReadOnly=false;
#if ENABLE_ZIP_STORE
    if(filed==0)
        filed=open(filePath,O_RDWR|O_CREAT);
    if(filed)
        write(filed,buffer,len);
#endif

    if(strlen(forwordBuf) + len < sizeof(forwordBuf)
            && false == g_onFlushMemCache){
        strcat(forwordBuf, buffer);
    }
    else
    {
        RtDateList.rtlMutex.lock();

        if(RtDateList.list.count() > cacheListItemsMax)
        {
            sdCardRWMutex.lock();

            MyWriteLog("WARNING: send buffer full.");
            QDir dir;
            QString rtCachePath = RT_CACHE_PATH;
            if(dir.exists(rtCachePath) || dir.mkpath(rtCachePath))
            {
                QDateTime dTime = QDateTime::currentDateTime();
                rtCachePath += dTime.toString("yyyyMMdd") + "/";

                if(dir.exists(rtCachePath) || dir.mkpath(rtCachePath))
                {
                    QString fileName = rtCachePath;
                    fileName += dTime.toString("yyyyMMddhhmmss.zzz");

                    QFile rtCachefile(fileName);
                    if(rtCachefile.open(QIODevice::WriteOnly))
                    {
                        QTextStream out(&rtCachefile);
                        foreach (char* listData, RtDateList.list) {
                            out << listData;
                            delete listData;
                        }
                        RtDateList.list.clear();
                        rtCachefile.close();
                    }
                    else
                    {
                        rtCachefile.close();
                        if(EROFS == errno)
                            isSDcardReadOnly = true;
                        MyWriteLog(sysErrInfo,"ERROR: create rtCachefile %s failed: %s.%d", fileName.toStdString().c_str(), strerror(errno),errno);
                    }
                }else{
                    if(EROFS == errno)
                        isSDcardReadOnly = true;
                    MyWriteLog(sysErrInfo,"ERROR: create rtdata cache subdirectory %s failed.", rtCachePath.toStdString().c_str());                 
                }

            }
            else{
                if(EROFS == errno)
                    isSDcardReadOnly = true;
                MyWriteLog(sysErrInfo,"ERROR: create rtdata cache directory %s failed.", rtCachePath.toStdString().c_str());
            }
            sdCardRWMutex.unlock();
        }

        if(RtDateList.list.count() <= cacheListItemsMax
                && 0 != strlen(forwordBuf))
        {
            //qDebug() << "to list " << forwordBuf;
            char *p = strdup(forwordBuf);
            RtDateList.list.push_back(p);
        }
        RtDateList.rtlMutex.unlock();
        strcpy(forwordBuf, buffer);

        if(true == g_onFlushMemCache)
            g_onFlushMemCache = false;
    }

    if(isSDcardReadOnly)
    {
        sdErrReadOnlyHandler();
    }

}

//数据存储函数
void statsVarFileWrite(char* filePath,char* buffer,int len)
{
    char forwordBuf[1024]={0};
    int cacheListItemsMax = 50;
    bool isSDcardReadOnly=false;
#if ENABLE_ZIP_STORE
    if(filed==0)
        filed=open(filePath,O_RDWR|O_CREAT);
    if(filed)
        write(filed,buffer,len);
#endif
    if(sizeof(forwordBuf) < len)
        return ;

    {
        statsRtDateList.rtlMutex.lock();

        if(statsRtDateList.list.count() > cacheListItemsMax)
        {
            sdCardRWMutex.lock();

            MyWriteLog("WARNING: stats send buffer full.");
            QDir dir;
            QString rtCachePath = filePath;
            if(dir.exists(rtCachePath) || dir.mkpath(rtCachePath))
            {
                QDateTime dTime = QDateTime::currentDateTime();
                rtCachePath += dTime.toString("yyyyMMdd") + "/";

                if(dir.exists(rtCachePath) || dir.mkpath(rtCachePath))
                {
                    QString fileName = rtCachePath;
                    fileName += dTime.toString("yyyyMMddhhmmss.zzz");

                    QFile rtCachefile(fileName);
                    if(rtCachefile.open(QIODevice::WriteOnly))
                    {
                        QTextStream out(&rtCachefile);
                        foreach (char* listData, statsRtDateList.list) {                            
                            out << listData;
                            delete listData;
                            out.flush();
                        }
                        statsRtDateList.list.clear();
                        rtCachefile.close();
                    }
                    else
                    {
                        rtCachefile.close();
                        MyWriteLog(sysErrInfo,"ERROR: create statsVar cachefile %s failed: %s.", fileName.toStdString().c_str(), strerror(errno));
                        isSDcardReadOnly = true;
                    }
                }else{
                    MyWriteLog(sysErrInfo,"ERROR: create statsVar rtdata cache subdirectory %s failed.", rtCachePath.toStdString().c_str());
                    isSDcardReadOnly = true;
                }

            }
            else{
                MyWriteLog(sysErrInfo,"ERROR: create rtdata cache directory %s failed.", rtCachePath.toStdString().c_str());
                isSDcardReadOnly = true;
            }
            sdCardRWMutex.unlock();
        }

        if(statsRtDateList.list.count() <= cacheListItemsMax
                && 0 != strlen(buffer))
        {
            strcat(forwordBuf, buffer);
            strcat(forwordBuf, "packageEnd.\r\n");
            //qDebug() << "to list " << statsforwordBuf;
            char *p = strdup(forwordBuf);
            statsRtDateList.list.push_back(p);
        }
        statsRtDateList.rtlMutex.unlock();
        //strcpy(statsforwordBuf, buffer);

        if(true == g_onFlushMemCache)
            g_onFlushMemCache = false;
    }

    if(isSDcardReadOnly)
    {
        sdErrReadOnlyHandler();
    }

}
//存储线程
void* Store_Thread(void* arg)
{
    char buffer[1024];
    char buf[1024];
    int filed=0;

    int fd=-1;
    //fd=open("/media/mmcblk0p1/Store_Thread",O_RDWR|O_CREAT|O_APPEND);
    MyWriteLog(sysExecFlow,"%s: threadid: %d.", __func__, pthread_self());

    MyWriteLog(sysExecFlow,"Store_Thread Run.");
    varConfig* pvar= p_varConfig;
    long hs=0;
    time_t now;
    struct tm *timenow;
    char mypath[254];
    time_t t;
    struct tm * a;
    struct timeval start,diff,endtime;
    struct timeval start1,diff1,endtime1;
    gettimeofday(&endtime,0);
    gettimeofday(&endtime1,0);
    unsigned int testVarInc = 0;
#if ENABLE_ZIP_STORE
    char *zipFileDirpath1 = "/media/mmcblk0p1/DataFile1/";
    char *zipFileDirpath2 = "/media/mmcblk0p1/DataFile1/";
    if(mkdir(zipFileDirpath1, 0755) < 0 && EEXIST != errno)
    {
        MyWriteLog("ERROR: ZIPFile dir create failed: %s.", zipFileDirpath);
        return NULL;
    }
    if(mkdir(zipFileDirpath2, 0755) < 0 && EEXIST != errno)
    {
        MyWriteLog("ERROR: ZIPFile dir create failed: %s.", zipFileDirpath);
        return NULL;
    }
#endif
    while(pvar!=NULL)
    {
        if(NULL == pvar->memoryData)
        {
            MyWriteLog(sysErrInfo, "ERROR: var [%d %s] configure invalid! please check var configure.",
                       pvar->myid,pvar->varName);
        }
        pvar=pvar->next;
    }
    while(1)
    {
        time(&t);
        a=localtime(&t);
#if ENABLE_ZIP_STORE
        memset(mypath,0,254);
        sprintf(mypath,"/media/mmcblk0p1/DataFile%d/%02d%02d%02d%02d%02d%02d%d.txt",g_ftpConfig.isRun+1,1900+a->tm_year,1+a->tm_mon,a->tm_mday,a->tm_hour,a->tm_min,a->tm_sec,g_ftpConfig.fileNum);
        if(filed==0)
            filed=open(mypath,O_RDWR|O_CREAT);
#endif
        pvar = p_varConfig;
        while(pvar!=NULL)
        {
            if(NULL != pvar->memoryData)
            {
                channelConfig* pchal = p_channelConfig;
                while(NULL != pchal && pchal->channelID != pvar->channelID)
                    pchal=pchal->next;
#if 0 //!通道失败超限策略先关闭，需要再打开
                if(pchal->statsReadFailed > 50){
                    pvar=pvar->next;
                    //MyWriteLog("WARN: var %d, chanl %d statsReadFailed!", pvar->myid, pvar->channelID);
                    continue;
                }
#endif
                memset(buffer,0x00,1024);
                if(zgzmywritefile(pvar,buffer)==1)//返回1 写入文件，返回0，不写
                {
#if ENABLE_ZIP_STORE
                    if(hs==0)  //第一行，时间及变量个数
                    {
                        memset(buf,0x00,1024);
                        time(&now);
                        timenow=localtime(&now);
                        sprintf(buf,"%02d-%02d-%02d,000001\r\n",1900+timenow->tm_year,1+timenow->tm_mon,timenow->tm_mday);
                        write(filed,buf,strlen(buf));
                        hs++;
                    }
#endif
                    MyFileWrite(filed,mypath,buffer,strlen(buffer));//数据存储函数
                    if(infoDbgLevel & 2)//调试
                    {
                        char *chanlTypeStr  = getChanlTypeStr(pchal->devDataType);
                        int varOffsetAddr=0;
                        if(pchal->devDataType>=4 && pchal->devDataType<=7)                    {
                            varOffsetAddr = pchal->devGeneralAdder + pvar->memoryBeginAdder/2;
                        }
                        else{
                            varOffsetAddr = pchal->devGeneralAdder + pvar->memoryBeginAdder;
                        }
                        sprintf(buffer+strlen(buffer)-2, ",ch%d,%s%d%c%d b%d,%d(B)",
                                pvar->channelID, chanlTypeStr,
                                pchal->devBeginAdder,
                                pchal->devBeginAdder == 0?'\b':'.',
                                varOffsetAddr,
                                pvar->dataType == 4?pvar->dataBit:0,
                                pvar->memoryDataLeng);
                        MyWriteLog(buffer);
                    }
                    hs++;
                }
            }
            pvar=pvar->next;
        }
#if ENABLE_ZIP_STORE
        gettimeofday(&start1,0);
        timeval_subtract(&diff1,&endtime1,&start1);
        if(diff1.tv_sec>=5)
        {
            gettimeofday(&endtime1,0);
            /*
            memset(buf,0x00,1024);
            sprintf(buf,"new file-%d",diff1.tv_sec);
            MyDebug(buf);
            */
            if(filed>0)
            {
                g_ftpConfig.fileNum++;
                lseek(filed,0,SEEK_SET);
                memset(buffer,0x00,1024);
                time(&now);
                timenow=localtime(&now);
                sprintf(buffer,"%02d-%02d-%02d,%06ld\r\n",1900+timenow->tm_year,1+timenow->tm_mon,timenow->tm_mday,hs);
                write(filed,buffer,strlen(buffer));
                close(filed);
                usleep(10000);
                filed=0;
                hs=0;
            }
        }

        //判断是否需要压缩文件
        gettimeofday(&start,0);
        timeval_subtract(&diff,&endtime,&start);
        if(diff.tv_sec+5>=g_ftpConfig.uploadTime&&g_ftpConfig.isZIP==0)
        {
            if(g_ftpConfig.isRun==0)
                g_ftpConfig.isRun=1;
            else
                g_ftpConfig.isRun=0;
            g_ftpConfig.fileNum=0;

            if(filed>0)
            {
                g_ftpConfig.fileNum++;
                lseek(filed,0,SEEK_SET);
                memset(buffer,0x00,1024);
                time(&now);
                timenow=localtime(&now);
                sprintf(buffer,"%02d-%02d-%02d,%06ld\r\n",1900+timenow->tm_year,1+timenow->tm_mon,timenow->tm_mday,hs);
                write(filed,buffer,strlen(buffer));
                close(filed);
                usleep(10000);
                filed=0;
                hs=0;
            }

            g_ftpConfig.isZIP=1;
            gettimeofday(&endtime,0);

            //MyDebug("begin ZIP");
        }
#endif
        usleep(10000);
    }
    if(fd>0)
    {
        close(fd);
    }
    MyWriteLog(sysExecFlow,"%s: threadid: %d exit.", __func__, pthread_self());
    return arg;
}
// callback function
int on_extract_entry(const char *filename, void *arg) {
    static int i = 0;
    int n = *(int *)arg;
    MyWriteLog("Extracted: %s (%d of %d).", filename, ++i, n);

    return 0;
}

int createZipFile(char* sourceFilepath, char* zipFileName)
{
    if(NULL == sourceFilepath || NULL == zipFileName)
        return -1;

    /*
       Create a new zip archive with default compression level (6)
    */
    struct zip_t *zip = zip_open(zipFileName, ZIP_DEFAULT_COMPRESSION_LEVEL, 0);
    if(zip)
    {
        QDir dir(sourceFilepath);
        dir.setFilter(QDir::Files);
        QFileInfoList list = dir.entryInfoList();
        foreach (QFileInfo fileInfo, list)
        {
            if(fileInfo.isFile())
            {
                zip_entry_open(zip, fileInfo.fileName().toStdString().c_str());
                zip_entry_fwrite(zip, fileInfo.filePath().toStdString().c_str());
                zip_entry_close(zip);
            }
        }
        zip_close(zip);
    }else{
        return -1;
    }

    return 0;
}
#if 0
int Myzipfile(char* filepath) {
    /*
       Create a new zip archive with default compression level (6)
    */
    struct zip_t *zip = zip_open("foo.zip", ZIP_DEFAULT_COMPRESSION_LEVEL, 0);
    // we should check if zip is NULL
    {
        zip_entry_open(zip, "foo-1.txt");
        {
            char *buf = "Some data here...";
            zip_entry_write(zip, buf, strlen(buf));
        }
        zip_entry_close(zip);

        zip_entry_open(zip, "foo-2.txt");
        {
            // merge 3 files into one entry and compress them on-the-fly.
            zip_entry_fwrite(zip, "foo-2.1.txt");
            zip_entry_fwrite(zip, "foo-2.2.txt");
            zip_entry_fwrite(zip, "foo-2.3.txt");
        }
        zip_entry_close(zip);
    }
    // always remember to close and release resources
    zip_close(zip);


    /*
        Extract a zip archive into /tmp folder
    */
    int arg = 2;
    zip_extract("foo.zip", "/tmp", on_extract_entry, &arg);

    return 0;
}
#endif
unsigned long get_file_size(const char *path)
{
    unsigned long filesize = -1;
    struct stat statbuff;
    if(stat(path, &statbuff) < 0){
        return filesize;
    }else{
        filesize = statbuff.st_size;
    }
    return filesize;
}
int mylistDir(char *path)
{
    DIR              *pDir ;
    struct dirent    *ent  ;
    char              childpath[512];

    pDir=opendir(path);
    if(NULL == pDir)
    {
        MyWriteLog("ERROR: open dir %s failed: %s", path, strerror(errno));
        return 1;
    }
    memset(childpath,0,512);

    struct timeval temp_Time;

    unsigned long tt=0;
    unsigned long zipfilesize=0;
    int sftc=0;
    time_t t;
    struct tm * a;
    time(&t);
    a=localtime(&t);
    gettimeofday(&temp_Time,0);
    sprintf(childpath,"/media/mmcblk0p1/ZIPFile/%02d%02d%02d/",1900+a->tm_year,1+a->tm_mon,a->tm_mday);
    if(mkdir(childpath, 0755) < 0 && EEXIST != errno)
    {
        MyWriteLog("ERROR: zip dir create failed: %s.", childpath);
        return 1;
    }
    sprintf(childpath+strlen(childpath),"%02d%02d%02d%02d%02d%02d%ld.zip",1900+a->tm_year,1+a->tm_mon,a->tm_mday,
            a->tm_hour,a->tm_min,a->tm_sec,temp_Time.tv_usec/1000);
    struct zip_t *zip = zip_open(childpath, ZIP_DEFAULT_COMPRESSION_LEVEL, 0);
    if(NULL == zip)
    {
        MyWriteLog("ERROR: open zip file %s failed.", childpath);
        return 1;
    }
    while((ent=readdir(pDir))!=NULL)
    {
        if(ent->d_type & DT_DIR)
        {
            continue;
        }
        else
        {
            if(zip!=NULL)
            {
                memset(childpath,0,512);
                sprintf(childpath,"%s/%s",path,ent->d_name);
                //获得文件大小，如果为0K直接删除文件
                tt=get_file_size(childpath);
                if(tt>0)
                {
                    zipfilesize+=tt/1024;
                    memset(childpath,0,512);
                    sprintf(childpath,"%s",ent->d_name);
                    zip_entry_open(zip, childpath);
                    memset(childpath,0,512);
                    sprintf(childpath,"%s/%s",path,ent->d_name);
                    zip_entry_fwrite(zip, childpath);
                    zip_entry_close(zip);
                }
            }
            usleep(10000);
            memset(childpath,0,512);
            sprintf(childpath,"%s/%s",path,ent->d_name);
            remove(childpath);
            if(zipfilesize>230)
            {
                sftc=1;
                break;
            }
        }
    }
    if(zip!=NULL)
    {
        zip_close(zip);
    }
    closedir(pDir);
    return sftc;

}
#if 0
//FileZIP_Thread
void* FileZIP_Thread(void* arg)
{
    int fd;
    char buffer[1024];
    char mypath[512];
    char *zipFileDirpath = "/media/mmcblk0p1/ZIPFile/";

    fd=open("/media/mmcblk0p1/FileZIP_Thread",O_RDWR|O_CREAT|O_APPEND);
    if(fd==-1)
        printf("open FileZIP_Thread Error");
    memset(buffer,0x00,1024);
    strcpy(buffer,"open FileZIP_Thread Run");
    MyWriteLog(fd,buffer);
    MyWriteLog("%s: threadid: %d.", __func__, pthread_self());

    if(mkdir(zipFileDirpath, 0755) < 0 && EEXIST != errno)
    {
        MyWriteLog("ERROR: ZIPFile dir create failed: %s.", zipFileDirpath);
        return NULL;
    }

    while(1)
    {
        if(g_ftpConfig.isZIP==1)
        {
            memset(mypath,0,512);
            if(g_ftpConfig.isRun==0)
                strcpy(mypath,"/media/mmcblk0p1/DataFile2/");
            else
                strcpy(mypath,"/media/mmcblk0p1/DataFile1/");

            if( mylistDir(mypath)==0)
                g_ftpConfig.isZIP=0;
            usleep(500000);
        }
        sleep(1);
    }

    memset(buffer,0x00,1024);
    strcpy(buffer,"exit FileZIP_Thread Run");
    MyWriteLog(fd,buffer);
    if(fd>0)
    {
        close(fd);
    }
    return arg;
}
#endif
int myftplistDir(int sockfd,char *path)
{
    int re=1;
    DIR              *pDir ;
    struct dirent    *ent  ;
    char              childpath[512], date[12];
    time_t now;
    struct tm *timenow;
    time(&now);
    timenow=localtime(&now);
    strftime (date,12,"%Y%m%d",timenow );

    pDir=opendir(path);
    if(NULL == pDir){
        MyWriteLog("ERROR: open ftp dir %s failed. %s", path, strerror(errno));
        exit(-1);
    }

    while((ent=readdir(pDir))!=NULL)
    {
        if(ent->d_type & DT_DIR)
        {
            if(!strcmp(".", ent->d_name) || !strcmp("./", ent->d_name)
                    || !strcmp("..", ent->d_name) || !strcmp("../", ent->d_name))
                continue;
            sprintf(childpath,"%s/%s",path,ent->d_name);

            if(myftplistDir(sockfd, childpath) && strcmp(date, ent->d_name))
                rmdir(childpath);
            else
                return 0;
        }
        else
        {
            memset(childpath,0,512);
            sprintf(childpath,"%s/%s",path,ent->d_name);

            if(UpFile(sockfd,childpath,ent->d_name,g_ftpConfig.serverIP,g_ftpConfig.lockIP)==1)
            {
                usleep(10000);
                remove(childpath);
            }
            else
            {
                CloseSocketConnect(sockfd);
                sockfd=0;
                re = 0;
                break;
            }
            re=1;
        }
    }
    closedir(pDir);
    return re;
}

void* FTP_uploadFileThread(void* arg)
{
    int index=0, ret = 0;
    int dateNum, startDataNum = 0;  //初始化，增强可移植性 --liut 171010
    int sockfd=0;
    MyWriteLog(sysExecFlow,"%s: threadid: %d.%x.", __func__, pthread_self(), arg);

    //QString dateTime;
    //dateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd");

    QString startAt;
    QString upgradeCmd = QString((char*)arg);

    QString ftpUser = upgradeCmd.section(":",1,1).mid(2,-1);
    QString ftpPassword = upgradeCmd.section(":",2,2).section("@",0,0);
    QString ftpIp = upgradeCmd.section(":",2,2).section("@",1,1).section("?", 0, 0);
    QString ftpCmd = upgradeCmd.section("?",1,1).mid(4,-1);
    qDebug()<<ftpUser<<ftpPassword<<ftpIp<<ftpCmd;

    QDir dir(SYS_LOG_PATH);
    dir.setFilter(QDir::Files);
    dir.setSorting(QDir::Name);

    QFileInfoList list = dir.entryInfoList();
    if(0 == list.count()){
        MyWriteLog("ERROR: no valid logFile. FTP_uploadFileThread exit.");
        goto ftpupload_exit;
    }
    if(ftpCmd.contains("startAt:"))
    {
        startAt = ftpCmd.mid(7,-1);
        startDataNum = startAt.section(":",1,1).section("-", 0, 0).toUInt()*10000; //1000改为10000 --liut 171010
        startDataNum += startAt.section(":",1,1).section("-", 1, 1).toUInt()*100;
        startDataNum += startAt.section(":",1,1).section("-", 2, 2).toUInt();
    }
    if(ftpUser.isEmpty() || ftpPassword.isEmpty() || ftpIp.isEmpty() || ftpCmd.isEmpty()){
        MyWriteLog("ERROR: ftp param empty. FTP_uploadFileThread exit.");
        goto ftpupload_exit;
    }

    g_isStopFtpUpload = false;

    while(index<list.count())
    {
        if(g_isStopFtpUpload)
            break;

        QFileInfo fileInfo = list.at(index);
        if(fileInfo.fileName().contains("-"))
        {
            dateNum = fileInfo.fileName().section(".", 2,2).section("-", 0, 0).toUInt()*10000;//1000改为10000 --liut 171010
            dateNum += fileInfo.fileName().section(".", 2,2).section("-", 1, 1).toUInt()*100;
            dateNum += fileInfo.fileName().section(".", 2,2).section("-", 2, 2).toUInt();
        }
        else
            dateNum = 0xfffffff;//！设置最大值

        if(dateNum < startDataNum){
            index++;
            continue;
        }
        if(sockfd==0)
        {
            if(InitSocketConnect(sockfd,g_ftpConfig.lockIP,ftpIp.toStdString().c_str(),21)>0)
            {
                MyWriteLog("FTP net connect successful.");
                if(FTPLogin(sockfd,ftpUser.toStdString().c_str(),ftpPassword.toStdString().c_str())==1)
                {
                    MyWriteLog("FTP login successful.");
                }
                else
                {
                    MyWriteLog("ERROR: FTP login %s@%s:%s failed.", ftpUser.toStdString().c_str(),ftpPassword.toStdString().c_str(),ftpIp.toStdString().c_str());
                    CloseSocketConnect(sockfd);
                    sockfd=0;
                    sleep(2);
                }
            }
            else
            {
                MyWriteLog("FTP connect failed.");
                sockfd=0;
                sleep(2);
            }
        }

        if(sockfd!=0)
        {
            char zipLogfilePath[128]={0};
            char zipLogfilename[128]={0};
            sprintf(zipLogfilename, "%s.zip", fileInfo.fileName().toStdString().c_str());
            sprintf(zipLogfilePath,"/tmp/%s", zipLogfilename);
            struct zip_t *zip = zip_open(zipLogfilePath, ZIP_DEFAULT_COMPRESSION_LEVEL, 0);
            if(NULL == zip)
            {
                MyWriteLog("ERROR: open zip file %s failed.", zipLogfilePath);
                return NULL;
            }
            int ret = zip_entry_open(zip, fileInfo.fileName().toStdString().c_str());
            ret = zip_entry_fwrite(zip, fileInfo.filePath().toStdString().c_str());

            zip_entry_close(zip);
            zip_close(zip);
            MyWriteLog("logUpload: %d, %s. start.", index, fileInfo.filePath().toStdString().c_str());
            if(UpFile(sockfd,zipLogfilePath,zipLogfilename,ftpIp.toStdString().c_str(),g_ftpConfig.lockIP)==1)
            {
                MyWriteLog("logUpload: %d, %s. success.", index, fileInfo.filePath().toStdString().c_str());
                index++;
            }
            else{
                MyWriteLog("logUpload: %d, %s. failed.", index, fileInfo.filePath().toStdString().c_str());
                sleep(1);
            }
            if(FTPMyIsOline(sockfd)==0)
            {
                CloseSocketConnect(sockfd);
                sockfd=0;
            }
        }

    }

ftpupload_exit:
    if(sockfd)
    {
        FTPQuit(sockfd);
        CloseSocketConnect(sockfd);
        sockfd=0;
    }
    g_isStopFtpUpload = true;
    MyWriteLog("%s: threadid: %d exit.", __func__, pthread_self());
    return NULL;
}
#if 0
void* FTP_Thread(void* arg)
{
    int fd;
    char buffer[1024];
    char mypath[512];
    int sockfd=0;
    MyWriteLog("%s: threadid: %d.", __func__, pthread_self());

    fd=open("/media/mmcblk0p1/FTP_Thread",O_RDWR|O_CREAT|O_APPEND);
    if(fd==-1)
        printf("open FTP_Thread Error.");
    memset(buffer,0x00,1024);
    strcpy(buffer,"open FTP_Thread Run.");
    MyWriteLog(fd,buffer);

    struct timeval start,end,diff;
    gettimeofday(&start,0);
    gettimeofday(&end,0);

    while(1)
    {
        gettimeofday(&start,0);
        timeval_subtract(&diff,&end,&start);
        if(diff.tv_sec>=g_ftpConfig.uploadTime)
        {
            if(g_ftpConfig.isZIP==0)
            {
                //MyDebug("begin ftp");
                 gettimeofday(&end,0);
                if(sockfd==0)
                {
                    if(InitSocketConnect(sockfd,g_ftpConfig.lockIP,g_ftpConfig.serverIP,g_ftpConfig.uploadNum)>0)
                    {
                        MyWriteLog("FTP net connect successful.");
                        if(FTPLogin(sockfd,g_ftpConfig.ftpUser,g_ftpConfig.ftpPass)==1)
                        {
                            MyWriteLog("FTP login successful.");
                        }
                        else
                        {
                            MyWriteLog("ERROR: FTP login %s@%s:%s failed.", g_ftpConfig.ftpUser,g_ftpConfig.ftpPass, g_ftpConfig.serverIP);
                            CloseSocketConnect(sockfd);
                            sockfd=0;
                            sleep(2);
                        }
                    }
                    else
                    {
                        MyWriteLog("FTP connect failed.");
                        sockfd=0;
                        sleep(2);
                    }
                }
                memset(mypath,0,512);
                strcpy(mypath,"/media/mmcblk0p1/ZIPFile/");
                if(sockfd!=0&&myftplistDir(sockfd,mypath)==0)
                {
                    if(FTPMyIsOline(sockfd)==0)
                    {
                        CloseSocketConnect(sockfd);
                        sockfd=0;
                    }
                }
            }
        }
        sleep(1);
    }

    memset(buffer,0x00,1024);
    strcpy(buffer,"exit FTP_Thread Run");
    MyWriteLog(fd,buffer);
    if(fd>0)
    {
        close(fd);
    }
    return arg;
}
#endif
int myDelleteAlllistDir(char *path)
{
    int re=0;
    DIR              *pDir ;
    struct dirent    *ent  ;
    char              childpath[512];

    pDir=opendir(path);

    while((ent=readdir(pDir))!=NULL)
    {

        if(ent->d_type & DT_DIR)
        {
            continue;
        }
        else
        {
            memset(childpath,0,512);
            sprintf(childpath,"%s/%s",path,ent->d_name);
            remove(childpath);
            re=1;
        }
    }
    closedir(pDir);
    return re;
}


//TCP业务处理线程
int Analysis(void* sfd, u_char* buffer,int datalen,int& analysised)
{
    static int sloopFlag;
    static int packageTotalNum, packageCurNum;
    static int uploadCfgFileOffset,uploadPackageTotalNum,uploadPackageCurNum;
    char buf[128];
    int *pfd, sockfd=0,ret=0, packageTotalLen;
    char mcd[32];
    quint8 cmdRet = 1, cmdRetry=0;
    pfd = &sockfd;
    kdIotApdu *pRdApdu = (kdIotApdu*)buffer;

    if(NULL == sfd)
    {
        MyWriteLog("ERROR: Analysis invalid socket.");
        return -2;
    }

    if(datalen < PACKAGE_HEAD_LENGTH || (pRdApdu->length <1024 && datalen < pRdApdu->length))
    {
        MyWriteLog(commRdFailed, "WARING: Analysis received incomplete package. length %d.", datalen);
        return -3;
    }
    if(pRdApdu->length > 1024)
    {
        MyWriteLog(commRdFailed, "WARING: Analysis received huge package. length %d.", pRdApdu->length);
        return -4;
    }
#if 1
    if(infoDbgLevel & 8){
        char dbgMsg[1024*5] = {0};
        if(datalen>1024*5)
            MyWriteLog("WARING: Analysis huge package, length %d >1024*5.", datalen);
        MyWriteLog("Analysis: %d bytes.", datalen);
        for(int i=0; i<datalen; i++)
            sprintf(dbgMsg+i*3, "%02x ", buffer[i]);
        MyWriteLog(dbgMsg);
    }
#endif
    pRdApdu->length = (pRdApdu->length);
    pRdApdu->packageNum = (pRdApdu->packageNum);

    packageTotalLen = pRdApdu->length + offsetof(kdIotApdu, packageNum);
    analysised = packageTotalLen;

    switch (pRdApdu->funcId) {
    case ACK_PACKAGE(heartBeat):
        ret = heartBeat;
        break;
    case reqVarTable:
        {
            QString varTab, varTabSec;
            if(0 == extractVarTable(varTab))
            {
                MyWriteLog("ERROR: extractVarTable failed.");
                return -2;
            }
            MyWriteLog("INFO: reqVarTable.");
            QStringList strSecList = varTab.split(";");

            while(cmdRetry < 3 && sloopFlag < strSecList.size())
            {
                varTabSec = strSecList.at(sloopFlag);

                varTabSec.insert(0, QString("%1\r\n%2\r\n").arg(strSecList.size()).arg(sloopFlag+1));
                //qDebug() << varTabSec.toStdString().data() ;

                ret = ctrlCmdSendMore(sfd, ACK_PACKAGE(reqVarTable),
                                        (u_char *)varTabSec.toStdString().data(), varTabSec.size(),
                                        cStringType, sloopFlag);
                //ret = ctrlCmdRecv(sfd);
                if(ret<0 && EAGAIN!=errno)
                {
                    break;
                }
                else if(ret >=0 ){
                   sloopFlag++;
                   cmdRetry = 0;
                }
                else{
                    cmdRetry += 1;
                }
                sleep(1);
                //usleep(500000);
            }
            sloopFlag = 0;
        }
        break;
    case ACK_PACKAGE(reqVarTable):
        if(sloopFlag != pRdApdu->packageNum)
            ret = -1;
        else
            ret = pRdApdu->packageNum;
        break;
    case correctTime:
        pRdApdu->dUnit.dataLength = (pRdApdu->dUnit.dataLength);
        if(6 != pRdApdu->dUnit.dataLength)
            goto aerr;
        MyWriteLog("INFO: correctTime.");
        setCorrectTime((quint8 *)pRdApdu->dUnit.dataPtr);

        while(cmdRetry++<3)
        {
            ret = ctrlCmdSend(sfd, ACK_PACKAGE(correctTime),NULL, 0);
            if((ret<0 && EAGAIN!=errno) || ret == 0)
                break;
            usleep(10000);
        }
        break;
    case ACK_PACKAGE(correctTime):
        pRdApdu->dUnit.dataLength = (pRdApdu->dUnit.dataLength);
        qDebug() << pRdApdu->dUnit.dataLength;
        if(6 != pRdApdu->dUnit.dataLength)
            goto aerr;
        MyWriteLog("INFO: correctTime.");
        //QDate d = QDate::currentDate();
        setCorrectTime((quint8 *)pRdApdu->dUnit.dataPtr);
        ret = correctTime;
        break;
    case ACK_PACKAGE(realDataUpload):
        ret = pRdApdu->packageNum;
        break;
    case ACK_PACKAGE(statisticsDataUpload):
        ret = pRdApdu->packageNum;
        break;
    case rebootSystem:
        MyWriteLog("INFO: rebootSystem.");
        while(cmdRetry++<3)
        {
            ret = ctrlCmdSend(sfd, ACK_PACKAGE(rebootSystem),NULL, 0);
            if((ret<0 && EAGAIN!=errno) || ret == 0)
                break;
            usleep(10000);
        }
        reboot(RB_AUTOBOOT);
        break;

    case deviceWrite:
        {
            if(NULL == pRdApdu->dUnit.dataPtr)
                goto aerr;
            MyWriteLog("INFO: deviceWrite.");
            QString funcData = (char*)pRdApdu->dUnit.dataPtr;
            funcData.resize(pRdApdu->dUnit.dataLength);       //解决deviceWrite命令粘包问题 --liut 20171025
            QStringList strSecList = funcData.split("\r\n");
            QListIterator<QString> itr(strSecList);
            varConfig* pvar= p_varConfig;
            cmdRet = 1;
            while(itr.hasNext())
            {
                QString varId, varNum;
                QString current=itr.next();

                MyWriteLog("WARING: remote write : %s", current.toStdString().c_str());
                QStringList strCmd = current.split(',');
                if(2 != strCmd.size())
                    continue;

                varId = strCmd.at(0);
                varNum = strCmd.at(1);
                while(pvar!=NULL && pvar->myid != varId.toInt()){
                    pvar=pvar->next;
                }

                if(pvar!=NULL && pvar->myid == varId.toInt())   //pvar!=NULL,防止段错误 --liut 171008
                {
                    switch(pvar->dataType)
                    {
                        //数据类型  1-无符号整数，2-有符号整数，3-小数，4-位
                        case 1:
                            pvar->wuiVar = varNum.toULong();
                            pvar->isWriteCmdValid = 1;
                            break;
                        case 2:
                        case 4:
                            pvar->wiVar = varNum.toInt();
                            pvar->isWriteCmdValid = 1;
                            break;
                        case 3:
                            pvar->wfVar = varNum.toFloat();
                            pvar->isWriteCmdValid = 1;
                            break;
                        default:
                            cmdRet = 0;
                            break;
                    }                    
                }
            }

            while(cmdRetry++<3)
            {
                ret = ctrlCmdSend(sfd, ACK_PACKAGE(deviceWrite),&cmdRet, 1);
                if((ret<0 && EAGAIN!=errno) || ret == 0)
                    break;
                usleep(10000);
            }
        }
        ret = deviceWrite;
        break;
    case setUpLoadInterval:
        int newUploadTime;
        if(NULL == pRdApdu->dUnit.dataPtr)
            goto aerr;
        newUploadTime = *(quint32*)pRdApdu->dUnit.dataPtr;
        if(newUploadTime > 6000)
            newUploadTime = 6000;
        else if(newUploadTime<0)
            newUploadTime = 1;
        g_ftpConfig.rtDatauploadTime = (newUploadTime);
        replaceConfigColumn("ftp.dll", 1, 1, newUploadTime);
        MyWriteLog("INFO: set rtDatauploadTime to %d s.", g_ftpConfig.rtDatauploadTime);

        cmdRet = 1;
        while(cmdRetry++<3)
        {
            ret = ctrlCmdSend(sfd, ACK_PACKAGE(setUpLoadInterval),NULL, 0);
            if((ret<0 && EAGAIN!=errno) || ret == 0)
                break;
            usleep(10000);
        }
        ret = setUpLoadInterval;
        break;
    case setVarAttribute:
        {
            if(NULL == pRdApdu->dUnit.dataPtr)
                goto aerr;
            MyWriteLog("INFO: receive setVarAttribute.");
            QString funcData = (char*)pRdApdu->dUnit.dataPtr;
            QStringList strSecList = funcData.split("\r\n");
            QListIterator<QString> itr(strSecList);

            cmdRet = true;
            while(itr.hasNext())
            {
                varConfig* pvar= p_varConfig;
                channelConfig* pchal = p_channelConfig;
                QString current=itr.next();

                MyWriteLog("WARING: setVarAttribute : %s", current.toStdString().c_str());
                QStringList strCmd = current.split(',');
                if(6 != strCmd.size()){
                    cmdRet = false;
                    continue;
                }
                QString varId = strCmd.at(0);
                QString varType = strCmd.at(1);
                QString varName = strCmd.at(2);
                QString varAddrName = strCmd.at(3);
                QString storeType = strCmd.at(4);
                QString storeData = strCmd.at(5);

                while(pvar!=NULL && pvar->myid != varId.toInt())
                    pvar=pvar->next;
                if(NULL == pvar){
                    cmdRet = false;
                    continue;
                }

                while(NULL != pchal && pchal->channelID != pvar->channelID)
                    pchal=pchal->next;
                if(NULL == pchal){
                    cmdRet = false;
                    continue;
                }

                strncpy(pvar->varName, varName.toStdString().c_str(), sizeof(pvar->varName));
                pvar->storeType = storeType.toUInt()==0?4:storeType.toUInt()%4;
                pvar->storeData = storeData.toUInt();

                if(varType.startsWith("INT"))
                {
                    pvar->dataType = 2;
                    pvar->memoryDataLeng = varType.mid(3,-1).toUInt()/8;
                }
                else if(varType.startsWith("UINT"))
                {
                   pvar->dataType = 1;
                   pvar->memoryDataLeng = varType.mid(4,-1).toUInt()/8;
                }
                else if(varType.startsWith("FLOAT"))
                {
                   pvar->dataType = 3;
                   pvar->memoryDataLeng = 4;
                }
                else if(varType.startsWith("BOOL"))
                {
                   pvar->dataType = 4;
                   pvar->memoryDataLeng = 1;
                }
                int varDataType;
                if(varAddrName.contains("DB", Qt::CaseInsensitive))
                    varDataType = varType_PLC_DB;
                else
                    continue;

                if(varAddrName == QString(pvar->varAddrName))
                {
                    MyWriteLog("WARING: varAddrName has not changed.");
                    continue;
                }
                MyWriteLog("WARING: %s -> %s.", pvar->varAddrName, varAddrName.toStdString().c_str());
                strncpy(pvar->varAddrName, varAddrName.toStdString().c_str(), sizeof(pvar->varAddrName));

                int addrParam1 = -1, addrParam2 = -1, addrParam3 = -1;
                if(varAddrName.contains("."))
                {
                   QString param1 = varAddrName.section(".", 0, 0);
                   QString param2 = varAddrName.section(".", 1, 1);
                   QString param3 = varAddrName.section(".", 2, 2);
                   qDebug()<<param1<<param2<<param3;
                   int pos = 0;
                   if(!param1.isEmpty()){
                       while (pos < param1.size() && false == param1.at(pos).isNumber()) pos++;
                       qDebug()<<pos<<param1.mid(pos,-1);
                       addrParam1 = param1.mid(pos,-1).toUInt();
                   }
                   if(!param2.isEmpty()){
                       pos = 0;
                       while (pos < param2.size() && false == param2.at(pos).isNumber()) pos++;
                       qDebug()<<pos<<param2.mid(pos,-1);
                       addrParam2 = param2.mid(pos,-1).toUInt();
                   }
                   if(!param3.isEmpty()){
                       pos = 0;
                       while (pos < param3.size() && false == param3.at(pos).isNumber()) pos++;
                       qDebug()<<pos<<param3.mid(pos,-1);
                       addrParam3 = param3.mid(pos,-1).toUInt();
                   }
                }
                else
                {
                   int pos = 0;
                   while (pos < varAddrName.size() && false == varAddrName.at(pos++).isNumber());
                   addrParam1 = varAddrName.mid(pos,-1).toUInt();
                }
                MyWriteLog("WARING: new varAddr : %d.%d.%d %dB.",
                           addrParam1,addrParam2,addrParam3<0?0:addrParam3,
                           pvar->memoryDataLeng);

                pchal = getChanlConfig(addrParam1, addrParam2, pvar->memoryDataLeng);
                if(NULL == pchal)
                {
                    MyWriteLog("WARING: can't find valid channel.");
                    cmdRet = false;
                    continue;
                }

                pvar->channelID = pchal->channelID;
                pvar->memoryBeginAdder = addrParam2 - pchal->devGeneralAdder;
                pvar->memoryData = pchal->dataBuf+pvar->memoryBeginAdder;
                if(addrParam3>0)
                    pvar->dataBit = addrParam3%7;

                MyWriteLog("WARING: new var to chanl %d : %d,%d",
                           pchal->channelID, pvar->memoryBeginAdder, pvar->memoryDataLeng);

            }
            flushVarToFile();

            while(cmdRetry++<3)
            {
                ret = ctrlCmdSend(sfd, ACK_PACKAGE(setVarAttribute),&cmdRet, 1);
                if((ret<0 && EAGAIN!=errno) || ret == 0)
                    break;
                usleep(10000);
            }
        }
        ret = setVarAttribute;
        break;
    case firmwareUpgrade:
        {
            if(NULL == pRdApdu->dUnit.dataPtr)
                goto aerr;
            QString upgradeCmd = (char*)pRdApdu->dUnit.dataPtr;
            MyWriteLog("INFO: receive firmwareUpgrade cmd: %s.", pRdApdu->dUnit.dataPtr);

            if(upgradeCmd.startsWith("ftp://") && 3 == upgradeCmd.split(":").count())
            {
                cmdRet = 1;
                QString ftpUser = upgradeCmd.section(":",1,1).mid(2,-1);
                QString ftpPassword = upgradeCmd.section(":",2,2).section("@",0,0);
                QString ftpIp = upgradeCmd.section(":",2,2).section("@",1,1);

                QString ftpFile = "TZfirware";

                //格式 ftp://user:123456@211.142.23.107/xxx.zip
                if(upgradeCmd.split("/").count()==4)
                {
                    ftpFile = upgradeCmd.section("/",-1,-1);

                    ftpIp = upgradeCmd.section(":",2,2).section("@",1,1).section("/",0,0);
                }

                qDebug()<<ftpIp<<ftpUser<<ftpPassword<<ftpFile;
                if(InitSocketConnect(sockfd,g_ftpConfig.lockIP,ftpIp.toStdString().c_str(),21)>0)
                {
                    MyWriteLog("Analysis-FTP net connect successful.");
                    if(FTPLogin(sockfd,ftpUser.toStdString().c_str(),ftpPassword.toStdString().c_str())==1)
                    {
                        MyWriteLog("Analysis-FTP login successful.");

                        if(MyDownLoad(sockfd,(char*)ftpFile.toStdString().c_str(),g_ftpConfig.lockIP,ftpIp.toStdString().c_str())==1)
                        {
                            QString cmd_temp = QString("cd /tmp;rm /tmp/update -rf;unzip %1;cd update;chmod a+x *.sh;./up.sh").arg(ftpFile);

                            ret = system(cmd_temp.toStdString().c_str());

                            CloseSocketConnect(sockfd);
                            sockfd=0;
                        }
                    }
                    else
                    {
                        MyWriteLog("ERROR: Analysis-FTP login failed.");
                        CloseSocketConnect(sockfd);
                        sockfd=0;
                        cmdRet=0;
                    }
                }
                else
                {
                    MyDebug("Analysis-FTP connect fail");
                    sockfd=0;
                    cmdRet=0;
                }
            }
            else{
                cmdRet = 0;
            }
        }
        MyWriteLog("INFO: firmwareUpgrade send reply.");
        ctrlCmdSend(sfd, ACK_PACKAGE(firmwareUpgrade), &cmdRet, 1);
        ret = firmwareUpgrade;
        break;

    case uploadlogFile:
        {
            if(NULL == pRdApdu->dUnit.dataPtr)
                goto aerr;
            QString uploadLogCmd = (char*)pRdApdu->dUnit.dataPtr;
            MyWriteLog("INFO: receive uploadlogFile: %s.", pRdApdu->dUnit.dataPtr);

            if(uploadLogCmd.contains("stop"))
            {
                g_isStopFtpUpload = true;   //若线程已存在，则会自动结束 --liut 171010
            }
            else if(g_isStopFtpUpload)
            {
                pthread_attr_t attr;
                pthread_t th_ftp;

                pthread_attr_init(&attr);
                pthread_attr_setdetachstate(&attr,   PTHREAD_CREATE_DETACHED);
                pthread_create(&th_ftp, &attr, FTP_uploadFileThread, (void*)pRdApdu->dUnit.dataPtr);
                while(cmdRetry++<3)
                {
                    if(false == g_isStopFtpUpload)
                        break;
                    usleep(10000);
                }
            }
        }
        ctrlCmdSend(sfd, ACK_PACKAGE(uploadlogFile),NULL, 0);
        ret = uploadlogFile;
        break;
    case remoteCmdStdout:
        {
            if(NULL == pRdApdu->dUnit.dataPtr)
                goto aerr;

            QString remoteCmd = (char*)pRdApdu->dUnit.dataPtr;
            MyWriteLog("INFO: receive remoteCmd: %s.", pRdApdu->dUnit.dataPtr);

            if(remoteCmd.contains("rebootCmd="))
            {
                QFile qf("/opt/rebootCmd");
                if(qf.open(QIODevice::WriteOnly))
                {
                    QTextStream out(&qf);
                    out << remoteCmd.mid(10,-1);
                }
                ctrlCmdSend(sfd, ACK_PACKAGE(remoteCmdStdout),NULL, 0);
            }
            if(remoteCmd.contains("dbglevel="))
            {
                int dbglevel = remoteCmd.mid(9,-1).toUInt();
                infoDbgLevel = dbglevel;
                MyWriteLog("change infoDbgLevel: %d",infoDbgLevel);
                ctrlCmdSend(sfd, ACK_PACKAGE(remoteCmdStdout),NULL, 0);
            }
            else
            {
                remoteCmd += " 1>/tmp/remotePipe 2>&1";
                system(remoteCmd.toStdString().c_str());
                QFile pipeFile("/tmp/remotePipe");

                if(pipeFile.open(QIODevice::ReadWrite))
                {
                    QByteArray msg = pipeFile.readAll();
                    if(msg.size()<1000){
                        ret = ctrlCmdSendMore(sfd, ACK_PACKAGE(remoteCmdStdout),
                                (u_char *)msg.data(), msg.size(),
                                cStringType, sloopFlag);
                    }

                }
                else
                    ctrlCmdSend(sfd, ACK_PACKAGE(remoteCmdStdout),NULL, 0);
            }

        }
        ret = remoteCmdStdout;
        break;
    case switchServer:
        {
            if(NULL == pRdApdu->dUnit.dataPtr)
                goto aerr;

            QString switchServerInfo= (char*)pRdApdu->dUnit.dataPtr;
            switchServerInfo.resize(pRdApdu->dUnit.dataLength);
            MyWriteLog("INFO: receive switchServer: %s.", pRdApdu->dUnit.dataPtr);

            if(switchServerInfo.contains(":"))
            {
                QString serverIp   = switchServerInfo.section(":", 0, 0);
                QString serverPort = switchServerInfo.section(":", 1, 1);
                if(!serverIp.isEmpty() && !serverPort.isEmpty())
                {
                    strncpy(g_ServerConfig.serverIP, serverIp.toStdString().c_str(), sizeof(g_ServerConfig.serverIP)-1);
                    g_ServerConfig.Num = serverPort.toULong();
                    replaceConfigswitchServer(serverIp, serverPort);
                    parseNetConfigDomain();
                    g_onSwitchServer=true;

                    cmdRet = true;
                }
                else
                    cmdRet = false;
            }
            else{
                cmdRet = false;
            }
        }
        ctrlCmdSend(sfd, ACK_PACKAGE(switchServer), &cmdRet, 1);
        ret = switchServer;
        break;
    case uploadCfgFile:
    case ACK_PACKAGE(uploadCfgFile):
        {
            int dateRet = FALSE;
            if(NULL != pRdApdu->dUnit.dataPtr)
                dateRet = *(unsigned char*)pRdApdu->dUnit.dataPtr;

            MyWriteLog("INFO: receive uploadCfgFile.");

            char *cfgFile = "/tmp/configure.zip";
            QFile file(cfgFile);
            if(0 == uploadPackageTotalNum)
            {
                uploadPackageTotalNum = (file.size()/1000) + (file.size()%1000?1:0);
                if(!file.exists())
                    ret = createZipFile("/opt/DataCollection/configure", cfgFile);
            }
            else if(TRUE == dateRet)
            {
                if(uploadPackageTotalNum == uploadPackageCurNum+1){
                    uploadPackageCurNum = 0;
                    uploadCfgFileOffset = 0;
                }else{
                    uploadPackageCurNum++;
                    uploadCfgFileOffset+=1000;
                }
            }

            if(file.open(QIODevice::ReadOnly))
            {
                QByteArray datagram;
                file.seek(uploadCfgFileOffset);
                if(false == file.atEnd())
                {
                    datagram = file.read(1000);
                    QByteArray sendArray;
                    sendArray.append(QString("%1\r\n%2\r\n").arg(uploadPackageTotalNum).arg(uploadPackageCurNum+1));
                    sendArray.append(datagram);
                    ctrlCmdSendMore(sfd, ACK_PACKAGE(uploadCfgFile), (u_char*)sendArray.data(), sendArray.size(),zipBasicType, 0);
                    MyWriteLog("INFO: uploadCfgFile [%d/%d].",
                               uploadPackageTotalNum,uploadPackageCurNum+1);
                }
            }
        }
        ret = uploadCfgFile;
        break;
    case downloadCfgFile:
        {
            if(NULL == pRdApdu->dUnit.dataPtr ||
                    0 == pRdApdu->dUnit.dataLength ||
                    zipBasicType != pRdApdu->dUnit.dataType){
                qDebug()<<pRdApdu->dUnit.dataPtr<<pRdApdu->dUnit.dataLength<<pRdApdu->dUnit.dataType;
                goto aerr;
            }
            QString msgInfo = (char*)pRdApdu->dUnit.dataPtr;
            QString packageTotal = msgInfo.split("\r\n").at(0);
            QString packageNum   = msgInfo.split("\r\n").at(1);
            if(packageTotal.isEmpty() || packageNum.isEmpty()){
                qDebug()<<msgInfo;
                goto aerr;
            }
            MyWriteLog("INFO: receive downloadCfgFile [%d/%d].",
                       packageTotal.toUInt(),packageNum.toUInt());

            if(packageTotal.toUInt() != 0)
                packageTotalNum = packageTotal.toInt();
            QFile file("/tmp/config.zip");
            if(0 == packageCurNum){
                file.open(QIODevice::WriteOnly);
            }
            else{
                file.open(QIODevice::Append);
            }

            if(packageCurNum+1 != packageNum.toUInt()){
                cmdRet = FALSE;
                file.close();
            }
            else{
                QDataStream out(&file);
                int validDataOff = msgInfo.indexOf("\r\n", 2)+2;
                qDebug()<<msgInfo.indexOf("\n")<<msgInfo.indexOf("\n", 2);
                out.writeRawData((char*)pRdApdu->dUnit.dataPtr+validDataOff, pRdApdu->dUnit.dataLength-validDataOff);
                cmdRet = TRUE;
                packageCurNum++;
                file.close();

                if(packageNum.toUInt() == packageTotal.toUInt())
                {
                    cmdRet = FALSE;
                    system("mv /opt/DataCollection/configure/ /opt/DataCollection/configure.bak");
                    pid_t status = system("unzip /tmp/config.zip -d /opt/DataCollection/");
                    if(WIFEXITED(status))
                    {
                        if(0 == WEXITSTATUS(status)){
                            cmdRet = TRUE;
                            MyWriteLog("configure file unzip successfully");
                        }else{
                          system("mv /opt/DataCollection/configure.bak /opt/DataCollection/configure/ ");
                        }
                    }else{
                        system("mv /opt/DataCollection/configure.bak /opt/DataCollection/configure/ ");
                    }
                    packageTotalNum = 0;
                    packageCurNum = 0;
                }
            }
        }

        ctrlCmdSend(sfd, ACK_PACKAGE(downloadCfgFile), &cmdRet, 1);
        ret = downloadCfgFile;
        break;

    case startRuning:
        MyWriteLog("INFO: receive startRun.");
        cmdRet = true;
        ctrlCmdSend(sfd, ACK_PACKAGE(startRuning), &cmdRet, 1);
        ret = startRuning;
        g_sysState = State_START_DBG;
        break;
    case stopRuning:
        MyWriteLog("INFO: receive stopRun.");
        cmdRet = true;
        ctrlCmdSend(sfd, ACK_PACKAGE(stopRuning), &cmdRet, 1);
        ret = stopRuning;
        g_sysState = State_START_DBG;
        break;
    case openDataStream:
        MyWriteLog("INFO: receive openDataStream.");
        cmdRet = true;
        ctrlCmdSend(sfd, ACK_PACKAGE(stopRuning), &cmdRet, 1);
        ret = stopRuning;
        break;
    case closeDataStream:
        MyWriteLog("INFO: receive closeDataStream.");
        cmdRet = true;
        ctrlCmdSend(sfd, ACK_PACKAGE(stopRuning), &cmdRet, 1);
        ret = stopRuning;
        break;
    default:
        ret = -2;
        MyWriteLog(commRdFailed, "WARING: receive error cmd 0x%x", pRdApdu->funcId);
        if(1){
            char dbgMsg[2048] = {0};
            MyWriteLog(commRdFailed,"%d bytes.", datalen);
            for(int i=0; i<datalen; i++)
                sprintf(dbgMsg+i*3, "%02x ", buffer[i]);
            MyWriteLog(commRdFailed,dbgMsg);
        }
        break;
    }

    return ret;

aerr:
    {
        cmdRet = false;
        ctrlCmdSend(sfd, ACK_PACKAGE(pRdApdu->funcId), &cmdRet, 1);
        MyWriteLog("WARING: Analysis cmd package failed.");
        return -1;
    }


#if 0
        else if(funcode==6) //下载指定文件
        {
            memset(buf,0,128);
            memcpy(buf,buffer+analysislen,funlen);
            if(InitSocketConnect(sockfd,g_ftpConfig.lockIP,g_ftpConfig.serverIP,g_ftpConfig.uploadNum)>0)
            {
                MyWriteLog("Analysis-FTP net connect successful.");
                if(FTPLogin(sockfd,"keda","keda_138")==1)
                {
                    MyDebug("Analysis-FTP login successful.");
                    if(MyDownLoad(sockfd,buf,g_ftpConfig.lockIP,g_ftpConfig.serverIP)==1)
                    {
                        CloseSocketConnect(sockfd);
                        sockfd=0;
                    }
                }
                else
                {
                    MyWriteLog("ERROR: Analysis-FTP login failed.");
                    CloseSocketConnect(sockfd);
                    sockfd=0;
                    sleep(2);
                }
            }
            else
            {
                MyDebug("Analysis-FTP connect fail");
                sockfd=0;
                sleep(2);
            }
        }
#endif

}

//远程管理线程
void* Manage_Thread(void* arg)
{
    int fd, ret;
    void *ctx = NULL;
    int timeout = 5000;// millsecond
    void *pfd=NULL;
    int sockfd, maybeDisconnected = TRUE;
    bool isMainManagerThread = (NULL == arg)?true:false;
    char heartBeatString[10]={0};
    int serverPort;   //数据上传端口
    char serverIP[16]={0};     //服务器IP
    char localIP[16];     //服务器IP

    MyWriteLog(sysExecFlow,"%s: threadid: %d. Start.", __func__, pthread_self());

    strncpy(heartBeatString, g_ServerConfig.iotDevName, 6);
    strncpy(heartBeatString+sizeof(g_ServerConfig.iotDevName), SOFTVERSIONNUM, strlen(SOFTVERSIONNUM));

#if !USEMQ
    pfd = (void*)&sockfd;
#endif

    while(1)
    {
        sleep(1);
        if(TRUE == maybeDisconnected)//断线重连
        {
            if(isMainManagerThread){
                memcpy(localIP, g_ServerConfig.lockIP, sizeof(localIP));
                memcpy(serverIP, g_ServerConfig.serverIP, sizeof(serverIP));
                serverPort = g_ServerConfig.Num;
            }
            else{
                if(g_ServerConfigAuxiliary.Num<=0){
                    MyWriteLog(commInfo,"%s: threadid: %d exit.", __func__, pthread_self());
                    return NULL;
                }
                memcpy(localIP, g_ServerConfigAuxiliary.lockIP, sizeof(localIP));
                memcpy(serverIP, g_ServerConfigAuxiliary.serverIP, sizeof(serverIP));
                serverPort = g_ServerConfigAuxiliary.Num;
                if(!g_ServerConfigAuxiliary.serverIP[0])
                    break;
            }
#if USEMQ
            ctx = zmq_ctx_new ();
            pfd = zmq_socket (ctx, ZMQ_REQ);
            zmq_setsockopt (pfd, ZMQ_RCVTIMEO, &timeout, sizeof (int));
#endif            
            if(upLinkInit(pfd, localIP,serverIP,serverPort))//连接远程服务器成功
            {
                MyWriteLog(commInfo,"%s Manager Server %s net connect successful.", isMainManagerThread?"Main":"AUX",serverIP);
                if(ctrlCmdSend(pfd, heartBeat, (u_char *)heartBeatString, HEADBEAT_PACK_DATALEN))//发送心跳包
                {
                    ret = ctrlCmdRecv(pfd);//接收心跳包
                    if(0 > ret){
                        goto ManagerReadErr;
                    }
                    else
                    {
                        maybeDisconnected = FALSE;
                        MyWriteLog(commInfo,"%s Manager Server %s connect successful.",  isMainManagerThread?"Main":"AUX", serverIP);
                        if(ctrlCmdSend(pfd, correctTime, NULL, 0))//校时
                        {
                            ret = ctrlCmdRecv(pfd);
                            if(-1 == ret)
                                goto ManagerReadErr;
                            else if(isMainManagerThread){
                                g_ManageThreadIsStarted = true;
                            }
                        }
                    }
                }
            }
            else
            {                
                MyWriteLog(commInfo,"ERROR: connect to Manager Server failed. serverIP %s", serverIP);
                upLinkClose(pfd);
                sleep(5);
            }
        }
        else
        {
            ret = ctrlCmdRecv(pfd);
            if(-1 == ret)//此处不处理接收超时错误
            {
                goto ManagerReadErr;
            }
            ret = ctrlCmdSend(pfd, heartBeat, (u_char *)heartBeatString, sizeof(heartBeatString));//发送心跳包
            if(ret <= 0)
            {
                goto ManagerWriteErr;
            }
        }
        continue;

ManagerReadErr:
        {
            MyWriteLog("ERROR: Manager Server: receive cmd failed. serverIP %s", serverIP);
            {
                maybeDisconnected = TRUE;
                upLinkClose(pfd);
#if USEMQ
                zmq_ctx_destroy(ctx);
#endif
            }
        }

        continue;

ManagerWriteErr:
        {
            MyWriteLog("ERROR: Manager Server: cmd send failed. serverIP %s", serverIP);

            maybeDisconnected = TRUE;
            upLinkClose(pfd);
#if USEMQ
            zmq_ctx_destroy(ctx);
#endif
        }
    }

    if(fd>0)
    {
        close(fd);//关闭连接
    }
#if USEMQ
    zmq_ctx_term (ctx);
#endif

    MyWriteLog(sysExecFlow,"%s: threadid: %d exit.", __func__, pthread_self());

    return arg;
}

//实时数据上传线程
void* realTimeDataUpload_Thread(void* arg)
{
    int ret;
    void *ctx = NULL;
    int errCount=0;
    int timeout = 3000;// millsecond
    void *pfd=NULL;
    int sockfd, maybeDisconnected = TRUE;
    struct timeval start,end,diff;

    g_ftpConfig.rtDatauploadTime = 10;
#if 0 //!不依赖管理流，日后需要再打开
    while(false == g_ManageThreadIsStarted)
        sleep(1);
#endif
    MyWriteLog(sysExecFlow,"%s: threadid: %d", __func__, pthread_self());
    signal(SIGPIPE,SIG_IGN);

#if !USEMQ
    pfd = (void*)&sockfd;
#endif

	//初始化设备断线计时起始时间和持续时间 --liut 170922
    QDateTime disconnectBegin(QDateTime::currentDateTime());
    QDateTime disconnectNow;
	
    gettimeofday(&start,0);
    while(1)
    {
        if(TRUE == maybeDisconnected)//断线重连 --liut
        {
			//超过规定时间 重启wifi --liut 170922
            disconnectNow = QDateTime::currentDateTime();

            if(0 == resetWlanInterval)
            {
                resetWlanInterval = 600;
            }

            if(disconnectBegin.secsTo(disconnectNow) >= resetWlanInterval)
            {
#ifdef PRODUCTVERSION_ZC
                ret = system("ifdown wlan0;sleep 1;ifup wlan0;sleep 1;ifconfig wlan0 up");
                if(0 == ret)
                {
                    MyWriteLog("reset wifi success.");
                }
                else
                {
                    MyWriteLog("reset wifi failure.");
                }

                disconnectBegin = QDateTime::currentDateTime();
#endif
            }

			
#if USEMQ
            ctx = zmq_ctx_new ();
            pfd = zmq_socket (ctx, ZMQ_REQ);
            zmq_setsockopt (pfd, ZMQ_RCVTIMEO, &timeout, sizeof (int));

#endif
            if(upLinkInit(pfd, g_ServerConfig.lockIP,g_ServerConfig.serverIP,g_ServerConfig.Num))//连接远程服务器 --liut
            {
                MyWriteLog(commInfo,"realTimeData Server Net Connect successful.");
                gettimeofday(&start,0);
                maybeDisconnected = FALSE;
                sleep(1);       //防止服务器无法快速响应新连接以处理数据 --liut 171014
#if !USEMQ
                struct timeval tout={timeout/1000,0};
                ret=setsockopt(*(int*)pfd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&tout,sizeof(tout));
                ret=setsockopt(*(int*)pfd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&tout,sizeof(tout));
#endif
            }
            else
            {
                MyWriteLog(commInfo,"ERROR: connect to realTimeData Server failed.");
                upLinkClose(pfd);
                sleep(5);
            }
        }
        else
        {
			//更新设备断线计时起始时间 --liut 170922
            disconnectBegin = QDateTime::currentDateTime();
			
            gettimeofday(&end,0);
            timeval_subtract(&diff,&start,&end);

            QDir dir(RT_CACHE_PATH);
            dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
            dir.setSorting(QDir::Name);
            ret = 0;

            QFileInfoList list = dir.entryInfoList();
            foreach (QFileInfo fileInfo, list)//遍历目录 --liut
            {
                if(fileInfo.isDir())
                {
                    dir.setPath(fileInfo.filePath());
                    dir.setFilter(QDir::Files);

                    if(dir.entryList().count() != 0)
                        break;
                    else if (fileInfo.fileName() == QDate::currentDate().toString("yyyyMMdd"))
                        break;
                    MyWriteLog("WARNING: remove cache dir %s.", fileInfo.filePath().toStdString().c_str());

                    dir.rmpath(fileInfo.filePath());
                }

            }

            if(dir.exists() && dir.entryInfoList().count()>0)//存在历史文件时 --liut
            {
                sdCardRWMutex.lock();

                QFileInfoList flist = dir.entryInfoList();
                QFileInfo fileInfo = flist.first();
                QFile file( dir.filePath(fileInfo.filePath()) );
                if(0 == file.size())
                {
                    MyWriteLog("WARNING: remove cache %s.", file.fileName().toStdString().c_str());
                    file.remove();
                }
                else{
                    ret = remoteDataTransport(pfd, realDataUpload, file, 0);//传输历史文件 --liut
                }

                sdCardRWMutex.unlock();
            }
            else if(diff.tv_sec>=g_ftpConfig.rtDatauploadTime)
            {
                ret = remoteDataTransport(pfd, realDataUpload);//传输实时数据 --liut
                gettimeofday(&start,0);
            }

            if(-1 == ret || g_onSwitchServer)
            {
                if(g_onSwitchServer){
                    MyWriteLog("INFO: %s event : onSwitchServer %s:%d",
                               __func__, g_ServerConfig.serverIP, g_ServerConfig.Num);
                    g_onSwitchServer = false;
                }
                maybeDisconnected = TRUE;
                upLinkClose(pfd);
#if USEMQ
                zmq_ctx_destroy(ctx);
#endif
                errCount = 0;
                sleep(1);
            }
        }
        usleep(100000);//周期100ms
    }

    if(*(int*)pfd)
    {
        close(*(int*)pfd);
    }
#if USEMQ
    zmq_ctx_term (ctx);
#endif

    MyWriteLog(sysExecFlow,"%s: threadid: %d exit.", __func__, pthread_self());
    return arg;
}

void* statsVarAcquisition_Thread(void* arg)
{
    QMapIterator<int, statsVarConfig*> svar(g_statsVarConfig);
    QScriptEngine expressionEngine;
    QMap<int, QString> varIdMap;

    MyWriteLog(sysExecFlow,"%s: threadid: %d", __func__, pthread_self());

    while(svar.hasNext())
    {
        svar.next();
        int j=0;

        QString expression = QString(svar.value()->varTriggerCond);//事件触发条件 --liut

        while ((j = expression.indexOf(QRegExp("[vV]+[0-9]*"), j)) != -1) {
            int varId = expression.mid(j+1,-1).split(QRegExp("[^vV0-9]")).at(0).toUInt();
            QString  varName = expression.mid(j,-1).split(QRegExp("[^vV0-9]")).at(0);
            varIdMap.insert(varId, varName);
            j++;
        }
    }

    while(1){
        QMapIterator<int, QString> varId(varIdMap);
        while(varId.hasNext())
        {
            varId.next();
            varConfig* var = g_varsHashTable.value(varId.key());

            if(NULL == var)     //增加变量判断 防止出现段错误 --liut 20171012
            {
                MyWriteLog("ERROR: varId:%d isn't existed in g_varsHashTable.", varId.key());
                continue;
            }

            switch (var->dataType) {
                case 1:
                case 4:
                    expressionEngine.globalObject().setProperty(varId.value(), QScriptValue(var->uiVar));
                    //qDebug()<<varId.value()<<var->uiVar;
                    break;
                case 2:
                    expressionEngine.globalObject().setProperty(varId.value(), QScriptValue(var->iVar));
                    //qDebug()<<varId.value()<<var->iVar;
                    break;
                case 3:
                    expressionEngine.globalObject().setProperty(varId.value(), QScriptValue(var->fVar));
                    //qDebug()<<varId.value()<<var->fVar;
                    break;
                default:
                    break;
            }

        }
        svar.toFront();
        while(svar.hasNext())
        {
            svar.next();
            QString expression = QString(svar.value()->varTriggerCond);//事件触发条件 --liut
            bool isTriggerd = expressionEngine.evaluate(expression).toBool();
            QTime now = QTime::currentTime();

            if(isTriggerd && true == svar.value()->enStats)//事件已触发 --liut
            {
                if(svar.value()->triggerTime.isNull())
                {
                    svar.value()->triggerTime = now;
                    //qDebug()<<"set:"<<svar.value()->varName<<svar.value()->elapsedMs<<svar.value()->triggerTime.toString("ss.zzz");

                }else{
                    svar.value()->rwMutex.lock();
                    if(svar.value()->triggerTime.msecsTo(now) > 0)
                        svar.value()->elapsedMs = svar.value()->triggerTime.msecsTo(now);
//                    qDebug()<<svar.value()->varName
//                           <<svar.value()->triggerTime.toString("ss.zzz")
//                          <<now.toString("ss.zzz")
//                         <<svar.value()->triggerTime.msecsTo(now);
                    svar.value()->rwMutex.unlock();
                }
                //qDebug()<<svar.value()->varName<<svar.value()->elapsedMs<<expression;
            }
            else{
                svar.value()->triggerTime = QTime();
            }
        }
        usleep(1000);
    }

    return NULL;
}

void* statsEventDataUpload_Thread(void* arg)//统计数据上传线程 --liu
{
    int ret;
    void *ctx = NULL;
    int errCount=0;
    int timeout = 3000;// millsecond
    void *pfd=NULL;
    int sockfd, maybeDisconnected = TRUE;

    MyWriteLog(sysExecFlow,"%s: threadid: %d", __func__, pthread_self());
    signal(SIGPIPE,SIG_IGN);

#if !USEMQ
    pfd = (void*)&sockfd;
#endif

    while(1)
    {
        //send data
        if(TRUE == maybeDisconnected)//断线重连 --liut
        {
#if USEMQ
            ctx = zmq_ctx_new ();
            pfd = zmq_socket (ctx, ZMQ_REQ);
            zmq_setsockopt (pfd, ZMQ_RCVTIMEO, &timeout, sizeof (int));

#endif
            if(upLinkInit(pfd, g_ServerConfig.lockIP,g_ServerConfig.serverIP,g_ServerConfig.Num))//连接远程服务器 --liut
            {
                MyWriteLog(commInfo,"stats realTimeData Server Net Connect successful.");
                maybeDisconnected = FALSE;
                sleep(1);       //防止服务器无法快速响应新连接以处理数据 --liut 171014
#if !USEMQ
                struct timeval tout={timeout/1000,0};
                ret=setsockopt(*(int*)pfd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&tout,sizeof(tout));
                ret=setsockopt(*(int*)pfd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&tout,sizeof(tout));
#endif
            }
            else
            {
                MyWriteLog(commInfo,"ERROR: connect to stats realTimeData Server failed.");
                upLinkClose(pfd);
                sleep(5);
            }
        }
        else
        {
            QDir dir(STATS_RT_CACHE_PATH);
            dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
            dir.setSorting(QDir::Name);
            ret = 0;

            QFileInfoList list = dir.entryInfoList();
            foreach (QFileInfo fileInfo, list)
            {
                if(fileInfo.isDir())
                {
                    dir.setPath(fileInfo.filePath());
                    dir.setFilter(QDir::Files);

                    if(dir.entryList().count() != 0)
                        break;
                    else if (fileInfo.fileName() == QDate::currentDate().toString("yyyyMMdd"))
                        break;
                    MyWriteLog("WARNING: remove cache dir %s.", fileInfo.filePath().toStdString().c_str());

                    dir.rmpath(fileInfo.filePath());
                }

            }

            if(dir.exists() && dir.entryInfoList().count()>0)
            {
                sdCardRWMutex.lock();

                QFileInfoList flist = dir.entryInfoList();
                QFileInfo fileInfo = flist.first();
                QFile file( dir.filePath(fileInfo.filePath()) );
                if(0 == file.size())
                {
                    MyWriteLog("WARNING: remove cache %s.", file.fileName().toStdString().c_str());
                    file.remove();
                }
                else{
                    ret = remoteDataTransport(pfd, statisticsDataUpload, file, 0);//上传历史文件 --liut
                }

                sdCardRWMutex.unlock();
            }
            else
            {
                ret = remoteDataTransport(pfd, statisticsDataUpload);//上传实时数据 --liut
            }

            if(-1 == ret)
            {
                maybeDisconnected = TRUE;
                upLinkClose(pfd);
#if USEMQ
                zmq_ctx_destroy(ctx);qDebug()<<__func__<<__LINE__;
#endif
                errCount = 0;
                sleep(1);
            }
        }
        sleep(1);
    }
    if(*(int*)pfd)
    {
        close(*(int*)pfd);
    }
#if USEMQ
    zmq_ctx_term (ctx);
#endif

    MyWriteLog(sysExecFlow,"%s: threadid: %d exit.", __func__, pthread_self());
    return arg;
}
//统计数据流线程
void* statsEventStore_Thread(void* arg)
{

    if(NULL == arg)
    {
        MyWriteLog(sysExecFlow,"%s: threadid: %d arg invalid.", __func__, pthread_self());
        return NULL;
    }
    //!获取事件节点
    eventConfig* eventNode = (eventConfig*)arg;
    //!获取事件所属的统计变量MAP集合
    QMapIterator<int, statsVarConfig*> varsOfEvent(eventNode->varGroup);

    //!获取事件触发条件判断规则
    QScriptEngine expressionEngine;
    QString eventExpression = QString(eventNode->eventTriggerCond);

    //!创建变量id map集合
    QMap<int, QString> varIdMap;
    //!如下从事件触发条件表达式中提取对应的变量，并存储到varIdMap中
    int j=0;
    while ((j = eventExpression.indexOf(QRegExp("[vV]+[0-9]*"), j)) != -1)
    {
        int varId = eventExpression.mid(j+1,-1).split(QRegExp("[^0-9]")).at(0).toUInt();
        QString  varName = eventExpression.mid(j,-1).split(QRegExp("[^vV0-9]")).at(0);
        varIdMap.insert(varId, varName);
        j++;
    }

    MyWriteLog(sysExecFlow,"%s: threadid: %d", __func__, pthread_self());
    signal(SIGPIPE,SIG_IGN);

    bool triggerState=false;//!事件历史触发状态
    bool isTriggerd=false;  //!事件是否触发标志
    QDateTime startTime=QDateTime();//!事件触发后计时器
    QDateTime eventTriggerdTime = QDateTime();//!事件触发开始时间

    while(1)
    {
        QString statsInfo;
        QMapIterator<int, QString> it(varIdMap);
        while(it.hasNext())
        {
            it.next();
            varConfig* var = g_varsHashTable.value(it.key());

            if(NULL == var)     //增加变量判断 防止出现段错误 --liut 20171012
            {
                MyWriteLog("ERROR: varId:%d isn't existed in g_varsHashTable.", it.key());
                continue;
            }

            switch (var->dataType) {
                case 1:
                case 4:
                    expressionEngine.globalObject().setProperty(it.value(), QScriptValue(var->uiVar));
                    statsInfo.append(QString("%1,%2\r\n").arg(var->myid).arg(var->uiVar));
                    break;
                case 2:
                    expressionEngine.globalObject().setProperty(it.value(), QScriptValue(var->iVar));
                    statsInfo.append(QString("%1,%2\r\n").arg(var->myid).arg(var->iVar));
                    break;
                case 3:
                    expressionEngine.globalObject().setProperty(it.value(), QScriptValue(var->fVar));
                    statsInfo.append(QString("%1,%2\r\n").arg(var->myid).arg(var->fVar));
                    break;
                default:
                    break;
            }
            //qDebug()<<"statsInfo"<<statsInfo;
        }

        isTriggerd = expressionEngine.evaluate(eventExpression).toBool();

        QDateTime now = QDateTime::currentDateTime();
        if(isTriggerd)//事件触发 --liut
        {
            if(startTime.isNull())
                startTime = QDateTime::currentDateTime();
            if(eventTriggerdTime.isNull())
                eventTriggerdTime = QDateTime::currentDateTime();
            //start
            varsOfEvent.toFront();
            while(varsOfEvent.hasNext())
            {
                varsOfEvent.next();
                varsOfEvent.value()->rwMutex.lock();
                varsOfEvent.value()->enStats = true;
                varsOfEvent.value()->rwMutex.unlock();
            }

        }
        //qDebug()<<__func__<<__LINE__<<eventNode->id<<triggerState<<isTriggerd;
        //qDebug()<<startTime.secsTo(now)
        //       <<startTime.toString("yyyy-MM-dd hh:mm:ss")
        //       <<QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        if((false == isTriggerd && true == triggerState) ||
                (true == isTriggerd && startTime.secsTo(now)>=eventNode->uploadCycleS))
        {
            bool statsEventIsEnd = false;
            //!统计开始时间,统计结束时间 + 统计时长
            if((false == isTriggerd && true == triggerState))
            {
                statsInfo.append(QString("%1,%2\r\n%3\r\n").
                                 arg(eventTriggerdTime.toString("yyyy-MM-dd hh:mm:ss")).
                                 arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).
                                 arg(startTime.secsTo(now)));
                eventTriggerdTime = QDateTime();
                statsEventIsEnd = true;

            }else{
                statsInfo.append(QString("%1,\r\n%2\r\n").
                                 arg(eventTriggerdTime.toString("yyyy-MM-dd hh:mm:ss")).
                                 arg(startTime.secsTo(now)));
            }

            varsOfEvent.toFront();
            while(varsOfEvent.hasNext())//存储数据 --liut
            {
                varsOfEvent.next();
                statsInfo.append(QString("%1,%2\r\n").arg(varsOfEvent.value()->id).arg(varsOfEvent.value()->elapsedMs));
                varsOfEvent.value()->rwMutex.lock();

                //!标识事件结束
                if(statsEventIsEnd)
                {
                    varsOfEvent.value()->enStats = false;
                }
                //qDebug()<<varsOfEvent.value()->id<<varsOfEvent.value()->type;
                //! 变量是否在事件周期内进行清0处理
                if(varsOfEvent.value()->type == statsType_IncClear)
                {
                    varsOfEvent.value()->triggerTime = QTime();
                }

                varsOfEvent.value()->rwMutex.unlock();

            }

            if(true == statsEventIsEnd)
                //!事件结束复位计算周期起始时间
                startTime=QDateTime();
            else
                //!事件结束之前，每个超时周期需要重新计算周期起始时间
                startTime = QDateTime::currentDateTime();
            //qDebug()<<statsInfo;
            statsVarFileWrite(STATS_RT_CACHE_PATH, (char*)statsInfo.toStdString().c_str(), statsInfo.length());
        }
        triggerState = isTriggerd;


        usleep(eventNode->interCycleMs*1000);
    }

    MyWriteLog(sysExecFlow,"%s: threadid: %d exit.", __func__, pthread_self());
    return arg;

}

#define GROUPIP "239.255.255.250"
#define GROUPPORT 6666
//需要route add -net 239.255.255.250 netmask 255.255.255.255 eth0
void* udpManagerServer_Thread(void* arg)
{
#if 0 //!配置工具调试指令流，调试完成再打开
    QString devInfo;
    QStringList address;
    QMap<QString,QString> ipInfoMap;

    QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
    foreach(QNetworkInterface interface, list)
    {
        QNetworkInterface::InterfaceFlags flags = interface.flags();
        if (flags.testFlag(QNetworkInterface::IsRunning))
        {
            QList<QNetworkAddressEntry> entryList = interface.addressEntries();
            foreach(QNetworkAddressEntry entry,entryList){
                qDebug()<<"IP Address: "<<entry.ip().toString();
                ipInfoMap.insert(entry.ip().toString(), entry.netmask().toString());
            }
        }

    }

    struct sockaddr_in addr;
    struct ip_mreq mreq;
    int sock;

    memset( &mreq, 0, sizeof(struct ip_mreq) );
    if ( (sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) <0)
    {
        perror("socket");
        return NULL;
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(GROUPPORT);
    if( inet_aton(GROUPIP, &addr.sin_addr ) < 0 ) {
        perror("inet_aton");
        return NULL;
    }
    //addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(sock,(struct sockaddr *)&addr,sizeof(addr));

    mreq.imr_multiaddr.s_addr = inet_addr(GROUPIP);  /*多播地址*/
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);      /*网络接口为默认*/
    /*将本机加入多播组*/
    int ret = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq, sizeof(mreq));
    if (ret < 0)
    {
        perror("setsockopt():IP_ADD_MEMBERSHIP");
        return NULL;
    }

    char buff[512];
    struct sockaddr_in clientAddr;
    int len = sizeof(clientAddr);
    while (1)
    {
        int n;
        n = recvfrom(sock, buff, 512, 0, (struct sockaddr *)&addr, (socklen_t*)&len);
        if (n>0)
        {
            buff[n] = 0;
            printf("received [%s:%d]:", inet_ntoa(addr.sin_addr), addr.sin_port);
            puts(buff);
        }

        if(!strcmp(buff, "detecet KDZK-IOT"))
        {
            QString localIp;
            QMapIterator<QString,QString> it(ipInfoMap);
            while(it.hasNext())
            {
                it.next();
                quint32 ipAddr = inet_addr(it.key().toStdString().c_str());
                quint32 ipNetMask = inet_addr(it.value().toStdString().c_str());
                quint32  IPAddrA = ipAddr & ipNetMask;
                quint32  IPAddrB = addr.sin_addr.s_addr & ipNetMask;
                //printf("%x %x %x %x %x\n", ipAddr,ipNetMask,IPAddrA,IPAddrB,addr.sin_addr.s_addr);
                if(IPAddrA == IPAddrB)
                    localIp = it.key();
            }

            if(!localIp.isEmpty()){
                devInfo = localIp+":"+DEVICE_TYPEID+":"+SOFTVERSION;
            }

            sprintf(buff, "%s\n", devInfo.toStdString().c_str());
            n = sendto(sock, buff, strlen(buff), 0, (struct sockaddr *)&addr, sizeof(addr));
            if (n < 0)
            {
                perror("sendto");
                close(sock);
                break;
            }
        }

        sleep(1);
    }
#endif
    return NULL;
}


#define TCP_SERVER_PORT 6668
void* tcpManagerServer_Thread(void* arg)
{
#if 0 //!配置工具调试指令流，调试完成再打开
    int ret;
    int sockfd;                        // Socket file descriptor
    int nsockfd;                       // New Socket file descriptor
    char sdbuf[512];                // Send buffer
    struct sockaddr_in addr_local;
    struct sockaddr_in addr_remote;

    char heartBeatString[10]={0};
    strncpy(heartBeatString, g_ServerConfig.iotDevName, 6);
    strncpy(heartBeatString+sizeof(g_ServerConfig.iotDevName), SOFTVERSIONNUM, strlen(SOFTVERSIONNUM));

    /* Get the Socket file descriptor */
    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
    {
        printf ("ERROR: Failed to obtain Socket Despcritor.\n");
        return (0);
    }
    else
    {
        printf ("OK: Obtain Socket Despcritor sucessfully.\n");
    }

    /* Fill the local socket address struct */
    addr_local.sin_family = AF_INET;           // Protocol Family
    addr_local.sin_port = htons(TCP_SERVER_PORT);         // Port number
    addr_local.sin_addr.s_addr  = INADDR_ANY;  // AutoFill local address
    bzero(&(addr_local.sin_zero), 8);          // Flush the rest of struct

    /*  Blind a special Port */
    if( bind(sockfd, (struct sockaddr*)&addr_local, sizeof(struct sockaddr)) == -1 )
    {
          printf ("ERROR: Failed to bind Port %d.\n",TCP_SERVER_PORT);
        return (0);
    }
    else
    {
        printf("OK: Bind the Port %d sucessfully.\n",TCP_SERVER_PORT);
    }

    /*  Listen remote connect/calling */
    if(listen(sockfd,10) == -1)
    {
        printf ("ERROR: Failed to listen Port %d.\n", TCP_SERVER_PORT);
        return (0);
    }
    else
        {
        printf ("OK: Listening the Port %d sucessfully.\n", TCP_SERVER_PORT);
    }

    while(1)
    {
        int sin_size = sizeof(struct sockaddr_in);

        /*  Wait a connection, and obtain a new socket file despriptor for single connection */
        if ((nsockfd = accept(sockfd, (struct sockaddr *)&addr_remote, (socklen_t*)&sin_size)) == -1)
        {
            printf ("ERROR: Obtain new Socket Despcritor error.\n");
            sleep(1);
            continue;
        }
        else
        {
            printf ("OK: Server has got connect from %s.\n", inet_ntoa(addr_remote.sin_addr));
        }

        uchar* pOutBuf = NULL;
        unsigned short dataLen, dataType;

        while(1)
        {
            usleep(10000);
            QTime startTime = QTime::currentTime();

            ret = ctrlCmdRecv(&nsockfd);
            if(-1 == ret){//此处不处理接收超时错误-2
                qDebug()<<"ctrlCmdRecv error!";
                break;
            }

            DbgRtDateList.rtlMutex.lock();
            while(DbgRtDateList.list.count())
            {
                pOutBuf = (uchar*)DbgRtDateList.list.front();
                int dataLen = strlen((char*)pOutBuf);
                dataType = cStringType;
                ctrlCmdSendMore(&nsockfd, channelDataUpload, pOutBuf, dataLen, dataType, 0);
                delete pOutBuf;
                DbgRtDateList.list.pop_front();
            }
            DbgRtDateList.rtlMutex.unlock();

            QString logStr;
            while(getLogList(logStr))
            {
                dataLen = logStr.length();
                dataType = cStringType;
                ctrlCmdSendMore(&nsockfd, logDataUpload, pOutBuf, dataLen, dataType, 0);
            }

            QTime stopTime = QTime::currentTime();
            int elapsed = startTime.msecsTo(stopTime);
            if(elapsed>1000)
            {
                ret = ctrlCmdSend(&nsockfd, heartBeat, (u_char *)heartBeatString, sizeof(heartBeatString));
                if(ret <= 0){
                    qDebug()<<"ctrlCmdSend error!";
                    break;
                }
            }
        }
        close(nsockfd);
    }
#endif
    return NULL;
}
