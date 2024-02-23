#ifndef DEVICECONTAINER_H
#define DEVICECONTAINER_H

#include <QObject>
#include "device.h"
#include "Windows/Device/devicewnd.h"
#include "Utility/log.h"

class DeviceContainer : public QObject
{
    Q_OBJECT
public:
    explicit DeviceContainer(QObject *parent = nullptr,  DeviceWnd* aDeviceWnd = nullptr, Device* aDevice = nullptr);
    ~DeviceContainer();

signals:
    void    sigDeviceClosed(Device* device);

public slots:
    void    onDeviceControlLinkDisconnected();
    void    onDeviceControlLinkConnected();
    void    onDeviceStatusLinkNewDeviceAdded(QString aDeviceIP);
    void    onDeviceStatusLinkNewMessageReceived(QString aDeviceIP, QString aMessage);
    void    onDeviceClosed();
    void    onConsoleWndMessageRcvd(QString msg);
    void    onConsoleWndHandleControlMsgResponse(QString msg);
    void    onResolutionChanged(int index);
    void    onClockDivChanged(int index);
    void    onSampleTimeChanged(int index);
    void    onSamplingTimeChanged(QString time);
    void    onInterfaceChanged(QString interfaceIp);
    void    onAvrRatioChanged(int index);
    void    onVOffsetChanged(QString off);
    void    onCOffsetChanged(QString off);

private:
    DeviceWnd*  deviceWnd;
    Device*     device;
    Log*        log;

};

#endif // DEVICECONTAINER_H
