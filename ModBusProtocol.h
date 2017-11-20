


class CModBusProtocol
{
public:
	CModBusProtocol(void);
	~CModBusProtocol(void);


	unsigned long MyReadData(int type, unsigned int DevAdder, int len,char* fhBuf);
	int MyWriteData(int type, unsigned int DevAdder, int len, char* Buf);
	int MyInit(char* ip,int dk,char* lockip);

	socket s_sock;
};
