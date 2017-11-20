#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include "S7Protocol.h"
#include "PublicFun.h"


char * plcS7Strerror(int code)
{
    switch (code) {
       case daveResOK: return "ok";
       case daveResMultipleBitsNotSupported:return "the CPU does not support reading a bit block of length<>1";
       case daveResItemNotAvailable: return "the desired item is not available in the PLC";
       case daveResItemNotAvailable200: return "the desired item is not available in the PLC (200 family)";
       case daveAddressOutOfRange: return "the desired address is beyond limit for this PLC";
       case daveResCPUNoData : return "the PLC returned a packet with no result data";
       case daveUnknownError : return "the PLC returned an error code not understood by this library";
       case daveEmptyResultError : return "this result contains no data";
       case daveEmptyResultSetError: return "cannot work with an undefined result set";
       case daveResCannotEvaluatePDU: return "cannot evaluate the received PDU";
       case daveWriteDataSizeMismatch: return "Write data size error";
       case daveResNoPeripheralAtAddress: return "No data from I/O module";
       case daveResUnexpectedFunc: return "Unexpected function code in answer";
       case daveResUnknownDataUnitSize: return "PLC responds with an unknown data type";

       case daveResShortPacket: return "Short packet from PLC";
       case daveResTimeout: return "Timeout when waiting for PLC response";
       case daveResNoBuffer: return "No buffer provided";
       case daveNotAvailableInS5: return "Function not supported for S5";

       case 0x8000: return "function already occupied.";
       case 0x8001: return "not allowed in current operating status.";
       case 0x8101: return "hardware fault.";
       case 0x8103: return "object access not allowed.";
       case 0x8104: return "context is not supported. Step7 says:Function not implemented or error in telgram.";
       case 0x8105: return "invalid address.";
       case 0x8106: return "data type not supported.";
       case 0x8107: return "data type not consistent.";
       case 0x810A: return "object does not exist.";
       case 0x8301: return "insufficient CPU memory ?";
       case 0x8402: return "CPU already in RUN or already in STOP ?";
       case 0x8404: return "severe error ?";
       case 0x8500: return "incorrect PDU size.";
       case 0x8702: return "address invalid."; ;
       case 0xd002: return "Step7:variant of command is illegal.";
       case 0xd004: return "Step7:status for this command is illegal.";
       case 0xd0A1: return "Step7:function is not allowed in the current protection level.";
       case 0xd201: return "block name syntax error.";
       case 0xd202: return "syntax error function parameter.";
       case 0xd203: return "syntax error block type.";
       case 0xd204: return "no linked block in storage medium.";
       case 0xd205: return "object already exists.";
       case 0xd206: return "object already exists.";
       case 0xd207: return "block exists in EPROM.";
       case 0xd209: return "block does not exist/could not be found.";
       case 0xd20e: return "no block present.";
       case 0xd210: return "block number too big.";
   //	case 0xd240: return "unfinished block transfer in progress?";  // my guess
       case 0xd240: return "Coordination rules were violated.";
   /*  Multiple functions tried to manipulate the same object.
       Example: a block could not be copied,because it is already present in the target system
       and
   */
       case 0xd241: return "Operation not permitted in current protection level.";
   /**/	case 0xd242: return "protection violation while processing F-blocks. F-blocks can only be processed after password input.";
       case 0xd401: return "invalid SZL ID.";
       case 0xd402: return "invalid SZL index.";
       case 0xd406: return "diagnosis: info not available.";
       case 0xd409: return "diagnosis: DP error.";
       case 0xdc01: return "invalid BCD code or Invalid time format?";
       default: return "no message defined!";
       }
}


//参数 1  地址类型 0-i 1-q 2-m 3-d
//参数 2  设备地址
//参数 3  db地址
//参数 4  读取长度
//参数 5  回复buf
int PLC400Read(int type, int DevAdder, int DBAdder, int len,char* fhBuf,int s_sock)
{
    try
    {
        unsigned long retlen=0;
        int postgs=0;
        int buflen=0;
        int re=0;
        u_char PostBuf[1024];
        u_char messBuf[1024];
        memset(PostBuf,0x00,1024);
        memset(messBuf,0x00,1024);
        postgs=0;
        if(s_sock==0)
            return -1;
        // 02 F0 80 32 01 00 00 CC C1 00 0E 00 00（数据区长度） 04（只读） 01 12 0A 10 02 00 05（读取地址字节长度） 00 00(数据块) 81(80-输入 81-映像输入 82-映像输出 83-M中间变量  84-db快) 00 00 28（起始地址*8）
        PostBuf[postgs++]=0x03;
        //长度
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        //不明
        PostBuf[postgs++]=0x02;
        PostBuf[postgs++]=0xF0;
        PostBuf[postgs++]=0x80;
        PostBuf[postgs++]=0x32;
        PostBuf[postgs++]=0x01;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0xCC;
        PostBuf[postgs++]=0xC1;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x0E;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x04;//（只读）;
        PostBuf[postgs++]=0x01;
        PostBuf[postgs++]=0x12;
        PostBuf[postgs++]=0x0A;
        PostBuf[postgs++]=0x10;
        PostBuf[postgs++]=0x02;
        //（读取地址字节长度）
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=len;
        //(数据块)
        {
            PostBuf[postgs++]=DBAdder/256;
            PostBuf[postgs++]=DBAdder%256;
        }
        //0x84= data block(DB); 0X82= outputs(Q); 0x81=inputs(I); 0x83= Flags(M); 0x1d= S7 timers(T); 0x1c= S7counters(C);
        if(type==0)
            PostBuf[postgs++]=0x81;
        else if(type==1)
            PostBuf[postgs++]=0x82;
        else if(type==2)
            PostBuf[postgs++]=0x83;
        else
            PostBuf[postgs++]=0x84;
        //（起始地址*8）
        long temp=0;
        temp=DevAdder;
        temp=temp*8;
        PostBuf[postgs++]=(int)(temp/65536);
        PostBuf[postgs++]=(int)(temp/256);
        PostBuf[postgs++]=(int)(temp%256);
        //给定地址
        PostBuf[1]=postgs/65536;
        PostBuf[2]=postgs/256;
        PostBuf[3]=postgs%256;
sendTryAgain:
        re=send(s_sock,PostBuf,postgs,0);
        if(re>0)
        {
            int errCount=0;
            do{
                buflen=recv(s_sock, messBuf, 1024, 0);
                if(buflen>=25)
                {
                    //03 00 00 1b（总长度，包括头）
                    //02 f0 80 (3字节COTP)
                    //32 03 00 00 cc c1 00 02 [00 06]（2字节数据区长度）00(偏移18，error class) 00(偏移19，error code)
                    //04 01
                    //ff (偏移22，return code)04(transport size) 00 10(数据长度)
                    //以上报文协议区域25字节，后续就是数据区
                    int errCode=messBuf[17]*256+messBuf[18];
                    if(errCode){
                        MyWriteLog(commRdFailed,plcS7Strerror(errCode));
                        return -1;
                    }

                    errCode = messBuf[21];
                    if(errCode!=0xff){
                        MyWriteLog(commRdFailed,plcS7Strerror(errCode));
                        return -1;
                    }

                    buflen=messBuf[15]*256+messBuf[16];
                    if(len!=buflen-4)
                    {
                        retlen = -2;
                    }
                    else
                    {
                        memcpy(fhBuf,messBuf+25,buflen-4);
                        retlen=buflen;
                        break;
                    }
                 }
                else if(buflen<0 && EAGAIN != errno){
                    MyWriteLog(commRdFailed,"ERROR: plc read: %s",strerror(errno));
                    return -1;
                }
                else if(buflen<0 && EAGAIN == errno){
                    MyWriteLog(commRdFailed,"ERROR: plc read: EAGAIN try receive.",strerror(errno));
                    continue;
                }
                else
                    retlen = 0;
            }while(errCount++<3);

            if(errCount >=3 && EAGAIN == errno)
                goto sendTryAgain;
        }
        else{
            MyWriteLog("PLC send ERROR: %s",strerror(errno));
            if(EAGAIN == errno)
                goto sendTryAgain;
            return -1;
        }
        return retlen;
    }
    catch(...)
    {
            MyWriteLog("CS7Protocol-MyReadData");
            return -1;
    }
}

//softtype   plc300 2; plc400 1;plc200 3
//参数 1  地址类型 0-i 1-q 2-m 3-d
//参数 2  设备地址
//参数 3  db地址
//参数 4  读取长度
//参数 5  写入buf
int PLC400Write(int type, int DevAdder, int DBAdder, int len, char* Buf,int s_sock)
{
    try
    {
        unsigned long retlen=0;
        int postgs=0;
        int re=0;
        u_char PostBuf[1024];
        u_char messBuf[1024];
        memset(PostBuf,0x00,1024);
        memset(messBuf,0x00,1024);
        postgs=0;
        if(s_sock==0)
            return -1;
        // 02 F0 80 32 01 00 00 CC C1 00 0E 00 00（数据区长度） 04（只读） 01 12 0A 10 02 00 05（读取地址字节长度） 00 00(数据块) 81(80-输入 81-映像输入 82-映像输出 83-M中间变量  84-db快) 00 00 28（起始地址*8）
        PostBuf[postgs++]=0x03;
        //长度
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        //不明
        PostBuf[postgs++]=0x02;
        PostBuf[postgs++]=0xF0;
        PostBuf[postgs++]=0x80;
        PostBuf[postgs++]=0x32;
        PostBuf[postgs++]=0x01;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0xCC;
        PostBuf[postgs++]=0xC1;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x0E;
        //数据区长度
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=len+4;
        //（只写）;
        PostBuf[postgs++]=0x05;
        PostBuf[postgs++]=0x01;
        PostBuf[postgs++]=0x12;
        PostBuf[postgs++]=0x0A;
        PostBuf[postgs++]=0x10;
        PostBuf[postgs++]=0x02;
        //（读取地址字节长度）
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=len;
        //(数据块)
        {
            PostBuf[postgs++]=DevAdder/256;
            PostBuf[postgs++]=DevAdder%256;
        }

        //(80-输入 81-映像输入 82-映像输出 83-M中间变量  84-db快)
        if(type==0)
            PostBuf[postgs++]=0x81;
        else if(type==1)
            PostBuf[postgs++]=0x82;
        else if(type==2)
            PostBuf[postgs++]=0x83;
        else
            PostBuf[postgs++]=0x84;
        //（起始地址*8）
        long temp=0;
        temp=DBAdder;
        temp=temp*8;
        PostBuf[postgs++]=(int)(temp/65536);
        PostBuf[postgs++]=(int)(temp/256);
        PostBuf[postgs++]=(int)(temp%256);
        //数据区
        PostBuf[postgs++]=0;
        PostBuf[postgs++]=4;
        temp=len;
        temp=temp*8;
        PostBuf[postgs++]=(int)(temp/256);
        PostBuf[postgs++]=(int)(temp%256);
        for(int i=0;i<len;i++)
            PostBuf[postgs++]=Buf[i];
        //给定地址
        PostBuf[1]=postgs/65536;
        PostBuf[2]=postgs/256;
        PostBuf[3]=postgs%256;


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
        MyWriteLog("CS7Protocol-MyReadData");
        return 0;
    }
}

int PLC400WriteData(int type, int DevAdder, int DBAdder, int len, char* Buf,int s_sock)
{
    int ret = 0, writeLen=0;
    int writeBlock = 220;

    for(int i=0; i<len; i+=writeBlock)
    {
        writeLen = (len-i) >= writeBlock? writeBlock:len-i;
        ret = PLC400Write(type, DevAdder+i, DBAdder, writeLen, Buf+i, s_sock);
        if(ret <= 0)
            return ret;
    }
    return len;
}
int PLC400ReadData(int type, int DevAdder, int DBAdder, int len,char* fhBuf,int s_sock)
{
    int ret = 0, readLen=0;
    int readBlock;
    if(type == 3)
        readBlock = 220;
    else
        readBlock = 460;

    for(int i=0; i<len; i+=readBlock)
    {
        readLen = (len-i) >= readBlock? readBlock:len-i;
        ret = PLC400Read(type, DevAdder+i, DBAdder, readLen, fhBuf+i, s_sock);
        if(ret <= 0)
            return ret;
    }
    return len;
}
int PLC400Init(int s_sock)
{
    try
    {
        int fh=0;
        int re=0;
        int postgs=0;
        u_char PostBuf[1024];
        u_char messBuf[1024];
        //发送初始化
        memset(PostBuf,0x00,1024);
        memset(messBuf,0x00,1024);
        {
            postgs=0;
            PostBuf[postgs++]=0x03;
            PostBuf[postgs++]=0x00;
            PostBuf[postgs++]=0x00;
            PostBuf[postgs++]=0x16;
            PostBuf[postgs++]=0x11;
            PostBuf[postgs++]=0xe0;
            PostBuf[postgs++]=0x00;
            PostBuf[postgs++]=0x00;
            PostBuf[postgs++]=0x00;
            PostBuf[postgs++]=0x01;
            PostBuf[postgs++]=0x00;
            PostBuf[postgs++]=0xc1;
            PostBuf[postgs++]=0x02;
            PostBuf[postgs++]=0x02;
            PostBuf[postgs++]=0x00;
            PostBuf[postgs++]=0xc2;
            PostBuf[postgs++]=0x02;
            PostBuf[postgs++]=0x02;
            PostBuf[postgs++]=0x03;
            PostBuf[postgs++]=0xc0;
            PostBuf[postgs++]=0x01;
            PostBuf[postgs++]=0x09;
        }

        re=send(s_sock,PostBuf,postgs,0);
        if(re>0)
        {
            memset(messBuf,0,1024);
            fh=recv(s_sock, messBuf, 1024, 0);
            if(fh<=0)
                return -1;
        }
        else
        {
            return -1;
        }
        memset(PostBuf,0x00,1024);
        memset(messBuf,0x00,1024);
        postgs=0;
        PostBuf[postgs++]=0x03;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x19; //25
        PostBuf[postgs++]=0x02;
        PostBuf[postgs++]=0xF0;
        PostBuf[postgs++]=0x80;
        PostBuf[postgs++]=0x32;
        PostBuf[postgs++]=0x01; //07
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0xCC;//0a
        PostBuf[postgs++]=0xC1; //d4
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x08;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00; //0c
        PostBuf[postgs++]=0xF0; //00
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x01;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x01;
        PostBuf[postgs++]=0x01;
        PostBuf[postgs++]=0xE0;

        re=send(s_sock,PostBuf,postgs,0);
        if(re>0)
        {
            fh=recv(s_sock, messBuf, 1024, 0);
            if(fh<=0)
                return -1;
        }
        else
        {
            return -1;
        }
        return fh;
    }
    catch(...)
    {
        MyWriteLog("CS7Protocol-MyInit");
        return -1;
    }
}
