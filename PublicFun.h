#ifndef PUBLICFUN_H
#define PUBLICFUN_H
#include <QString>
#include <QFile>


enum logLevelType{
    logNone,

    commInfo            =   (1<<1 ),
    commRdSuccess       =   (1<<2 ),
    commRdFailed        =   (1<<3 ),
    commWrSuccess       =   (1<<4 ),
    commWrFailed        =   (1<<5 ),

    sysErrInfo          =   (1<<10),
    sysExecFlow         =   (1<<11),

    logAll              =   (0xFFFFFFFF),
};

#define HEADBEAT_PACK_DATALEN 10

struct {
    unsigned char funFlag;
    unsigned char funLen;
    unsigned char *funData;

    unsigned char dataFlag;
    unsigned char dataLen;
    unsigned char *Data;
} autoUploadData;

enum funcDataType{
    basicType = 1,
    cStringType,
    zipFile,
    md5_32Type,
    binType,
    xmlType,
    jsonType,
    imageType,
    audioType,
    videoType,
};
enum compressType{
    noneCompress,
    zipCompress = 1,
};

#define zipcStringType  (zipCompress*256+cStringType)
#define zipBasicType    (zipCompress*256+basicType)

enum ManagerFunctions{
    DEFAULT,
    heartBeat          =    1,
    reqVarTable        =    2,
    correctTime        =    3,
    realDataUpload     =    4,
    rebootSystem       =    5,
    deviceWrite        =    6,
    setUpLoadInterval  =    7,
    setVarAttribute    =    8,
    firmwareUpgrade    =    9,
    switchServer       =    10,
    statisticsDataUpload  = 11,
    reqDevParameter    =    12,

    uploadlogFile      =    60,
    remoteCmdStdout    =    61,
    uploadCfgFile      =    62,
    downloadCfgFile    =    63,
    startRuning        =    64,
    stopRuning         =    65,
    openDataStream     =    66,
    closeDataStream    =    67,
    channelDataUpload  =    68,
    logDataUpload      =    69,
};
//#define offsetof(s,m) (size_t)&(((s *)0)->m)

#pragma pack(push,1)
#define ACK_PACKAGE(x)  (0x80|x)
#define PACKAGE_HEAD_LENGTH  offsetof(kdIotApdu, dUnit)
struct dataUnit{
    unsigned short dataType;
    unsigned short dataLength;
    void* dataPtr[0];
};

struct kdIotApdu {
    unsigned char funcId;
    unsigned short length;
    unsigned short packageNum;
    char devName[6];
    struct dataUnit dUnit;
};

#pragma pack(pop)

void LogFileInit(void); //日志初始化
void CloseLogFile(void); //关闭日志文件
void MyWriteLog(char* buf, ...);//写日志函数
void MyDebug(char* buf, ...);
//void MyWriteLog(int fd,char* buf, ...);
void MyWriteLog(logLevelType level, char* buf, ...);
int getLogList(QString &logStr);

long Get16Data2(unsigned char* data);
unsigned long UNGet16Data2(unsigned char* data);
long Get16Data4(unsigned char* data);
unsigned long UNGet16Data4(unsigned char* data);
float GetFloatData4(unsigned char* data);
int setCorrectTime(unsigned char *dataTime);
QString GBK2UTF8(char* inStr);
extern int ctrlCmdSendMore(void* sockfd, u_char code, u_char *cmd, unsigned short cmdlen, unsigned short dataType, unsigned short packageNum);
extern int ctrlCmdRecv(void* sockfd);
extern int upLinkClose(void* fd);
extern int upLinkInit(void* fd,char* lockIP,char* serverIp,int serverPort);

extern int remoteDataTransport(void* sockfd, ManagerFunctions func);
extern int remoteDataTransport(void* sockfd, ManagerFunctions func, QFile &rtCacheFile, int fileOffset);

extern size_t zip_compress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len);
extern size_t zip_uncompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len);

extern int Analysis(void* sfd, u_char* buffer,int datalen,int& analysised);

#endif  /* PUBLICFUN_H */
