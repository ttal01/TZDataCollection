#ifndef SERIALPORT_H
#define SERIALPORT_H

int InitSerialConnect(int &sockfd, unsigned int nComN, unsigned int nBaudRate, char nParity, int nByteSize, int nStopBits);

#endif // SERIALPORT_H

