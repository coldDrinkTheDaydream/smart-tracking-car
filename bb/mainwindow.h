#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QLabel>
#include <QImage>
#include "receiverthread.h"
#include "imagethread.h"
#include "tahthread.h"
#include "disthread.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:


    void on_newImageConnection();

    void on_newOtherConnection();
    void on_newDisConnection();

    void onTemperatureUpdated(double temperature);

    void onSpeedUpdated(int speed);

    void onHumUpdated(double temperature);
    void onDisUpdated(float dis);



    void sendCommand(const QString& cmd);

    void onImageProcessed(const QImage &image);

    void on_forward_button_clicked();

    void on_left_button_pressed();

    void on_left_button_released();

    void on_stop_button_clicked();

    void on_right_button_pressed();

    void on_right_button_released();

    void on_back_button_pressed();

    void on_back_button_released();

    void on_speed_sb_valueChanged(int arg1);

    void on_ac_button_pressed();

    void on_ac_button_released();


    void on_sound_button_clicked();

    void on_dc_button_pressed();

    void on_dc_button_released();

    void on_hand_rbutton_toggled(bool checked);

    void on_auto_rbutton_toggled(bool checked);

    void on_avoid_checkBox_toggled(bool checked);

private:
    Ui::MainWindow *ui;
    QTcpServer* imageServer;  // 图像服务器（新端口）
    QTcpServer* otherServer; // 控制服务器（原端口）
    QTcpServer* disServer;
    QTcpSocket* imageSocket = nullptr;
    QTcpSocket* otherSocket = nullptr;
    QTcpSocket* disSocket = nullptr;
    // 处理线程
    QThread *imageRThread;       // 图像处理线程
    QThread *otherRThread;       // 其他数据处理线程
    QThread *disRThread;         // 距离处理线程

    ReceiverThread *imageReceiver;
    ReceiverThread *otherReceiver;
    ReceiverThread *disReceiver;
    ImageThread *imageThread; //图像处理子线程
    TahThread *tahThread;
    DisThread *disThread;
    QThread *workerThread;
};
#endif // MAINWINDOW_H
