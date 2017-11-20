#include "StdAfx.h"
#include ".\s7protocol.h"

extern void MyWriteLog(char* log);
CS7Protocol::CS7Protocol(void)
{
	s_sock=0;
	memset(m_ip,0x00,32);
	memset(LockIP,0x00,32);
}

CS7Protocol::~CS7Protocol(void)
{
	m_Socket.MyFree(s_sock);
}
//参数 1  地址类型 0-i 1-q 2-m 3-d
//参数 2  设备地址
//参数 3  db地址
//参数 4  读取长度
//参数 5  回复buf
unsigned long CS7Protocol::MyReadData(int type, char* DevAdder, char* DBAdder, int len,char* fhBuf,int PLCType)
{
	try
	{
		/////////////////////////////
		unsigned long retlen=0;
		int postgs=0;
		int buflen=0;
		UCHAR PostBuf[1024];
		UCHAR messBuf[1024];
		memset(PostBuf,0x00,1024);
		memset(messBuf,0x00,1024);
		postgs=0;
		if(s_sock==0)
			MyInit(m_ip,m_dk,LockIP,PLCType);
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
		if(PLCType==1)
		{
			PostBuf[postgs++]=atoi(DBAdder)/256;
			PostBuf[postgs++]=atoi(DBAdder)%256;
			/*PostBuf[postgs++]=atoi(DevAdder)/256;
			PostBuf[postgs++]=atoi(DevAdder)%256;*/
		}
		else
		{
			PostBuf[postgs++]=0;
			PostBuf[postgs++]=1;
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
		//if(PLCType==1)
		//{
		//	temp=atol(DevAdder);
		//	//temp=atol(DBAdder);
		//}
		//else
		//	temp=atol(DBAdder);
		temp=atol(DevAdder);
		temp=temp*8;
		PostBuf[postgs++]=(int)(temp/65536);
		PostBuf[postgs++]=(int)(temp/256);
		PostBuf[postgs++]=(int)(temp%256);
		//给定地址
		PostBuf[1]=postgs/65536;
		PostBuf[2]=postgs/256;
		PostBuf[3]=postgs%256;
		if(s_sock!=0&&m_Socket.MySend(s_sock,(char *)PostBuf,postgs)>0)
		{
			buflen=m_Socket.MyRecv(s_sock,messBuf,1);
			if(buflen>25)
			{
				//03 00 00 1b（总长度，包括头） 02 f0 80 32 03 00 00 cc c1 00 02 00 06（数据区长度）== 00 00 04 01 ff 04 00 10 02 49 
				buflen=messBuf[15]*256+messBuf[16];
				if(len!=buflen-4)
				{
					m_Socket.MyFree(s_sock);
					s_sock=0;
					retlen=0;
				}
				else
				{
					memcpy(fhBuf,messBuf+25,buflen-4);
					retlen=buflen;
				}
			}
		}
		else
		{
			m_Socket.MyFree(s_sock);
			s_sock=0;
			MyInit(m_ip,m_dk,LockIP,PLCType);
			if(s_sock!=0&&m_Socket.MySend(s_sock,(char *)PostBuf,postgs)>0)
			{
				buflen=m_Socket.MyRecv(s_sock,messBuf,1);
				if(buflen>25)
				{
					//03 00 00 1b（总长度，包括头） 02 f0 80 32 03 00 00 cc c1 00 02 00 06（数据区长度）== 00 00 04 01 ff 04 00 10 02 49 
					buflen=messBuf[15]*256+messBuf[16];
					memcpy(fhBuf,messBuf+25,buflen-4);
					retlen=buflen;
				}
			}
		}
		return retlen;
	}
	catch(...)
	{
		MyWriteLog("CS7Protocol-MyReadData");
		return 0;
	}
}

int CS7Protocol::MyInit(char* ip,int dk,char* lockip,int myplctype)
{
	try
	{
		strcpy(m_ip,ip);
		strcpy(LockIP,lockip);
		m_dk=dk;
		int fh=0;
		int postgs=0;
		UCHAR PostBuf[1024];
		UCHAR messBuf[1024];
		if(m_Socket.MyInit(s_sock,ip,dk,LockIP)==0)
		{
			fh=0;
		}
		//发送初始化
		memset(PostBuf,0x00,1024);
		memset(messBuf,0x00,1024);
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
		if (myplctype==1)
			PostBuf[postgs++]=0x02;
		else
			PostBuf[postgs++]=0x01;
		PostBuf[postgs++]=0xc0;
		PostBuf[postgs++]=0x01;
		PostBuf[postgs++]=0x09;
		if(s_sock!=0&&m_Socket.MySend(s_sock,(char *)PostBuf,postgs)>0)
		{
			fh=m_Socket.MyRecv(s_sock,messBuf,1);
		}
		else
		{
			m_Socket.MyFree(s_sock);
			s_sock=0;
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
		
		if(s_sock!=0&&m_Socket.MySend(s_sock,(char *)PostBuf,postgs)>0)
		{
			fh=m_Socket.MyRecv(s_sock,messBuf,1);
		}
		else
		{
			m_Socket.MyFree(s_sock);
			s_sock=0;
			fh=0;
		}
		return fh;
	}
	catch(...)
	{
		MyWriteLog("CS7Protocol-MyInit");
		return 0;
	}
}
