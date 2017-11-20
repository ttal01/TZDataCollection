#ifndef SOCKETCAN_H
#define SOCKETCAN_H


int InitSocketCanConnect(int &sockfd, unsigned int nCanN, unsigned int nBaudRate);
int SocketCanInit(int s_sock);
int socketCanRead(int type, int DevAdder, int devBeginAdder, int len,char* fhBuf, int sockfd);

#endif // SOCKETCAN_H

