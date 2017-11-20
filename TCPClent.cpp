#include <stdio.h>
#include <string.h>
#include <unistd.h>  //系统调用
#include <stdlib.h>  //标准库头文件
#include <fcntl.h>   //文件操作
//#include "EDAType.h"
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include "PublicFun.h"
#include "arpa/inet.h"
#include <errno.h>

int InitSocketConnect(int &sockfd,const char* lockIP,const char* serverIp,int serverPort)//连接远程服务器
{
    int re;                           // Counter of received bytes
    struct sockaddr_in remote_addr;    // Host address information
    struct sockaddr_in local_addr;    // Host address information
    struct timeval timeout={3,0};//3s


    /* Get the Socket file descriptor */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        MyWriteLog(commInfo,"ERROR: Failed to obtain Socket Descriptor: %s",strerror(errno));
        return 0;
    }
    memset( &local_addr, 0, sizeof(local_addr) );
                local_addr.sin_family = AF_INET;
                inet_pton(AF_INET,lockIP, &local_addr.sin_addr); // Net Address
                local_addr.sin_port = 0;

    setsockopt(sockfd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
    setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));

    /* Fill the socket address struct */
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;                   // Protocol Family
    remote_addr.sin_port = htons(serverPort);                 // Port number
    inet_pton(AF_INET, serverIp, &remote_addr.sin_addr); // Net Address    

    /* Try to connect the remote */
    if (connect(sockfd, (struct sockaddr *)&remote_addr,  sizeof(struct sockaddr)) == -1)
    {
        MyWriteLog (commInfo,"ERROR: Failed to connect to the host %s:%d, %s", serverIp, serverPort, strerror(errno));
        close(sockfd);
        return 0;
    }
    else
    {
        return 1;
    }
}

void CloseSocketConnect(int sockfd)
{
    shutdown(sockfd,2);
    close(sockfd);
}

int MyAgreRect(int clntSocket,u_char* buffer,int len){

    u_char buf[1024];
    memset(buffer,0,len);
    int myagrelen=0;
    int datalen=0;
    memset(buf,0,1024);
    ssize_t numBytesRcvd = recv(clntSocket, buf, 1024, 0);
    while(numBytesRcvd>0)
    {
        if(myagrelen==0)
        {
            if(buf[0]==0xa2)
                myagrelen=buf[1]*256+buf[2];
            else
                break;
            if(myagrelen>=1024)
                 break;
        }
        if(datalen+numBytesRcvd<1024)
        {
             memcpy(buffer+datalen,buf,numBytesRcvd);
             datalen+=numBytesRcvd;
        }
        else
            break;
        if(myagrelen!=0&&datalen>=myagrelen)
            break;
        memset(buf,0,1024);
        numBytesRcvd = recv(clntSocket, buf, 1024, 0);
    }

    return datalen;
}
