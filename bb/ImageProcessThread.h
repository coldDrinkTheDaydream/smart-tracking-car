#ifndef IMAGEPROCESSTHREAD_H
#define IMAGEPROCESSTHREAD_H
// ImageProcessThread.h
#include <QThread>
#include <QByteArray>
#include <QPixmap>
#include <QString>
#include<QDir>
#include<QDateTime>

class ImageProcessThread : public QThread {
    Q_OBJECT
public:
    explicit ImageProcessThread(QObject *parent = nullptr) : QThread(parent) {}
    ~ImageProcessThread() {
        // 线程退出时确保资源释放
        requestInterruption();
        wait();
    }

    // 供主线程传递接收到的数据
    void setReceivedData(const QByteArray &data) {
        m_receivedData = data;
    }

signals:
    // 处理完成后，发送QPixmap给主线程显示
    void processFinished(const QPixmap &pixmap, const QString &saveMsg);
    // 发送解码失败信号
    void decodeFailed();

protected:
    void run() override {  // 线程核心执行函数（子线程中运行）
        quint32 imageSize = 0;
        QByteArray receivedData = m_receivedData;
        QByteArray imageData;

        // 1. 解析数据（先读4字节长度，再读图像数据）
        if (receivedData.size() >= sizeof(quint32)) {
            QDataStream in(&receivedData, QIODevice::ReadOnly);
            in.setByteOrder(QDataStream::BigEndian);  // 匹配下位机格式
            in >> imageSize;
            receivedData.remove(0, sizeof(quint32));

            if (receivedData.size() >= imageSize) {
                imageData = receivedData.left(imageSize);
                receivedData.remove(0, imageSize);
            } else {
                emit decodeFailed();  // 数据不完整，解码失败
                return;
            }
        } else {
            emit decodeFailed();
            return;
        }

        // 2. 解码JPG图像
        QImage image;
        if (!image.loadFromData(imageData, "JPG")) {
            emit decodeFailed();
            return;
        }

        // 3. 图像缩放+裁剪（224x224）
        const int TARGET_WIDTH = 224;
        const int TARGET_HEIGHT = 224;
        QImage scaledImage = image.scaled(
            TARGET_WIDTH, TARGET_HEIGHT,
            Qt::KeepAspectRatioByExpanding,
            Qt::SmoothTransformation
        );
        if (scaledImage.width() > TARGET_WIDTH || scaledImage.height() > TARGET_HEIGHT) {
            int x = (scaledImage.width() - TARGET_WIDTH) / 2;
            int y = (scaledImage.height() - TARGET_HEIGHT) / 2;
            scaledImage = scaledImage.copy(x, y, TARGET_WIDTH, TARGET_HEIGHT);
        }
        QPixmap pixmap = QPixmap::fromImage(scaledImage);

        // 4. 保存图像（耗时操作，在子线程执行）
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
        QString savePath = "D:/Camera_Saved/Qt_Received_Images/";
        QDir dir;
        if (!dir.exists(savePath)) {
            dir.mkpath(savePath);
        }
        QString fullSavePath = savePath + "received_" + timestamp + ".jpg";
        QString saveMsg = "图片已保存到：" + fullSavePath;
        if (!pixmap.save(fullSavePath, "JPG", 95)) {
            saveMsg = "图片保存失败：" + fullSavePath;
        }

        // 5. 发送结果给主线程（UI更新必须在主线程）
        emit processFinished(pixmap, saveMsg);
    }

private:
    QByteArray m_receivedData;  // 存储主线程传递的数据
};
#endif // IMAGEPROCESSTHREAD_H
