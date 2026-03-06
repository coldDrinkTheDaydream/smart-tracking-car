#include "tahthread.h"
#include <QDebug>
#include <QDataStream>
TahThread::TahThread(QObject *parent) : QObject(parent)
{

}
void TahThread::temp(qint32 dataSize, const QByteArray &actualData) {
    // 检查数据长度是否匹配（2个int类型的大小）
    if (dataSize == 2 * sizeof(int)) {
        QDataStream tempStream(actualData);
        // 设置为网络字节序（大端），QDataStream会自动转换为本地字节序
        tempStream.setByteOrder(QDataStream::BigEndian);

        int temp_int, temp_dec;
        // 从流中读取数据（自动完成字节序转换）
        tempStream >> temp_int >> temp_dec;

        // 转换为实际温度值
        double temperature = temp_int + temp_dec / 10.0;

        qDebug() << "接收温度数据（QDataStream转换后）：" << temperature;

        // 发射信号更新UI
        emit temperatureUpdated(temperature);
    } else {
        qDebug() << "温度数据大小不匹配，期望" << 2 * sizeof(int)
                 << "字节，实际" << dataSize << "字节";
    }
}

void TahThread::speed(qint32 dataSize, const QByteArray &actualData){
    if (dataSize == sizeof(int)) {
        QDataStream speedStream(actualData);
        // 设置为网络字节序（大端），QDataStream会自动转换为本地字节序
        speedStream.setByteOrder(QDataStream::BigEndian);

        int speed;
        // 从流中读取数据（自动完成字节序转换）
        speedStream >> speed;

        qDebug() << "接收速度数据（QDataStream转换后）：" << speed;

        // 发射信号更新UI
        emit speedUpdated(speed);
    } else {
        qDebug() << "速度数据大小不匹配，期望" << sizeof(int)
                 << "字节，实际" << dataSize << "字节";
    }
}

void TahThread::hum(qint32 dataSize, const QByteArray &actualData) {
    // 检查数据长度是否匹配（2个int类型的大小）
    if (dataSize == 2 * sizeof(int)) {
        QDataStream humStream(actualData);
        // 设置为网络字节序（大端），QDataStream会自动转换为本地字节序
        humStream.setByteOrder(QDataStream::BigEndian);

        int hum_int, hum_dec;
        // 从流中读取数据（自动完成字节序转换）
        humStream >> hum_int >> hum_dec;

        // 转换为实际温度值
        double hum = hum_int + hum_dec / 10.0;

        qDebug() << "接收湿度数据（QDataStream转换后）：" << hum;

        // 发射信号更新UI
        emit humUpdated(hum);
    } else {
        qDebug() << "湿度数据大小不匹配，期望" << 2 * sizeof(int)
                 << "字节，实际" << dataSize << "字节";
    }
}

