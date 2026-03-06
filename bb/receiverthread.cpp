#include "receiverthread.h"
#include <QDebug>
#include <QDataStream>

ReceiverThread::ReceiverThread(QObject *parent) : QObject(parent), client(nullptr)
{

}

// 设置客户端套接字
void ReceiverThread::setSocket(QTcpSocket *socket)
{
    // 断开旧连接的信号槽
    if (client) {
        disconnect(client, &QTcpSocket::readyRead, this, &ReceiverThread::onReadyRead);
        disconnect(client, &QTcpSocket::disconnected, this, &ReceiverThread::onDisconnected);
    }
    client = socket;
    // 连接新的信号槽
    if (client) {
        connect(client, &QTcpSocket::readyRead, this, &ReceiverThread::onReadyRead);
        connect(client, &QTcpSocket::disconnected, this, &ReceiverThread::onDisconnected);
    }
}

void ReceiverThread::onReadyRead(){
     qDebug() << "端口" << getPort() << "的线程ID:" << QThread::currentThreadId();
    // 读取所有可用数据
       QByteArray data = client->readAll();
       m_receiveBuffer.append(data); // 将数据追加到缓冲区
  qDebug() << "接收到" << data.size() << "字节，缓冲区总大小:" << m_receiveBuffer.size() << "字节";
       // 循环处理缓冲区中的完整数据包
       while (processBuffer()) {
           // 持续处理直到缓冲区中没有完整数据包
       }
}
// 处理缓冲区中的数据
bool ReceiverThread::processBuffer() {
    // 至少需要1字节的类型标识
    if (m_receiveBuffer.size() < 1) {
        return false;
    }
    // 获取数据类型
    type =m_receiveBuffer.at(0);
    // 2. 提取数据大小（需要额外4字节）
       if (m_receiveBuffer.size() < 1 + 4) {
           return false; // 数据不足，无法提取大小
       }
       QDataStream stream(m_receiveBuffer.mid(1, 4)); // 跳过类型字节
       stream.setByteOrder(QDataStream::BigEndian); // 网络字节序
       stream >> dataSize;
       if (m_receiveBuffer.size() < 1 + 4 + dataSize) {
           qDebug() << "端口" << getPort() << "数据不完整，需要" << (1 + 4 + dataSize)
                    << "字节，实际收到" << m_receiveBuffer.size() << "字节";
           return false;
       }
       actualData.clear();
       actualData = m_receiveBuffer.mid(1 + 4, dataSize);

       // 从缓冲区移除已处理的数据
       m_receiveBuffer.remove(0, 1 + 4 + dataSize);
       qDebug() << "端口" << getPort() << "提取数据完成，类型:" << type
                << "，大小:" << dataSize << "字节，剩余缓冲区:" << m_receiveBuffer.size();
    // 根据数据类型处理
    int port = getPort();
    switch (type) {
        case 1:
             emit imageSend(port,type, dataSize, actualData);
        break;
        case 2:
            emit speedSend(dataSize,actualData);
        break;
        case 3:
            emit tempSend(dataSize,actualData);
        break;
        case 4:
            emit humSend(dataSize,actualData);
        break;
        case 5:
            emit disSend(dataSize,actualData);
        break;

        default:
            // 未知类型，移除该字节并继续
            m_receiveBuffer.remove(0, 1);
        break;
    }
}

// 处理断开连接
void ReceiverThread::onDisconnected()
{
    qDebug() << "端口" << getPort() << "连接断开";
    client = nullptr;
}

// 获取当前连接的端口号
int ReceiverThread::getPort() const
{
    if (client) {
        return client->localPort();
    }
    return -1;
}


