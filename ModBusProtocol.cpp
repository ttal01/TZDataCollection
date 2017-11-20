#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <QDebug>
#include <QTime>
#include <termios.h>
#include "PublicFun.h"

extern int infoDbgLevel;

/* Table of CRC values for high-order byte */
static const u_char table_crc_hi[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};

/* Table of CRC values for low-order byte */
static const u_char table_crc_lo[] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
    0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
    0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
    0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
    0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
    0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
    0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
    0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
    0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
    0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
    0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
    0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
    0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
    0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
    0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
    0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
    0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
    0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
    0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
    0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
    0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};
static int crc16(char *buffer, int buffer_length)
{
    u_char crc_hi = 0xFF; /* high CRC byte initialized */
    u_char crc_lo = 0xFF; /* low CRC byte initialized */
    unsigned int i; /* will index into CRC lookup */

    /* pass through message buffer */
    while (buffer_length--) {
        i = crc_hi ^ *buffer++; /* calculate the CRC  */
        crc_hi = crc_lo ^ table_crc_hi[i];
        crc_lo = table_crc_lo[i];
    }

    return (crc_hi << 8 | crc_lo);
}
//参数 1  地址类型 0-读输入点（位）  1、2=读线圈（位）   3=读多个寄存器
//参数 2  设备地址
//参数 3  db地址
//参数 4  读取长度
//参数 5  回复buf
int ModBusTcpReadData(int type, int DevAdder, int devBeginAdder, int len,char* fhBuf, int s_sock)
{
    try
    {
        int re=0, crc=0;
        unsigned long retlen=0;
        int postgs=0;
        int buflen=0;
        char PostBuf[1024];
        char messBuf[1024];
        memset(PostBuf,0x00,1024);
        memset(messBuf,0x00,1024);
        postgs=0;
        if(s_sock==0 || len>1000){
            MyWriteLog("ERROR: ModBusRtuReadData: input args invalid!");
            return -1;
        }
        //
        PostBuf[postgs++]=0;
        PostBuf[postgs++]=0;
        PostBuf[postgs++]=0;
        PostBuf[postgs++]=0;
        //长度
        PostBuf[postgs++]=0;
        PostBuf[postgs++]=6;
        //id
        PostBuf[postgs++]=DevAdder;

        //功能码
        if(type==4){//DI Read Discrete Input
            PostBuf[postgs++]=2;
        }
        else if(type==5){//DO Read Coil
            PostBuf[postgs++]=1;
        }
        else if(type==6){//AI Read Input Registers
            PostBuf[postgs++]=4;
            len /= 2;
        }
        else{//AO Read Holding Registers
            PostBuf[postgs++]=3;
            len /= 2;
        }

        //起始地址
        PostBuf[postgs++]=devBeginAdder/256;
        PostBuf[postgs++]=devBeginAdder%256;
        //个数
        PostBuf[postgs++]=len/256;
        PostBuf[postgs++]=len%256;

        re=send(s_sock,PostBuf,postgs,0);
        if(re>0)
        {
            buflen=recv(s_sock, messBuf, 1024, 0);
            if(buflen>1&&PostBuf[7]==messBuf[7])
            {
                crc=messBuf[4]*256+messBuf[5];
                crc=crc-3;
                if(crc>0) //有数据
                {
                    memcpy(fhBuf,messBuf+9,crc);
                    retlen=crc;
                }
                else
                {
                    retlen=0;
                }
            }
            else
                retlen = -1;
        }
        else
            return -1;

        return retlen;
    }
    catch(...)
    {
        MyWriteLog("CModBusProtocol-MyReadData");
        return 0;
    }
}

//参数 1  地址类型 0-读输入点（位）  1、2=读线圈（位）   3=读多个寄存器
//参数 2  设备地址
//参数 3  地址
//参数 4  读取长度
//参数 5  回复buf
int ModBusRtuReadData(int type, int DevAdder, int devBeginAdder, int len,char* fhBuf, int s_sock)
{
    try
    {
        int re=0, crc=0, waitResponsedBytes=0;
        unsigned long retlen=0;
        int postgs=0;
        int buflen=0;
        char PostBuf[1024];
        char messBuf[1024];
        memset(PostBuf,0x00,1024);
        memset(messBuf,0x00,1024);
        postgs=0;
        if(s_sock==0 || len>1000){
            MyWriteLog("ERROR: ModBusRtuReadData: input args invalid!");
            return -1;
        }
        //id
        PostBuf[postgs++]=DevAdder;
        //功能码
        if(type==4){//DI Read Discrete Input
            PostBuf[postgs++]=2;
            waitResponsedBytes = len + 5;
        }
        else if(type==5){//DO Read Coil
            PostBuf[postgs++]=1;
            waitResponsedBytes = len + 5;
        }
        else if(type==6){//AI Read Input Registers
            PostBuf[postgs++]=4;
            waitResponsedBytes = len + 5;
            len /= 2;
        }
        else{//AO Read Holding Registers
            PostBuf[postgs++]=3;
            waitResponsedBytes = len + 5;
            len /= 2;
        }
        //起始地址
        PostBuf[postgs++]=devBeginAdder/256;
        PostBuf[postgs++]=devBeginAdder%256;
        //个数
        PostBuf[postgs++]=len/256;
        PostBuf[postgs++]=len%256;

        crc = crc16(PostBuf, postgs);

        PostBuf[postgs++]=crc/256;
        PostBuf[postgs++]=crc%256;

        //re=send(s_sock,PostBuf,postgs,0);
        re=write(s_sock,PostBuf,postgs);
        usleep(20000);//20ms
        if(infoDbgLevel&32){
            printf("modbus send:");
                for(int i; i<postgs; i++)
                    printf("%02X ", PostBuf[i]);
                printf("\n");
        }
        if(re>0)
        {
            int deservedMsg = waitResponsedBytes, receivedBytes=0, rdErrLimit = 0;
            while(deservedMsg)
            {
                buflen=read(s_sock, messBuf + receivedBytes, deservedMsg);
                if(buflen < 0){
                    if(rdErrLimit++ > 100){//2000ms
                        MyWriteLog("ERROR: ModBusRtuReadData: %s.", strerror(errno));
                        retlen = -1;
                        break;
                    }
                    if(errno != EAGAIN){
                        retlen = -1;
                        break;
                    }
                    usleep(20000);//20ms
                    continue;
                }
                deservedMsg   -= buflen;
                receivedBytes += buflen;
            }
            if(infoDbgLevel&32){
                printf("modbus rsv:");
                for(int i=0; i<receivedBytes; i++)
                    printf("%02X ", messBuf[i]);
                printf("\n");
            }
            if(0 == deservedMsg)
            {
                crc = (*(messBuf + 3 + messBuf[2]))*256 | *(messBuf + 3 + messBuf[2] + 1);
                if(crc == crc16(messBuf,3 + messBuf[2]))
                {
                    memcpy(fhBuf,messBuf+3,messBuf[2]);
                    retlen=messBuf[2];
                }
                else
                {
                    usleep(20000);//20ms
                    tcflush(s_sock, TCIOFLUSH);
                    retlen=0;
                }
            }
        }
        else
            return -1;

        return retlen;
    }
    catch(...)
    {
        MyWriteLog("CModBusProtocol-MyReadData");
        return 0;
    }
}
//参数 1  地址类型
//参数 2  设备地址
//参数 3  modbus寄存器地址
//参数 4  写长度
//参数 5  写入buf
int ModBusTcpWriteData(int type, int DevAdder, int devBeginAdder, int len,char* fhBuf, int s_sock)
{
    try
    {
        int re=0, crc=0;
        unsigned long retlen=0;
        int postgs=0;
        int wtireQuantitys=0;
        u_char PostBuf[1024];
        char messBuf[1024];
        memset(PostBuf,0x00,1024);
        memset(messBuf,0x00,1024);
        postgs=0;
        if(s_sock==0)
            return -1;
        //
        PostBuf[postgs++]=0;//transaction id
        PostBuf[postgs++]=0;
        PostBuf[postgs++]=0;//protocol id
        PostBuf[postgs++]=0;
        //长度
        PostBuf[postgs++]=0;//package length
        if(type==2)
            PostBuf[postgs++]=4+len;
        else
            PostBuf[postgs++]=4+len+3;
        //plc架号 1
        PostBuf[postgs++]=DevAdder;
        //功能码
        if(type==5){//DO Write Coil
            PostBuf[postgs++]=15;                //funcode: Write Multiple Coils
            wtireQuantitys = len;
        }
        else if(type==7){//AO Write Input Registers
            PostBuf[postgs++]=16;               //funcode: Write Multiple
            wtireQuantitys = len/2;
        }
        else{
            MyWriteLog("invalid ModBus funcode.");
            return -1;
        }

        //起始地址
        PostBuf[postgs++]=devBeginAdder/256;    //Starting Address Hi
        PostBuf[postgs++]=devBeginAdder%256;    //Starting Address Lo
        //个数
        if(type==3)
        {
            PostBuf[postgs++]=wtireQuantitys/256;    //Quantity of Registers Hi
            PostBuf[postgs++]=wtireQuantitys%256;    //Quantity of Registers Lo
            PostBuf[postgs++]=len;                  //Byte Count
        }
        memcpy(PostBuf+postgs,fhBuf,len);
        /*PostBuf[postgs++]=Buf[0];
                PostBuf[postgs++]=Buf[1];*/
        postgs=postgs+len;

        re=send(s_sock,PostBuf,postgs,0);
        if(re>0)
        {
            retlen=recv(s_sock, messBuf, 1024, 0);
        }
        else
            return -1;
        return retlen;

    }
    catch(...)
    {
        //MyWriteLog("CModBusProtocol-MyWriteData");
        return 0;
    }
}
int ModBusTcpInit(int s_sock)
{
    try
    {

    }
    catch(...)
    {
        MyWriteLog("CModBusProtocol-MyInit");
        return 0;
    }
}
