#ifndef TAHTHREAD_H
#define TAHTHREAD_H
#include <QObject>
#include <QString>

class TahThread : public QObject
{
    Q_OBJECT
public:
    explicit TahThread(QObject *parent = nullptr);

signals:
     void temperatureUpdated(double temperature);
     void speedUpdated(int speed);
     void humUpdated(double hum);

public slots:
     void temp(qint32 dataSize, const QByteArray &actualData);
     void speed(qint32 dataSize, const QByteArray &actualData);
     void hum(qint32 dataSize, const QByteArray &actualData);


private:


};

#endif // TAHTHREAD_H
