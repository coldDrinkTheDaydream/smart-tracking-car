#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QDateTime>  // 用于生成唯一文件名（时间戳）
#include <QDataStream>
#include <QBuffer>
#include <iostream>
#include <winsock2.h>
#include <QThread>
#pragma comment(lib, "ws2_32.lib")  // 链接Winsock库
using namespace std;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //初始化图像TCP服务器(12345)
    imageServer =  new QTcpServer(this);
    connect(imageServer, &QTcpServer::newConnection, this, &MainWindow::on_newImageConnection);
    // 启动服务器
    if (!imageServer->listen(QHostAddress::Any, 12345)) {
        QMessageBox::critical(this, "错误", "图像服务器启动失败: " + imageServer->errorString());
        close();
     } else {
        qDebug() << "图像服务器已启动，监听端口: 12345";
     }
    // 初始化控制服务器（端口12346）
    otherServer = new QTcpServer(this);
    connect(otherServer, &QTcpServer::newConnection, this, &MainWindow::on_newOtherConnection);
    if (!otherServer->listen(QHostAddress::Any, 12346)) {
        QMessageBox::critical(this, "错误", "其他控制服务器启动失败: " + otherServer->errorString());
    } else {
        qDebug() << "其他控制服务器已启动，监听端口: 12346";
    }
    disServer =  new QTcpServer(this);
    connect(disServer, &QTcpServer::newConnection, this, &MainWindow::on_newDisConnection);
    // 启动服务器
    if (!disServer->listen(QHostAddress::Any, 12347)) {
        QMessageBox::critical(this, "错误", "距离服务器启动失败: " + disServer->errorString());
        close();
     } else {
        qDebug() << "距离服务器已启动，监听端口: 12347";
     }

    // 初始化处理线程（纯线程，不包含业务逻辑）
       imageRThread = new QThread(this);
       otherRThread = new QThread(this);
       disRThread = new QThread(this);

       imageReceiver = new ReceiverThread();
       otherReceiver = new ReceiverThread();
       disReceiver = new ReceiverThread();

       imageReceiver->moveToThread(imageRThread);
       otherReceiver->moveToThread(otherRThread);
       disReceiver->moveToThread(disRThread);
     //初始化接收线程
     //workerThread = new QThread();

    imageThread = new ImageThread();
    tahThread = new TahThread();
    disThread = new DisThread();
     // receiverThread->moveToThread(workerThread);
     // 连接信号槽
     //connect(receiverThread, &ReceiverThread::temperatureData, this, &MainWindow::on_temperatureReceived);
     connect(imageReceiver, &ReceiverThread::imageSend, imageThread, &ImageThread::image);
     connect(otherReceiver, &ReceiverThread::speedSend, tahThread, &TahThread::speed);
     connect(otherReceiver, &ReceiverThread::tempSend, tahThread, &TahThread::temp);
     connect(otherReceiver, &ReceiverThread::humSend, tahThread, &TahThread::hum);
     connect(disReceiver, &ReceiverThread::disSend, disThread, &DisThread::dis);
     //connect(workerThread, &QThread::finished, receiverThread, &QObject::deleteLater);
     connect(imageThread, &ImageThread::imageProcessed, this, &MainWindow::onImageProcessed);
     connect(tahThread, &TahThread::temperatureUpdated,this, &MainWindow::onTemperatureUpdated,Qt::QueuedConnection);
     connect(tahThread, &TahThread::speedUpdated,this, &MainWindow::onSpeedUpdated,Qt::QueuedConnection);
     connect(tahThread, &TahThread::humUpdated,this, &MainWindow::onHumUpdated,Qt::QueuedConnection);
     connect(disThread, &DisThread::disUpdated,this, &MainWindow::onDisUpdated,Qt::QueuedConnection);



     // 启动线程
     imageRThread->start();
     otherRThread->start();
     disRThread->start();
}

MainWindow::~MainWindow()
{
    // 停止线程
        imageRThread->quit();
        imageRThread->wait();
        otherRThread->quit();
        otherRThread->wait();
        disRThread->quit();
        disRThread->wait();

        // 删除处理器和线程
        delete imageReceiver;
        delete otherReceiver;
        delete disReceiver;
        delete imageRThread;
        delete otherRThread;
        delete disRThread;

    delete imageThread;
    delete tahThread;
    delete ui;
}

// 图像端口新连接处理
void MainWindow::on_newImageConnection()
{
    if (imageSocket) {
        imageSocket->disconnectFromHost();
        imageSocket->deleteLater();
    }//有连接，先断开
    imageSocket = imageServer->nextPendingConnection();//获取新连接
     // 将 socket 设置到处理器（会在对应线程中处理数据）
    imageReceiver->setSocket(imageSocket);//将客户端套接字传递给接收线程
    connect(imageSocket, &QTcpSocket::disconnected, [this]() {
        imageReceiver->setSocket(nullptr);
        imageSocket = nullptr;
    });//连接断开信号
}

// 其他控制端口新连接处理
void MainWindow::on_newOtherConnection()
{
    if (otherSocket) {
        otherSocket->disconnectFromHost();
        otherSocket->deleteLater();
    }
    otherSocket = otherServer->nextPendingConnection();
    otherReceiver->setSocket(otherSocket);
    connect(otherSocket, &QTcpSocket::disconnected, [this]() {
        otherReceiver->setSocket(nullptr);
        otherSocket = nullptr;
    });
}

void MainWindow::on_newDisConnection()
{
    if (disSocket) {
        disSocket->disconnectFromHost();
        disSocket->deleteLater();
    }//有连接，先断开
    disSocket = disServer->nextPendingConnection();//获取新连接
    disReceiver->setSocket(disSocket);//将客户端套接字传递给接收线程
    connect(disSocket, &QTcpSocket::disconnected, [this]() {
        disReceiver->setSocket(nullptr);
        disSocket = nullptr;
    });//连接断开信号
}

// 更新speed spinbox的槽函数
void MainWindow::onSpeedUpdated(int speed)
{
    qDebug() << "主线程接收速度：" << speed;
    ui->speed_sb->setValue(speed);  // 更新SpinBox显示
}

// 实现槽函数：更新UI显示温度
void MainWindow::onTemperatureUpdated(double temperature)
{
    qDebug() << "主线程接收温度：" << temperature;
    ui->temperature->setValue(temperature);  // 更新SpinBox显示
}
void MainWindow::onHumUpdated(double hum)
{
    qDebug() << "主线程接收湿度：" << hum;
    ui->humidity->setValue(hum);  // 更新SpinBox显示
}

void MainWindow::onDisUpdated(float dis)
{
    qDebug() << "主线程接收前方障碍物距离：" << dis;
    ui->o_distance->setValue(dis);  // 更新SpinBox显示
}
void MainWindow::onImageProcessed(const QImage &image)
{
    // 在UI线程中更新图像显示
    if (!image.isNull()) {
        QPixmap pixmap = QPixmap::fromImage(image);
        // 调整图像大小以适应显示区域，保持比例
        pixmap = pixmap.scaled(ui->display->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->display->setPixmap(pixmap);
    }
}


void MainWindow::on_forward_button_clicked()
{
    //QString msg = "forward";
    // QDataStream writer(socket);
    // writer << msg;
    sendCommand("forward");
}
// 提取的通用发送命令函数
void MainWindow::sendCommand(const QString& cmd)
{
    if (!otherSocket || otherSocket->state() != QTcpSocket::ConnectedState) {
        QMessageBox::warning(this, "警告", "未连接到下位机，无法发送命令！");
        return;
    }
    // 发送命令：注意下位机代码处理了「前4字节是数据长度」的逻辑，需保持一致
    QByteArray data;
    QDataStream writer(&data, QIODevice::WriteOnly);
    QByteArray cmdData = cmd.toUtf8();
    // 先写4字节长度（下位机代码会跳过这4字节），再写命令
    writer << (quint32)cmdData.size();
    writer.writeRawData(cmdData.data(), cmdData.size());  // 命令内容
    otherSocket->write(data);
    otherSocket->flush(); // 确保数据立即发送
    qDebug() << "发送命令：" << cmd;
}
void MainWindow::on_left_button_pressed()
{
    sendCommand("left_turn");
}

void MainWindow::on_left_button_released()
{
    sendCommand("forward");
}

void MainWindow::on_stop_button_clicked()
{
    sendCommand("stop");
}

void MainWindow::on_right_button_pressed()
{
     sendCommand("right_turn");
}

void MainWindow::on_right_button_released()
{
     sendCommand("forward");
}

void MainWindow::on_back_button_pressed()
{
     sendCommand("backward");
}

void MainWindow::on_back_button_released()
{
     sendCommand("stop");
}

void MainWindow::on_speed_sb_valueChanged(int arg1)
{
    // 构造包含SpinBox值的命令字符串
    // 格式可以自定义，例如 "spinbox:100" 表示值为100
    QString cmd = QString("spinbox:%1").arg(arg1);

    // 使用之前创建的通用发送函数发送数据
    sendCommand(cmd);
}

void MainWindow::on_ac_button_pressed()
{
     sendCommand("ac");
}

void MainWindow::on_ac_button_released()
{
     sendCommand("ac_cancel");
}



void MainWindow::on_sound_button_clicked()
{
     sendCommand("beep");
}

void MainWindow::on_dc_button_pressed()
{
    sendCommand("de");
}

void MainWindow::on_dc_button_released()
{
    sendCommand("de_cancel");
}

void MainWindow::on_hand_rbutton_toggled(bool checked)
{
    // 只在单选按钮被选中时执行操作
    if (checked) {
        // 确保car_control分组框存在
        if (ui->car_control) {
            // 遍历car_control分组框中的所有子组件
            foreach (QWidget *widget, ui->car_control->findChildren<QWidget*>()) {
                // 启用每个组件
                widget->setEnabled(true);
            }

            // 同时确保分组框本身也是启用状态
            ui->car_control->setEnabled(true);
        }
     sendCommand("auto_cancel");
    }
}

void MainWindow::on_auto_rbutton_toggled(bool checked)
{
    // 只在单选按钮被选中时执行操作
    if (checked) {
        // 检查car_control分组框是否存在
        if (ui->car_control) {
            // 遍历car_control分组框中的所有子组件并禁用
            foreach (QWidget *widget, ui->car_control->findChildren<QWidget*>()) {
                widget->setEnabled(false);
            }

            // 同时禁用分组框本身
            ui->car_control->setEnabled(false);
        }
     sendCommand("auto");
    }

}

void MainWindow::on_avoid_checkBox_toggled(bool checked)
{
    // 只在单选按钮被选中时执行操作
    if (checked) {
          sendCommand("avoid");
    }else {
        // 当复选框取消选中时，发送"stop_avoid"命令（命令可自定义）
        sendCommand("avoid_cancel");
    }

}
