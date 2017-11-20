#ifndef CONFIGFILE_H
#define CONFIGFILE_H


void LoadChannelConfigFile(void);
void LoadVarConfigFile(void);
void InitLoadChannelConfig(void);
void InitVarConfig(void);
void LoadFTPConfigFile(void);
int LoadConfigFile(void); //加载配置文件
void LoadDevConfigFile(void);
void LoadnetworkConfigFile(void);
void InitChannelConfig(void);
void InitDevConfig(void);
void InitInterfaceConfig(void);
void LoadSerialConfigFile(void);
int extractVarTable(QString &varTab);

void flushVarToFile();
channelConfig* getChanlConfig(int varAddrParam1, int varAddrParam2, int dataLeng);

int replaceConfigColumn(char *file, int row, int column, int setValue);
int replaceConfigColumn(char *file, int row, int column, char* setString);
int replaceConfigswitchServer(QString serverIp, QString serverPort);
char *getChanlTypeStr(int chanlType);

#endif  /* CONFIGFILE_H */
