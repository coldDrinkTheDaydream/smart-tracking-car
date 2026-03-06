#include "disthread.h"
#include <QDebug>
#include <QDataStream>
DisThread::DisThread(QObject *parent) : QObject(parent)
{

}
void DisThread::dis(qint32 dataSize, const QByteArray &actualData) {
    // 检查数据长度是否匹配（float类型的大小）
    if (dataSize == sizeof(float)) {

        float dis;
         memcpy(&dis, actualData.data(), sizeof(float));


        qDebug() << "接收前方障碍物距离数据（直接内存解析）：" << dis;

        // 发射信号更新UI
        emit disUpdated(dis);
    } else {
        qDebug() << "距离数据大小不匹配，期望" << sizeof(float)
                 << "字节，实际" << dataSize << "字节";
    }
}


