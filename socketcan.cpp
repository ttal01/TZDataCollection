#include <stdio.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/socket.h>
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/raw.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "PublicFun.h"
#define errout(_s)fprintf(stderr, "error class: %s\n", (_s))
#define errcode(_d) fprintf(stderr, "error code: %02x\n", (_d))

/*
*  扩展格式识别符由 29  位组成。其格式包含两个部分：11  位基本 ID、18  位扩展 ID。
* Controller Area Network Identifier structure *
* bit 0-28 : CAN 识别符 (11/29 bit)
* bit 29 :  错误帧标志 (0 = data frame, 1 = error frame)
* bit 30 :  远程发送请求标志 (1 = rtr frame)
* bit 31 :帧格式标志 (0 = standard 11 bit, 1 = extended 29 bit)
*/

typedef __u32 canid_t;

/*
struct can_frame {
    canid_t can_id;            //32 bit CAN_ID + EFF/RTR/ERR flags
    __u8 can_dlc;              // 数据长度: 0 .. 8
    __u8 data[8] __attribute__((aligned(8)));
};
*/


static void print_frame(struct can_frame *fr)
{
    int i;
    printf("%08x\n", fr->can_id & CAN_EFF_MASK);
    //printf("%08x\n", fr->can_id);
    printf("dlc = %d\n", fr->can_dlc);
    printf("data = ");
    for (i = 0; i < fr->can_dlc; i++)
    printf("%02x ", fr->data[i]);
    printf("\n");
}

static void handle_err_frame(const struct can_frame *fr)
{
    if (fr->can_id & CAN_ERR_TX_TIMEOUT) {
        errout("CAN_ERR_TX_TIMEOUT");
    }
    if (fr->can_id & CAN_ERR_LOSTARB) {
        errout("CAN_ERR_LOSTARB");
        errcode(fr->data[0]);
    }
    if (fr->can_id & CAN_ERR_CRTL) {
        errout("CAN_ERR_CRTL");
        errcode(fr->data[1]);
    }
    if (fr->can_id & CAN_ERR_PROT) {
        errout("CAN_ERR_PROT");
        errcode(fr->data[2]);
        errcode(fr->data[3]);
    }
    if (fr->can_id & CAN_ERR_TRX) {
        errout("CAN_ERR_TRX");
        errcode(fr->data[4]);
    }
    if (fr->can_id & CAN_ERR_ACK) {
        errout("CAN_ERR_ACK");
    }
    if (fr->can_id & CAN_ERR_BUSOFF) {
        errout("CAN_ERR_BUSOFF");
    }
    if (fr->can_id & CAN_ERR_BUSERROR) {
        errout("CAN_ERR_BUSERROR");
    }
    if (fr->can_id & CAN_ERR_RESTARTED) {
        errout("CAN_ERR_RESTARTED");
    }
}
//!sockfd 初始化后输出的接口，nCanN can物理口，nBaudRate can口波特率
int InitSocketCanConnect(int &sockfd, unsigned int nCanN, unsigned int nBaudRate)
{
    char canPort[16],sysCmd[128];
    int ret;
    struct sockaddr_can addr;
    struct ifreq ifr;

    MyWriteLog(sysExecFlow,"InitSocketCanConnect");

    if(nCanN > 0 && nCanN < 3){
        nCanN -= 1;
        sprintf(canPort, "can%d", nCanN);
    }
    else if(nCanN >= 11){
        nCanN -= 11;
        sprintf(canPort, "can%d", nCanN);
    }
    else{
        MyWriteLog(commInfo,"invalid can port!");
        return 0;
    }
    MyWriteLog("physical canPort : %s %d.", canPort, nBaudRate);
    if(nBaudRate)
    {
        sprintf(sysCmd, "ifconfig %s down", canPort);
        system(sysCmd);
        sleep(1);
        sprintf(sysCmd, "/sbin/ip link set %s type can bitrate %d", canPort, nBaudRate);
        system(sysCmd);
        sleep(1);
        sprintf(sysCmd, "ifconfig %s up", canPort);
        system(sysCmd);
        sleep(1);
    }

    sockfd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sockfd < 0) {
        MyWriteLog(commInfo,"socket PF_CAN failed");
        return 0;
    }
    /*  把套接字绑定到 can0 接口*/
    strcpy(ifr.ifr_name, canPort );
    ret = ioctl(sockfd, SIOCGIFINDEX, &ifr);
    if (ret < 0) {
        MyWriteLog(commInfo,"socket can ioctl failed");
        return 0;
    }
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        MyWriteLog(commInfo,"socket can bind failed");
        return 0;
    }
    MyWriteLog(sysExecFlow, "InitSocketCanConnect END.");
    return 1;
}

//!type 协议指令，DevAdder设备地址，devBeginAdder通道起始地址，len通道数据长度，fhBuf通道数据区，sockfd 文件描述符
int socketCanRead(int type, int DevAdder, int devBeginAdder, int len,char* fhBuf, int sockfd)
{
    static int errCnt;
    int nbytes, ret;
    type = type;
    devBeginAdder = devBeginAdder;
    struct timeval tv;
    fd_set rset;
    unsigned char resetCmd[8]={0x1C, 0x00, 0x48, 0x4E, 0x43, 0x5A, 0x44, 0x4B};

    struct can_frame frame;
    struct can_filter filter;

    if(DevAdder > 0xFFF )
    {
        MyWriteLog(commRdFailed, "ERROR: can socket read invalid DevAdder");
        return 0;
    }


    tv.tv_sec = 6;
    tv.tv_usec = 0;
    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    ret = select(sockfd+1, &rset, NULL, NULL, &tv);
    if (ret == 0) {
        MyWriteLog("Canbus time out.");
        errCnt++;
        if(errCnt > 2)
        {
            memset(&frame, 0, sizeof(frame));
            frame.can_dlc = 8;
            memcpy(frame.data, resetCmd, sizeof(resetCmd));
            nbytes = write(sockfd, &frame, sizeof(struct can_frame));
            if(nbytes<0){
                MyWriteLog(commWrFailed, "send reset to Canbus device failed.");
            }else{
                MyWriteLog(commWrFailed, "send reset to Canbus device successful.");
            }
            errCnt = 0;
        }
        return 0;
    }
    memset(&frame, 0, sizeof(frame));
    nbytes = read(sockfd, &frame, sizeof(struct can_frame));
    if (nbytes < 0) {
        MyWriteLog(commRdFailed, "socket can raw socket read");
        return 0;
    }

    /* paranoid check ... */
    if (nbytes < sizeof(struct can_frame)) {
        MyWriteLog(commRdFailed, "socket can read: incomplete CAN frame.");
        return 0;
    }

    if (frame.can_id & CAN_ERR_FLAG) { /*  检查数据帧是否错误*/
        handle_err_frame(&frame);
        MyWriteLog(commRdFailed, "CAN device error");
        return 0;
    }

    //print_frame(&frame);
    if(len > frame.can_dlc)
        len = frame.can_dlc;
    //unsigned char chanlId[8]={00, 00,  01, 00, 3, 00, 67, 00,};
    //can 总线低字节先发送，接收到数据顺序：
    //通道1低字节、通道1高字节，通道2低字节、通道2高字节，通道3低字节、通道3高字节，通道4低字节、通道4高字节，
    for(int i=0; i<len; i+=2)
    {
        if(frame.data[i] == 0 && frame.data[i+1] == 0xaa)
            frame.data[i] = 0, frame.data[i+1] = 0;
        else{
            uint8_t swapTmp = frame.data[i];
            frame.data[i] = frame.data[i+1];
            frame.data[i+1] = swapTmp;
            //if(frame.data[i] != chanlId[i])
            //    printf("Canbus error chanl %d : %d %d\n", i/2+1, frame.data[i], frame.data[i+1]);
        }
    }
    memcpy(fhBuf, frame.data, len);

    return 1;
}


int SocketCanInit(int s_sock)
{
    return 1;
}

