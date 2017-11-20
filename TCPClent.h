#ifndef TCPCLENT_H
#define TCPCLENT_H

int InitSocketConnect(int &sockfd,const char* lockIP,const char* serverIp,int serverPort);
void CloseSocketConnect(int sockfd);
int MyAgreRect(int clntSocket,u_char* buffer,int len);

#endif  /* TCPCLENT_H */
