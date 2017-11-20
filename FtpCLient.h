#ifndef FtpCLient_H
#define FtpCLient_H
#include <QObject>
#include <QUrl>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkAccessManager>

class FtpCLient : public QObject
{
    Q_OBJECT
public:
    FtpCLient();
    //~FtpCLient();
    // 设置地址和端口
    void setHostPort(const QString &host, int port = 21);
    // 设置登录 FTP 服务器的用户名和密码
    void setUserInfo(const QString &userName, const QString &password);
    // 上传文件
    void put(const QString &fileName, const QString &path);
    // 下载文件
    void get(const QString &path, const QString &fileName);

signals:
    void error(QNetworkReply::NetworkError);

private slots:
    // 下载过程中写文件
    void finished();

private:
    QUrl m_pUrl;
    QFile m_file;
    QNetworkAccessManager m_manager;
};

#endif // FtpCLient_H
