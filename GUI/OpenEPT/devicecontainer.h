#ifndef DEVICECONTAINER_H
#define DEVICECONTAINER_H

#include <QObject>
#include <QTimer>
#include "device.h"
#include "Windows/Device/devicewnd.h"
#include "Utility/log.h"
#include "Processing/fileprocessing.h"

class DeviceContainer : public QObject
{
    Q_OBJECT
public:
    explicit DeviceContainer(QObject *parent = nullptr,  DeviceWnd* aDeviceWnd = nullptr, Device* aDevice = nullptr);
    ~DeviceContainer();

signals:
    void    sigDeviceClosed(Device* device);

public slots:
    void    onConsoleWndMessageRcvd(QString msg);
    void    onDeviceWndResolutionChanged(QString resolution);
    void    onDeviceWndADCChanged(QString adc);
    void    onDeviceWndClockDivChanged(QString clockDiv);
    void    onDeviceWndChannelSamplingTimeChanged(QString stime);
    void    onDeviceWndSamplingPeriodChanged(QString time);
    void    onDeviceWndInterfaceChanged(QString interfaceIp);
    void    onDeviceWndAvrRatioChanged(QString avgRatio);
    void    onDeviceWndVOffsetChanged(QString off);
    void    onDeviceWndCOffsetChanged(QString off);
    void    onDeviceWndAcquisitionStart();
    void    onDeviceWndAcquisitionStop();
    void    onDeviceWndAcquisitionPause();
    void    onDeviceWndAdvConfGet();
    void    onDeviceWndAcquisitionRefresh();
    void    onDeviceWndNewConfiguration(QVariant newConfig);
    void    onDeviceWndClosed();
    void    onDeviceWndMaxNumberOfBuffersChanged(unsigned int maxNumber);
    void    onDeviceWndConsumptionTypeChanged(QString aConsumptionType);
    void    onDeviceWndMeasurementTypeChanged(QString aMeasurementType);
    void    onDeviceWndSamplesSavePathChanged(QString path);

    void    onDeviceControlLinkDisconnected();
    void    onDeviceControlLinkConnected();
    void    onDeviceStatusLinkNewDeviceAdded(QString aDeviceIP);
    void    onDeviceStatusLinkNewMessageReceived(QString aDeviceIP, QString aMessage);
    void    onDeviceHandleControlMsgResponse(QString msg, bool exeStatus);
    void    onDeviceResolutionObtained(QString resolution);
    void    onDeviceClkDivObtained(QString clkDiv);
    void    onDeviceChSampleTimeObtained(QString stime);
    void    onDeviceSamplingPeriodObtained(QString stime);
    void    onDeviceAdcInClkObtained(QString inClk);
    void    onDeviceAvgRatioChanged(QString aAvgRatio);
    void    onDeviceCOffsetObtained(QString coffset);
    void    onDeviceVOffsetObtained(QString voffset);
    void    onDeviceSamplingTimeChanged(double value);

    void    onDeviceAcquisitonStarted();
    void    onDeviceAcquisitonStopped();

    void    onTimeout();

    void    onDeviceNewVoltageCurrentSamplesReceived(QVector<double> voltage, QVector<double> current, QVector<double> voltageKeys, QVector<double> currentKeys);
    void    onDeviceNewConsumptionDataReceived(QVector<double> consumption, QVector<double> keys, dataprocessing_consumption_mode_t mode);
    void    onDeviceNewStatisticsReceived(dataprocessing_dev_info_t voltageStat, dataprocessing_dev_info_t currentStat, dataprocessing_dev_info_t consumptionStat);
    void    onDeviceNewSamplesBuffersProcessingStatistics(double dropRate, unsigned int dropPacketsNo, unsigned int fullReceivedBuffersNo, unsigned int lastBufferID, unsigned short ebp);
    void    onDeviceNewEBP(QVector<double> ebpValues, QVector<double> keys);
    void    onDeviceNewEBPFull(double value, double key, QString name);


private:
    DeviceWnd*                      deviceWnd;
    Device*                         device;
    Log*                            log;
    FileProcessing*                 fileProcessing;


    device_adc_resolution_t         getAdcResolutionFromString(QString resolution);
    device_adc_clock_div_t          getAdcClockDivFromString(QString clkDiv);
    device_adc_ch_sampling_time_t   getAdcChSamplingTimeFromString(QString chstime);
    device_adc_averaging_t          getAdcAvgRatioFromString(QString avgRatio);
    device_adc_t                    getAdcFromString(QString adc);

    double                          elapsedTime;
    QTimer                          *timer;

};

#endif // DEVICECONTAINER_H
