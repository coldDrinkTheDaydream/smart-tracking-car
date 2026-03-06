#include "imagethread.h"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QPixmap>
ImageThread::ImageThread(QObject *parent) : QObject(parent)
{

}

 void ImageThread::image(int port,char type, qint32 dataSize, const QByteArray &actualData)
    {
      qDebug() << "端口" << port << "接收图像";
     imageSize = dataSize;
     imageData = actualData;
        // 处理图像数据
        QImage processedImage = processImageData(imageData);

        if (!processedImage.isNull()) {
            // 发送处理后的图像给UI线程
            emit imageProcessed(processedImage);

            // 保存图像
            saveImage(processedImage);
        } else {
            qDebug() << "图像处理失败，生成了空图像";
        }

        // 重置，准备接收下一帧
        imageSize = 0;
        imageData.clear();
    }

QImage ImageThread::processImageData(const QByteArray &data)
{
    // 将数据转换为QImage
    QImage image;
    bool success = image.loadFromData(data, "JPG");
    if (!success) {
        return QImage();
    }

    // 调整图像大小为224x224
    // 先按比例缩放，使图像足够大以覆盖目标尺寸
    QImage scaledImage = image.scaled(
        TARGET_WIDTH, TARGET_HEIGHT,
        Qt::KeepAspectRatioByExpanding,  // 保持比例并扩展到至少目标尺寸
        Qt::SmoothTransformation        // 平滑缩放，保证质量
    );

    // 从中心裁剪出224x224的区域
    if (scaledImage.width() > TARGET_WIDTH || scaledImage.height() > TARGET_HEIGHT) {
        int x = (scaledImage.width() - TARGET_WIDTH) / 2;
        int y = (scaledImage.height() - TARGET_HEIGHT) / 2;
        scaledImage = scaledImage.copy(x, y, TARGET_WIDTH, TARGET_HEIGHT);
    }

    return scaledImage;
}

void ImageThread::saveImage(const QImage &image)
{
    // 1. 生成唯一文件名（格式：时间戳.jpg，避免覆盖）
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");

    // 2. 设置保存路径
    QString saveDir = "D:/Camera_Saved/";
    QString savePath = saveDir + "/Qt_Received_Images/";

    // 3. 确保保存目录存在
    QDir dir;
    if (!dir.exists(savePath)) {
        if (!dir.mkpath(savePath)) {

            return;
        }
    }

    // 4. 完整保存路径
    QString fullSavePath = savePath + "received_" + timestamp + ".jpg";

    // 5. 保存图像
    bool saveSuccess = image.save(fullSavePath, "JPG", 95);  // 95为图像质量

    // 6. 发送保存结果状态
    if (saveSuccess) {

        qDebug() << "图片已保存到：" << fullSavePath;
    } else {

    }
}



