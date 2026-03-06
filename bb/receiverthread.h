#ifndef RECEIVERTHREAD_H
#define RECEIVERTHREAD_H

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QThread>


class ReceiverThread : public QObject
{
    Q_OBJECT
public:
    explicit ReceiverThread(QObject *parent = nullptr);
    void setSocket(QTcpSocket *socket);
signals:
     void imageSend(int port,char type, qint32 dataSize, const QByteArray &actualData);
     void speedSend(qint32 dataSize,const QByteArray &actualData);
     void tempSend(qint32 dataSize, const QByteArray &actualData);
     void humSend(qint32 dataSize, const QByteArray &actualData);
     void disSend(qint32 dataSize, const QByteArray &actualData);

private slots:
    void onReadyRead();
    void onDisconnected();


private:
    QTcpSocket *client;
    char type;
    quint32 dataSize ;
    QByteArray m_receivedData;            // 主线程缓存数据（仅暂存，不处理）
    QString data;  // 包含图像数据的字符串
    int pos = 0;   // 解析位置指针
    QByteArray m_receiveBuffer;
    QByteArray actualData;
    bool processBuffer() ;
    int getPort() const; // 获取当前连接的端口号
};

#endif // RECEIVERTHREAD_H
