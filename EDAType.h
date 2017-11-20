#ifndef EDATYPE_H
#define EDATYPE_H
#include <qmutex.h>
#include <QMap>
#include <QTime>

//!产品选型开关
#define PRODUCTVERSION_ZC   //!中车版本选项开关
//#define PRODUCTVERSION_TZ //!太重版本选项开关

#define DEVICE_TYPEID "KDZK-TZ"
#define SOFTVERSION "1.0.0.1"
#define SOFTVERSIONNUM "1001"
#define USEMQ 0
#define ENABLE_ZIP_STORE  0


#ifdef PRODUCTVERSION_ZC
//!中车版本选项开关
#define ENABLE_RTDATA_COMPRESS 1
#define USE_TIME_SINCE_EPOCH 0//日期格式 --liut 20171012
#else
//!太重版本选项开关
#define ENABLE_RTDATA_COMPRESS 0//1
#define USE_TIME_SINCE_EPOCH 1//0时间差（毫秒）格式 --liut 20171012
#endif

#define RT_CACHE_PATH "/media/mmcblk0p1/RTData/"
#define STATS_RT_CACHE_PATH "/media/mmcblk0p1/STRTData/"
#define SYS_LOG_PATH "/media/mmcblk0p1/logs/"


enum{
    varType_START,
    varType_PLC_I,
    varType_PLC_Q,
    varType_PLC_M,
    varType_PLC_DB,
    varType_MODBUS_DI,
    varType_MODBUS_DO,
    varType_MODBUS_AI,
    varType_MODBUS_AO,
};

typedef enum{
    State_INVALID,
    State_START,
    State_RUNNING,
    State_STOP,
    State_EXIT,
    State_START_DBG,
    State_RUNNING_DBG,
    State_STOP_DBG,
    State_EXIT_DBG,
}SysStateMachine_e;

//硬件接口配置
struct canBusConfig
{
    int canPortNum;
    int interfaceID;
    unsigned int nBaudRate;  //波特率
    canBusConfig* next;
};

struct serialConfig
{
    int comPortNum;
    int interfaceID;
    unsigned int nBaudRate;  //波特率
    char nParity;  //奇偶校验
    int nByteSize;  //数据字节宽度
    int nStopBits; //停止位
    serialConfig* next;
};

//网卡结构体
struct networkConfig
{
    int ethPortNum;
    int interfaceID;
    int devPort;        //设备端口
    char devIP[16];     //设备IP
    char lockIP[16];    //本机IP
    networkConfig* next;
};

//通道
struct channelConfig
{
    int channelID;            //通道ID(自增，全局不重复)
    int devID;               //所属设备id
    int devGeneralAdder;      //设备通用地址，根据设备驱动确定用途,2级地址 db块内偏移
    int devBeginAdder;        //设备采集数据起始地址，1级地址，db块地址
    int devDataLength;        //设备采集数据长度
    int devDataType;          //数据类型，根据设备驱动确定，功能码
    int isError;              //是否当前读取数值是否正常（数据质量）
    unsigned char* dataBuf;    //数据缓存，当创建结构体时，根据长度创建（new）区域

    unsigned int statsReadSuccess;       //读正确统计
    unsigned int statsReadFailed;       //读失败统计
    unsigned int statsWrite;       //写统计

    channelConfig* next;       //指向下一个通道。(所有通道链表，用于初始化)
    channelConfig* devNext;    //同一设备的下一个通道
};


//设备
struct devConfig
{
    int devID;          //设备id(自增，全局不重复)
    int interfaceID;    //所属接口id
    int devNum;          //设备号
    int isValid;        //是否有效
    char devName[128];    //设备名称
    devConfig* next;    //指向下个设备(所有设备链表，用于初始化)
    devConfig* devnext;    //指向同一个接口的下个设备
    channelConfig* dataBuf;
};

//变量数据结构
struct varConfig
{
    int myid;  //序号（全局自增）,步长是字节。
    int channelID;            //通道ID
    int memoryBeginAdder;     //通道内的数据起始地址
    int memoryDataLeng;       //内存数据长度(字节)
    int dataType;       //数据类型  1-无符号整数，2-有符号整数，3-小数，4-位 5-字符串
    int dataBit;        //如果为数据位，表示取第几位数据
    int storeType;        //存储类型（1-差异存储，2-比例存储（百分比），
                          // 3-比例存储（数值） 4-定期存储，单位是毫秒）5-单个报警值存储 6-所有报警值存储
    int storeData;      //数据值（存储的比较值）
    int storeCondition;  //存储条件  1-始终存储 2-某个变量大于等于1存储
    int storeVar;              //值为 0 时为始终存储 大于0 为某个变量的序号，表示这个变量为1开始存储数据
    int isCompression;   //是否启用旋转门压缩算法，默认情况数值变量启用。
    int isValid;      //是否有效
    int isStore;      //按照规则，是否需要存储

    int iVar;         //独立线程根据规则计算出的当前数值（如果当前为int类类型）
    float fVar;       //独立线程根据规则计算出的当前数值（如果当前为float类类型）
    unsigned int uiVar;  //独立线程根据规则计算出的当前数值（如果当前为unsigned int类类型）

    long hiVar;         //历史独立线程根据规则计算出的当前数值（如果当前为int类类型）(用于数据存储)
    float hfVar;       //历史独立线程根据规则计算出的当前数值（如果当前为float类类型）(用于数据存储)
    unsigned int huiVar;  //历史独立线程根据规则计算出的当前数值（如果当前为unsigned int类类型）(用于数据存储)

    int varGroupId;
    char varName[128];    //变量名称
    char varAddrName[64];    //变量名称

    int isWriteCmdValid;
    qint32 wiVar;         //写指令数值（如果当前为int类类型）
    float wfVar;       //写指令数值（如果当前为float类类型）
    quint32 wuiVar;  //写指令数值（如果当前为unsigned int类类型）

    unsigned char* memoryData;   //初始化时，使该指针指向具体的内存地址，方便直接调用
    varConfig* next;
    struct timeval data_Time;  //数据的采集时间
    struct timeval data_Compare_Time;  //数据的采集时间
};

//接口结构体
struct interfaceConfig
{
    int interfaceID;    //接口id(自增，全局不重复)
    int interface;      //通信接口  1-以太 2-串口
    char driveName[32];  //驱动程序的名称
    networkConfig* netWork;  //以太口配置
    serialConfig* serial;    //串口配置
    canBusConfig* canBus;
    devConfig* pDev;  //指向设备指针
    interfaceConfig* next;  //指向下一个接口
    QMutex mutex;
};



//FTP数据上传配置
struct ftpConfig
{
    unsigned int uploadTime;  //数据上传时间，单位秒
    unsigned int rtDatauploadTime;  //数据上传时间，单位秒
    int uploadNum;   //数据上传端口
    char serverIP[16];     //服务器IP
    char lockIP[16];    //本机IP
    char ftpUser[16];  //ftp用户名
    char ftpPass[16];  //ftp密码
    int isRun;
    int isZIP;
    int fileNum;
};

//远程服务器配置
struct ServerConfig
{
    int devId;  //设备ID
    char iotDevName[6];    //本机名称
    int Num;   //数据上传端口
    char serverIP[128];     //服务器IP
    char lockIP[16];    //本机IP
};

//线程参数
struct ThreadArgs {
    int client_sock;
};

struct channelBuf{
    channelConfig* pChannel;
};

typedef enum{
    statsType_IncClear = 1,    //!递增清0
    statsType_alwaysInc,        //!递增不清0
    statsType_alwaysDec,        //!一直递减
}statsType_e;

struct statsVarConfig{
    unsigned int id;
    unsigned int eventId;
    statsType_e type;
    char varName[64];           //!变量名称
    char varTriggerCond[128];   //!变量触发条件

    QMap<int, QString> varIdMap;
    QTime triggerTime;          //!数据的采集时间
    quint64 elapsedMs;
    bool enStats;
    QMutex rwMutex;
};

struct eventConfig{
    unsigned int id;
    char eventName[64];             //!事件名称
    unsigned int interCycleMs;               //!事件监测循环间隔单位MS
    unsigned int uploadCycleS;               //!事件上传周期单位S
    char eventTriggerCond[128];       //!事件触发条件
    QMap<int, statsVarConfig*> varGroup;
};

typedef int (*FUNCREAD)(int type, int DevAdder, int DBAdder, int len,char* fhBuf,int s_sock);
typedef int (*FUNCWRITE)(int type, int DevAdder, int DBAdder, int len, char* Buf,int s_sock);
typedef int (*FUNCINIT)(int s_sock);
struct driverFuncs{
    FUNCREAD  readData;
    FUNCWRITE writeData;
    FUNCINIT  init;
};
struct threadPar{
    interfaceConfig* pInter;
    devConfig* pDev;
    struct driverFuncs *pDrvFuncs;
};

struct deviceProperties{

};
struct charDataList{
    QMutex rtlMutex;
    QList<char *> list;
};
extern QMap<int, eventConfig*> g_eventConfig;
extern QMap<int, statsVarConfig*> g_statsVarConfig;

extern ftpConfig g_ftpConfig;
extern ServerConfig g_ServerConfig;
extern ServerConfig g_ServerConfigAuxiliary;


extern channelConfig* p_channelConfig;
extern channelConfig* p_channelConfig_t;

extern varConfig* p_varConfig;
extern varConfig* p_varConfig_t;

extern devConfig* p_devConfig;
extern devConfig* p_devConfig_t;

extern networkConfig* p_networkConfig;
extern networkConfig* p_networkConfig_t;

extern interfaceConfig* p_interfaceConfig;
extern interfaceConfig* p_interfaceConfig_t;

extern serialConfig* p_serialConfig;
extern serialConfig* p_serialConfig_t;

extern canBusConfig* p_canBusConfig;
extern canBusConfig* p_canBusConfig_t;

extern channelBuf* g_channelBuf;
extern int g_channelBuf_NUM;

extern QHash<int,varConfig* > g_varsHashTable;

extern void sdErrReadOnlyHandler();

#endif // DATACOLLECTION_MAIN_H
