#include <QFileInfo>
#include "FtpCLient.h"

FtpCLient::FtpCLient()
{
    // 设置协议
    m_pUrl.setScheme("ftp");
}

//FtpCLient::~FtpCLient()
//{

//}

// 设置地址和端口
void FtpCLient::setHostPort(const QString &host, int port)
{
    m_pUrl.setHost(host);
    m_pUrl.setPort(port);
}

// 设置登录 FTP 服务器的用户名和密码
void FtpCLient::setUserInfo(const QString &userName, const QString &password)
{
    m_pUrl.setUserName(userName);
    m_pUrl.setPassword(password);
}

// 上传文件
void FtpCLient::put(const QString &fileName, const QString &path)
{
    QFile file(fileName);
    file.open(QIODevice::ReadOnly);
    QByteArray data = file.readAll();

    m_pUrl.setPath(path);
    m_manager.put(QNetworkRequest(m_pUrl), data);
    //QNetworkReply *pReply = m_manager.put(QNetworkRequest(m_pUrl), data);

//    connect(pReply, SIGNAL(uploadProgress(qint64, qint64)), this, SIGNAL(uploadProgress(qint64, qint64)));
//    connect(pReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SIGNAL(error(QNetworkReply::NetworkError)));
}

// 下载文件
void FtpCLient::get(const QString &path, const QString &fileName)
{
    QFileInfo info;
    info.setFile(fileName);

    m_file.setFileName(fileName);
    m_file.open(QIODevice::WriteOnly | QIODevice::Append);
    m_pUrl.setPath(path);

    QNetworkReply *pReply = m_manager.get(QNetworkRequest(m_pUrl));

    connect(pReply, SIGNAL(finished()), this, SLOT(finished()));
//    connect(pReply, SIGNAL(downloadProgress(qint64, qint64)), this, SIGNAL(downloadProgress(qint64, qint64)));
//    connect(pReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SIGNAL(error(QNetworkReply::NetworkError)));
}

// 下载过程中写文件
void FtpCLient::finished()
{
    QNetworkReply *pReply = qobject_cast<QNetworkReply *>(sender());
    switch (pReply->error()) {
    case QNetworkReply::NoError : {
        m_file.write(pReply->readAll());
        m_file.flush();
    }
        break;
    default:
        break;
    }

    m_file.close();
    pReply->deleteLater();
}
