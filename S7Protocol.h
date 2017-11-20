#ifndef S7PROTOCOL_H
#define S7PROTOCOL_H

/*
    Result codes. Genarally, 0 means ok,
    >0 are results (also errors) reported by the PLC
    <0 means error reported by library code.
*/
#define daveResOK 0				/* means all ok */
#define daveResNoPeripheralAtAddress 1		/* CPU tells there is no peripheral at address */
#define daveResMultipleBitsNotSupported 6 	/* CPU tells it does not support to read a bit block with a */
                        /* length other than 1 bit. */
#define daveResItemNotAvailable200 3		/* means a a piece of data is not available in the CPU, e.g. */
                        /* when trying to read a non existing DB or bit bloc of length<>1 */
                        /* This code seems to be specific to 200 family. */

#define daveResItemNotAvailable 10		/* means a a piece of data is not available in the CPU, e.g. */
                        /* when trying to read a non existing DB */

#define daveAddressOutOfRange 5			/* means the data address is beyond the CPUs address range */
#define daveWriteDataSizeMismatch 7		/* means the write data size doesn't fit item size */
#define daveResCannotEvaluatePDU -123    	/* PDU is not understood by libnodave */
#define daveResCPUNoData -124
#define daveUnknownError -125
#define daveEmptyResultError -126
#define daveEmptyResultSetError -127
#define daveResUnexpectedFunc -128
#define daveResUnknownDataUnitSize -129
#define daveResNoBuffer -130
#define daveNotAvailableInS5 -131
#define daveResInvalidLength -132
#define daveResInvalidParam -133
#define daveResNotYetImplemented -134

#define daveResShortPacket -1024
#define daveResTimeout -1025

int PLC300ReadData(int type, int DevAdder, int DBAdder, int len, char* fhBuf,int s_sock);
int PLC300WriteData(int type, int DevAdder, int DBAdder, int len, char* Buf,int s_sock);
int PLC300Init(int s_sock);

int PLC200ReadData(int type, int DevAdder, int DBAdder, int len,char* fhBuf,int s_sock);
int PLC200WriteData(int type, int DevAdder, int DBAdder, int len, char* Buf,int s_sock);
int PLC200Init(int s_sock);

int PLC400ReadData(int type, int DevAdder, int DBAdder, int len,char* fhBuf,int s_sock);
int PLC400WriteData(int type, int DevAdder, int DBAdder, int len, char* Buf,int s_sock);
int PLC400Init(int s_sock);

int ModBusRtuReadData(int type, int DevAdder, int devBeginAdder, int len,char* fhBuf, int s_sock);
int ModBusTcpReadData(int type, int DevAdder, int devBeginAdder, int len,char* fhBuf, int s_sock);
int ModBusTcpInit(int s_sock);

int GpsReadData(int type, int DevAdder, int devBeginAdder, int len,char* fhBuf, int s_sock);
int GpsInit(int s_sock);

int InitSocketAIConnect(int &sockfd);
int AIReadData(int type,int DevAdder,int devBeginAdder,int len,char *fhBuf,int sockfd);

int InitSocketDIConnect(int &sockfd);
int DIReadData(int type,int DevAdder,int devBeginAdder,int len,char *fhBuf,int sockfd);

char * plcS7Strerror(int code);

#endif  /* S7PROTOCOL_H */
