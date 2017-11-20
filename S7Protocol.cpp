#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "S7Protocol.h"
#include "PublicFun.h"

//softtype   plc300 2; plc400 1;plc200 3
//���� 1  ��ַ���� 0-i 1-q 2-m 3-d
//���� 2  �豸��ַ
//���� 3  db��ַ
//���� 4  ��ȡ����
//���� 5  �ظ�buf
int MyReadData(int type, int DevAdder, int DBAdder, int len,char* fhBuf,int s_sock,int softtype)
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
        // 02 F0 80 32 01 00 00 CC C1 00 0E 00 00�����������ȣ� 04��ֻ���� 01 12 0A 10 02 00 05����ȡ��ַ�ֽڳ��ȣ� 00 00(���ݿ�) 81(80-���� 81-ӳ������ 82-ӳ����� 83-M�м����  84-db��) 00 00 28����ʼ��ַ*8��
        PostBuf[postgs++]=0x03;
        //����
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        //����
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
        PostBuf[postgs++]=0x04;//��ֻ����;
        PostBuf[postgs++]=0x01;
        PostBuf[postgs++]=0x12;
        PostBuf[postgs++]=0x0A;
        PostBuf[postgs++]=0x10;
        PostBuf[postgs++]=0x02;
        //����ȡ��ַ�ֽڳ��ȣ�
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=len;
        //(���ݿ�)
        if(softtype==3)
        {
            PostBuf[postgs++]=DevAdder/256;
            PostBuf[postgs++]=DevAdder%256;
        }
        else
        {
            PostBuf[postgs++]=DBAdder/256;
            PostBuf[postgs++]=DBAdder%256;
        }
        //(80-���� 81-ӳ������ 82-ӳ����� 83-M�м����  84-db��)
        if(type==0)
            PostBuf[postgs++]=0x81;
        else if(type==1)
            PostBuf[postgs++]=0x82;
        else if(type==2)
            PostBuf[postgs++]=0x83;
        else
            PostBuf[postgs++]=0x84;
        //����ʼ��ַ*8��
        long temp=0;
        if(softtype==3)
            temp=DBAdder;
        else
            temp=DevAdder;
        temp=temp*8;
        PostBuf[postgs++]=(int)(temp/65536);
        PostBuf[postgs++]=(int)(temp/256);
        PostBuf[postgs++]=(int)(temp%256);
        //������ַ
        PostBuf[1]=postgs/65536;
        PostBuf[2]=postgs/256;
        PostBuf[3]=postgs%256;

        re=send(s_sock,PostBuf,postgs,0);
        if(re>0)
        {
            buflen=recv(s_sock, messBuf, 1024, 0);
            if(buflen>25)
            {
                //03 00 00 1b���ܳ��ȣ�����ͷ�� 02 f0 80 32 03 00 00 cc c1 00 02 00 06�����������ȣ�== 00 00 04 01 ff 04 00 10 02 49
                buflen=messBuf[15]*256+messBuf[16];
                if(len!=buflen-4)
                {
                    return 0;
                }
                else
                {
                    memcpy(fhBuf,messBuf+25,buflen-4);
                    retlen=buflen;
                }
             }
            else
                return 0;
        }
        else
            return -1;
        return retlen;
    }
    catch(...)
    {
            MyWriteLog("CS7Protocol-MyReadData");
            return -1;
    }
}


//softtype   plc300 2; plc400 1;plc200 3
//���� 1  ��ַ���� 0-i 1-q 2-m 3-d
//���� 2  �豸��ַ
//���� 3  db��ַ
//���� 4  ��ȡ����
//���� 5  д��buf
int MyWriteData(int type, char* DevAdder, char* DBAdder, int len, char* Buf,int s_sock,int softtype)
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
        // 02 F0 80 32 01 00 00 CC C1 00 0E 00 00�����������ȣ� 04��ֻ���� 01 12 0A 10 02 00 05����ȡ��ַ�ֽڳ��ȣ� 00 00(���ݿ�) 81(80-���� 81-ӳ������ 82-ӳ����� 83-M�м����  84-db��) 00 00 28����ʼ��ַ*8��
        PostBuf[postgs++]=0x03;
        //����
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=0x00;
        //����
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
        //����������
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=len+4;
        //��ֻд��;
        PostBuf[postgs++]=0x05;
        PostBuf[postgs++]=0x01;
        PostBuf[postgs++]=0x12;
        PostBuf[postgs++]=0x0A;
        PostBuf[postgs++]=0x10;
        PostBuf[postgs++]=0x02;
        //����ȡ��ַ�ֽڳ��ȣ�
        PostBuf[postgs++]=0x00;
        PostBuf[postgs++]=len;
        //(���ݿ�)
        if(softtype==1)
        {
            PostBuf[postgs++]=atoi(DevAdder)/256;
            PostBuf[postgs++]=atoi(DevAdder)%256;
        }
        else
        {
            PostBuf[postgs++]=atoi(DBAdder)/256;
            PostBuf[postgs++]=atoi(DBAdder)%256;
        }
        //(80-���� 81-ӳ������ 82-ӳ����� 83-M�м����  84-db��)
        if(type==0)
            PostBuf[postgs++]=0x81;
        else if(type==1)
            PostBuf[postgs++]=0x82;
        else if(type==2)
            PostBuf[postgs++]=0x83;
        else
            PostBuf[postgs++]=0x84;
        //����ʼ��ַ*8��
        long temp=0;
        if(softtype==1)
            temp=atol(DBAdder);
        else
            temp=atol(DevAdder);
        temp=temp*8;
        PostBuf[postgs++]=(int)(temp/65536);
        PostBuf[postgs++]=(int)(temp/256);
        PostBuf[postgs++]=(int)(temp%256);
        //������
        PostBuf[postgs++]=0;
        PostBuf[postgs++]=4;
        temp=len;
        temp=temp*8;
        PostBuf[postgs++]=(int)(temp/256);
        PostBuf[postgs++]=(int)(temp%256);
        for(int i=0;i<len;i++)
            PostBuf[postgs++]=Buf[i];
        //������ַ
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

int MyInit(int s_sock,int softtype)
{
    try
    {
        int fh=0;
        int re=0;
        int postgs=0;
        u_char PostBuf[1024];
        u_char messBuf[1024];
        //���ͳ�ʼ��
        memset(PostBuf,0x00,1024);
        memset(messBuf,0x00,1024);
        if(softtype==1)
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
            PostBuf[postgs++]=0x4d;
            PostBuf[postgs++]=0x57;
            PostBuf[postgs++]=0xc2;
            PostBuf[postgs++]=0x02;
            PostBuf[postgs++]=0x4d;
            PostBuf[postgs++]=0x57;
            PostBuf[postgs++]=0xc0;
            PostBuf[postgs++]=0x01;
            PostBuf[postgs++]=0x09;
        }
        else if(softtype==4)
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
        else
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
            PostBuf[postgs++]=0x02;
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
