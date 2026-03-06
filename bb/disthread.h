#ifndef DISTHREAD_H
#define DISTHREAD_H

#include <QObject>

class DisThread : public QObject
{
    Q_OBJECT
public:
    explicit DisThread(QObject *parent = nullptr);

signals:
     void disUpdated(float dis);

public slots:
     void dis(qint32 dataSize, const QByteArray &actualData);

private:


};
#endif // DISTHREAD_H
