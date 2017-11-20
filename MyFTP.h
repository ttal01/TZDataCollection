#ifndef MYFTP_H
#define MYFTP_H

int FTPLogin(int s_sock,const char* user,const char* pass);
int GetCode(char* revStr);
int FTPMyIsOline(int s_sock);
int GetHost(char* revStr, char* mb);
int UpFile(int s_sock, const char *filepath, const char *ftpfilename, const char *ftpServerIP, const char *ftpLocalIP);
int MyDownLoad(int s_sock, char* filename, char* ftpLocalIP, const char *ftpServerIP);
int FTPQuit(int s_sock);

#endif  /* MYFTP_H */
