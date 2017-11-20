#ifndef DATYPE_H
#define DATYPE_H


ftpConfig g_ftpConfig;
ServerConfig g_ServerConfig;
ServerConfig g_ServerConfigAuxiliary;

channelConfig* p_channelConfig;
channelConfig* p_channelConfig_t;

varConfig* p_varConfig;
varConfig* p_varConfig_t;

devConfig* p_devConfig;
devConfig* p_devConfig_t;

networkConfig* p_networkConfig;
networkConfig* p_networkConfig_t;

interfaceConfig* p_interfaceConfig;
interfaceConfig* p_interfaceConfig_t;

serialConfig* p_serialConfig;
serialConfig* p_serialConfig_t;

canBusConfig* p_canBusConfig;
canBusConfig* p_canBusConfig_t;

channelBuf* g_channelBuf;
int g_channelBuf_NUM;

QHash<int,varConfig* > g_varsHashTable;

QMap<int, eventConfig*> g_eventConfig;
QMap<int, statsVarConfig*> g_statsVarConfig;

#endif // DATACOLLECTION_MAIN_H
