#ifndef STREAMLINK_H
#define STREAMLINK_H

#include <QObject>
#include <QUdpSocket>
#include <QThread>
#include <QVector>

#define  STREAM_LINK_PACKET_SIZE            500

class StreamLink : public QObject
{
    Q_OBJECT
public:
    explicit        StreamLink(QObject *parent = nullptr);
    void            setPort(quint16 aPort);
    void            setID(unsigned int aID);
    unsigned int    getID();
    void            enable();
    void            flush();

signals:
    void            sigNewSamplesBufferReceived(QVector<double> rawData, int packetCounter, int magic);

private slots:
    void            initStreamLinkThread();
    void            readPendingData();

private:
    /* Socket informations*/
    QUdpSocket      *udpSocket;
    quint16         port;
    unsigned int    id;

    /* Stream link received data in separate worker thread*/
    QThread         *udpThread;

    /* Data parsing informations*/
    int             offset;

};

#endif // STREAMLINK_H
