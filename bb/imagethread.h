#ifndef IMAGETHREAD_H
#define IMAGETHREAD_H

#include <QObject>
#include <QString>


class ImageThread : public QObject
{
    Q_OBJECT
public:
    explicit ImageThread(QObject *parent = nullptr);

signals:
    void imageProcessed(const QImage &image);

public slots:
    void image(int port,char type, qint32 dataSize, const QByteArray &actualData);


private:

    QByteArray imageData;
    const int TARGET_WIDTH = 400;
    const int TARGET_HEIGHT = 250;
    quint32 imageSize = 0;
    QImage processImageData(const QByteArray &data);
    void saveImage(const QImage &image);
};

#endif // IMAGETHREAD_H
