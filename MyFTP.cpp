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
#include "MyFTP.h"
#include "PublicFun.h"
#include "TCPClent.h"


int GetHost(char* revStr,char* mb)
{
    int index=0;
    char temp[16];
    int js=0;
    int dk=0;
    char* tt=strstr(revStr,"(");
    if (tt!=NULL)
    {
        //1
        char* czlx=strstr(tt+1,",");
        if (czlx==NULL)
                return 0;
        index=czlx-tt;
        index--;
        memcpy(mb,tt+1,index);
        js+=index;
        memcpy(mb+js,".",1);
        js++;
        tt=czlx;
        //2
        czlx=strstr(tt+1,",");
        if (czlx==NULL)
                return 0;
        index=czlx-tt;
        index--;
        memcpy(mb+js,tt+1,index);
        js+=index;
        memcpy(mb+js,".",1);
        js++;
        tt=czlx;
        //3
        czlx=strstr(tt+1,",");
        if (czlx==NULL)
                return 0;
        index=czlx-tt;
        index--;
        memcpy(mb+js,tt+1,index);
        js+=index;
        memcpy(mb+js,".",1);
        js++;
        tt=czlx;
        //4
        czlx=strstr(tt+1,",");
        if (czlx==NULL)
                return 0;
        index=czlx-tt;
        index--;
        memcpy(mb+js,tt+1,index);
        js+=index;
        tt=czlx;
        //5
        czlx=strstr(tt+1,",");
        if (czlx==NULL)
                return 0;
        index=czlx-tt;
        index--;
        memset(temp,0x00,16);
        memcpy(temp,tt+1,index);
        dk=atoi(temp)*256;
        tt=czlx;

        czlx=strstr(tt+1,")");
        if (czlx==NULL)
                return 0;
        index=czlx-tt;
        index--;
        memset(temp,0x00,16);
        memcpy(temp,tt+1,index);
        dk=dk+atoi(temp);
    }
    return dk;
}

int FTPQuit(int s_sock)
{
    char buffer[1024];
    int recLen ;
    char buf[128];

    memset(buf,0,128);
    strcpy(buf,"QUIT \r\n");
    recLen=send(s_sock,buf,strlen(buf),0);
    if(recLen<=0)
        return 0;

    memset(buffer,0x00,1024);
    recLen=recv(s_sock, buffer, 1024, 0);
    if(recLen<=0)
        return 0;
    buffer[recLen]=0;
    if(GetCode(buffer) == 221)
        recLen=1;
    else
        recLen=0;
    return recLen;
}

int FTPMyIsOline(int s_sock)
{
    char buffer[1024];
    int recLen ;
    char buf[128];

    memset(buf,0,128);
    strcpy(buf,"CWD / \r\n");
    recLen=send(s_sock,buf,strlen(buf),0);
    if(recLen<=0)
        return 0;

    memset(buffer,0x00,1024);
    recLen=recv(s_sock, buffer, 1024, 0);
    if(recLen<=0)
        return 0;
    buffer[recLen]=0;
    if(GetCode(buffer) == 250)
        recLen=1;
    else
        recLen=0;
    //MyDebug(buffer);
    return recLen;
}

int GetCode(char* revStr)
{
    try
    {
        int index ;
        char temp[16];
        char* tt=strstr(revStr," ");
        if (tt!=NULL)
        {
            index = tt-revStr;
            memset(temp,0x00,16);
            memcpy(temp,revStr,index);
            index=atoi(temp);
            //printf("ftp rsv cmd [%s]. <%d>\n", revStr, index);
            return index;
        }
        else
            return 0;
    }
    catch(...)
    {
        return 0;
    }

}

int FTPLogin(int s_sock,const char* user,const char* pass)
{
    try
    {
        if (s_sock==0)
                return 0;
        //接收响应
        char  buffer[1024];
        char buf[128];
        int recLen ;

        memset(buffer,0x00,1024);
        recLen=recv(s_sock, buffer, 1024, 0);
        if(recLen<=0)
            return 0;
        buffer[recLen]=0;

        memset(buf,0x00,128);
        sprintf(buf,"USER %s\r\n",user);

        //printf("发送用户名...");
        recLen=send(s_sock,buf,strlen(buf),0);
        if(recLen<=0)
            return 0;
        //接收响应
        memset(buffer,0x00,1024);
        recLen=recv(s_sock, buffer, 1024, 0);
        if(recLen<=0)
            return 0;
        buffer[recLen]=0;
        if(GetCode(buffer) == 331)
        {
            //printf("服务器要求验证密码。");
            memset(buf,0x00,128);
            sprintf(buf,"PASS %s\r\n",pass);
            //printf("发送密码...");
            recLen=send(s_sock,buf,strlen(buf),0);
            if(recLen<=0)
                return 0;
            memset(buffer,0x00,1024);
            recLen=recv(s_sock, buffer, 1024, 0);
            if(recLen<=0)
                return 0;
            buffer[recLen]=0;
            int tryTimes = 3;
            while(GetCode(buffer) != 230 && tryTimes > 0)
            {
                memset(buffer,0x00,1024);
                recLen=recv(s_sock, buffer, 1024, 0);
                if(recLen<=0)
                    return 0;
                buffer[recLen]=0;
                tryTimes --;
                if(GetCode(buffer)==230)
                        break;

            }
            if(tryTimes < 0)
            {
                    //printf("登录失败！\n");
                    return 0;
            }
            else
                    ;//printf("密码验证成功成功！\n");
        }
        memset(buf,0x00,128);
        strcpy(buf,"PWD \r\n");
        recLen=send(s_sock,buf,strlen(buf),0);
        if(recLen<=0)
            return 0;
        memset(buffer,0x00,1024);
        recLen=recv(s_sock, buffer, 1024, 0);
        if(recLen<=0)
            return 0;
        buffer[recLen]=0;

        memset(buf,0x00,128);
        strcpy(buf,"TYPE I \r\n");
        recLen=send(s_sock,buf,strlen(buf),0);
        if(recLen<=0)
            return 0;
        memset(buffer,0x00,1024);
        recLen=recv(s_sock, buffer, 1024, 0);
        if(recLen<=0)
            return 0;
        buffer[recLen]=0;
        if(GetCode(buffer) != 200)
            return 0;
        return 1;
    }
    catch(...)
    {
        return 0;
    }
}

int UpFile(int s_sock,const char* filepath,const char* ftpfilename,const char* ftpServerIP,const char* ftpLocalIP)
{
    char buffer[1024];
    char buf[128];
    char server[32];
    int dk;
    int recLen ;
    int dataClient=0;

    memset(buf,0x00,128);
    strcpy(buf,"PASV \r\n");
    recLen=send(s_sock,buf,strlen(buf),0);
    if(recLen<=0)
    {
        return 0;
    }
    memset(buffer,0x00,1024);
    recLen=recv(s_sock, buffer, 1024, 0);
    if(recLen<=0)
    {
        return 0;
    }
    buffer[recLen]=0;
    if(GetCode(buffer) != 227)
    {
        return 0;
    }
    memset(server,0x00,32);
    dk = GetHost(buffer,server);


    if(InitSocketConnect(dataClient,ftpLocalIP,ftpServerIP,dk)>0)
    {
        //AddMsg("正在下载文件...");
        memset(buf,0x00,128);
        sprintf(buf,"STOR %s \r\n",ftpfilename);
        recLen=send(s_sock,buf,strlen(buf),0);
        if(recLen<=0)
        {
            return 0;
        }
        memset(buffer,0x00,1024);
        recLen=recv(s_sock, buffer, 1024, 0);
        if(recLen<=0)
        {
            return 0;
        }
        buffer[recLen]=0;

        if(GetCode(buffer) != 150&&GetCode(buffer) != 125)
        {
            return 0;
        }
        else
        {
            FILE *f=NULL;
            f = fopen(filepath,"rb+");
            if(f!=NULL)
            {
                int ret, jjjj=0;
                do
                {
                    memset(buffer,0x00,1024);
                    jjjj=fread(buffer,1,1024,f);
                    if (jjjj>0)
                    {
                        jjjj = send(dataClient,buffer,jjjj,0);
                    }
                } while (jjjj>0);
                fclose(f);
            }
        }
        CloseSocketConnect(dataClient);
        int retry = 3;
        while(retry--){
            //wait for server reply: 226 Transfer complete.
            recLen=recv(s_sock, buffer, 1024, 0);
            if(recLen<=0){
                usleep(10000);//return 0;
            }
            else{
                //printf("ftp rsv [%s].\n", buffer);
                break;
            }
        }
        buffer[recLen]=0;

    }
    return 1;
}
int MyDownLoad(int s_sock,char* filename,char* ftpLocalIP,const char* ftpServerIP)
{
    try
    {
        char buffer[1024];
        char buf[128];
        char server[32];
        char filepath[128];
        int dk;
        int recLen ;
        int dataClient=0;

        memset(buf,0x00,128);
        strcpy(buf,"PASV \r\n");
        recLen=send(s_sock,buf,strlen(buf),0);
        if(recLen<=0)
        {
            return 0;
        }

        memset(buffer,0x00,1024);
        recLen=recv(s_sock, buffer, 1024, 0);
        if(recLen<=0)
        {
            return 0;
        }
        buffer[recLen]=0;
        while(GetCode(buffer) != 227)
        {
            return 0;
        }
        memset(server,0x00,32);
        dk = GetHost(buffer,server);

        if(InitSocketConnect(dataClient,ftpLocalIP,ftpServerIP,dk)>0)
        {
            memset(buf,0x00,128);
            sprintf(buf,"RETR %s\r\n",filename);
            recLen=send(s_sock,buf,strlen(buf),0);
            if(recLen<=0)
            {
                return 0;
            }
            memset(buffer,0x00,1024);
            recLen=recv(s_sock, buffer, 1024, 0);
            if(recLen<=0)
            {
                return 0;
            }
            buffer[recLen]=0;
            while(GetCode(buffer) != 150&&GetCode(buffer) != 125)
            {
                return 0;
            }

            memset(buffer,0x00,1024);
            recLen=recv(dataClient, buffer, 1024, 0);
            if(recLen<=0)
            {
                return 0;
            }
            FILE *f=NULL;
            memset(filepath,0,128);
            sprintf(filepath,"/tmp/%s",filename);
            f = fopen(filepath,"wb+");
            if(f!=NULL)
            {
                while (recLen>0)
                {
                    fwrite(buffer,recLen,1,f);
                    memset(buffer,0x00,1024);
                    recLen=recv(dataClient, buffer, 1024, 0);
                } ;
                fclose(f);
            }
            CloseSocketConnect(dataClient);
            recLen=recv(s_sock, buffer, 1024, 0);
            if(recLen<=0)
                return 0;
            buffer[recLen]=0;
            return 1;
        }
        return 0;
    }
    catch(...)
    {
            return 0;
    }
}
