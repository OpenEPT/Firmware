#include <QDataStream>
#include <QNetworkDatagram>
#include <QDebug>
#include "streamlink.h"
#include <winsock2.h>
#include <ws2tcpip.h>

StreamLink::StreamLink(QObject *parent)
    : QObject{parent}
{
    port = 0;
    id  = 0;
    udpThread = new QThread(this);
    this->moveToThread(udpThread);
    udpThread->setObjectName("UDP Thread");
    QObject::connect(udpThread, &QThread::started, this, &StreamLink::initStreamLinkThread, Qt::QueuedConnection);
}

void StreamLink::setPort(quint16 aPort)
{
    port = aPort;
}

void StreamLink::setID(unsigned int aID)
{
    id = aID;
}

unsigned int StreamLink::getID()
{
    return id;
}

void StreamLink::enable()
{
    udpThread->start();
}

void StreamLink::flush()
{
    udpSocket->flush();
}

void StreamLink::initStreamLinkThread()
{
    udpSocket = new QUdpSocket();
    udpSocket->bind(port);
    int rcvbufsize = 120000*1024*2;
    int socketDesc = udpSocket->socketDescriptor();
    int ret = 0;
    udpSocket->setReadBufferSize(rcvbufsize);
    ret = setsockopt(socketDesc,SOL_SOCKET,SO_RCVBUF,(char*)&rcvbufsize,sizeof(rcvbufsize));
    qDebug()<< ret ;
    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(readPendingData()));
}

void StreamLink::readPendingData()
{
    unsigned int counter;
    unsigned int magic;
    while(udpSocket->hasPendingDatagrams())
    {
        QByteArray receivedData;
        QVector<short> data;
        QVector<double> data_double;

        receivedData.resize(udpSocket->pendingDatagramSize());
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        receivedData = datagram.data();

        data.resize(STREAM_LINK_PACKET_SIZE);
        memcpy(data.data(), receivedData.data()+8, STREAM_LINK_PACKET_SIZE*2);
        memcpy(&counter, receivedData.data(), 4);
        memcpy(&magic, receivedData.data()+4, 4);
        data_double.reserve(data.size());
        std::copy(data.cbegin(), data.cend(), std::back_inserter(data_double));        
        emit sigNewSamplesBufferReceived(data_double, counter, magic);
    }
}
