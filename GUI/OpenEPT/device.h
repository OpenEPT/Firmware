#ifndef DEVICE_H
#define DEVICE_H

#include <QObject>
#include <QString>
#include "Links/controllink.h"
#include "Links/statuslink.h"

typedef enum{
    DEVICE_ADC_RESOLUTION_UKNOWN       = 0,
    DEVICE_ADC_RESOLUTION_16BIT        = 16,
    DEVICE_ADC_RESOLUTION_14BIT        = 14,
    DEVICE_ADC_RESOLUTION_12BIT        = 12,
    DEVICE_ADC_RESOLUTION_10BIT        = 10
}device_adc_resolution_t;

typedef enum{
    DEVICE_ADC_SAMPLING_TIME_UKNOWN   = 0,
    DEVICE_ADC_SAMPLING_TIME_1C5      = 1,
    DEVICE_ADC_SAMPLING_TIME_2C5      = 2,
    DEVICE_ADC_SAMPLING_TIME_8C5      = 8,
    DEVICE_ADC_SAMPLING_TIME_16C5     = 16,
    DEVICE_ADC_SAMPLING_TIME_32C5     = 32,
    DEVICE_ADC_SAMPLING_TIME_64C5     = 64,
    DEVICE_ADC_SAMPLING_TIME_387C5    = 387,
    DEVICE_ADC_SAMPLING_TIME_810C5    = 810
}device_adc_ch_sampling_time_t;

typedef enum{
    DEVICE_ADC_CLOCK_DIV_UKNOWN        = 0,
    DEVICE_ADC_CLOCK_DIV_1             = 1,
    DEVICE_ADC_CLOCK_DIV_2             = 2,
    DEVICE_ADC_CLOCK_DIV_4             = 4,
    DEVICE_ADC_CLOCK_DIV_6             = 6,
    DEVICE_ADC_CLOCK_DIV_8             = 8,
    DEVICE_ADC_CLOCK_DIV_10            = 10,
    DEVICE_ADC_CLOCK_DIV_12            = 12,
    DEVICE_ADC_CLOCK_DIV_16            = 16,
    DEVICE_ADC_CLOCK_DIV_32            = 32,
    DEVICE_ADC_CLOCK_DIV_64            = 64,
    DEVICE_ADC_CLOCK_DIV_128           = 128
}device_adc_clock_div_t;

typedef enum{
    DEVICE_ADC_AVERAGING_UKNOWN        = 0,
    DEVICE_ADC_AVERAGING_DISABLED      = 1,
    DEVICE_ADC_AVERAGING_2             = 2,
    DEVICE_ADC_AVERAGING_4             = 4,
    DEVICE_ADC_AVERAGING_8             = 8,
    DEVICE_ADC_AVERAGING_16            = 16,
    DEVICE_ADC_AVERAGING_32            = 32,
    DEVICE_ADC_AVERAGING_64            = 64,
    DEVICE_ADC_AVERAGING_128           = 128,
    DEVICE_ADC_AVERAGING_256           = 256,
    DEVICE_ADC_AVERAGING_512           = 512,
    DEVICE_ADC_AVERAGING_1024          = 1024
}device_adc_averaging_t;

class Device : public QObject
{
    Q_OBJECT
public:
    explicit Device(QObject *parent = nullptr);
    ~Device();

    bool        acquisitionStart();
    bool        acquisitionStop();
    bool        acquisitionPause();
    bool        setName(QString aNewDeviceName);
    bool        getName(QString* aDeviceName);
    void        controlLinkAssign(ControlLink* link);
    bool        createStreamLink(QString ip, quint16 port, int* id);
    void        statusLinkCreate();
    void        controlLinkReconnect();
    void        sendControlMsg(QString msg);
    bool        setResolution(device_adc_resolution_t resolution);
    bool        getResolution(device_adc_resolution_t* resolution = NULL);
    bool        setClockDiv(device_adc_clock_div_t clockDiv);
    bool        getClockDiv(device_adc_clock_div_t* clockDiv = NULL);
    bool        setChSampleTime(device_adc_ch_sampling_time_t sampleTime);
    bool        getChSampleTime(device_adc_ch_sampling_time_t* sampleTime=NULL);
    bool        setAvrRatio(device_adc_averaging_t averagingRatio);
    bool        getAvrRatio(device_adc_averaging_t* averagingRatio=NULL);
    bool        setSamplingTime(QString time);
    bool        getSamplingTime(QString* time = NULL);
    bool        setVOffset(QString off);
    bool        getVOffset(QString* off=NULL);
    bool        setCOffset(QString off);
    bool        getCOffset(QString* off=NULL);
    bool        getADCInputClk(QString* clk = NULL);
    bool        acquireDeviceConfiguration();

signals:
    void        sigControlLinkConnected();
    void        sigControlLinkDisconnected();
    void        sigStatusLinkNewDeviceAdded(QString aDeviceIP);
    void        sigStatusLinkNewMessageReceived(QString aDeviceIP, QString aMessage);
    void        sigNewResponseReceived(QString response);

    void        sigResolutionObtained(QString resolution);
    void        sigChSampleTimeObtained(QString chstime);
    void        sigSampleTimeObtained(QString stime);
    void        sigClockDivObtained(QString clkDiv);
    void        sigAdcInputClkObtained(QString inClk);
    void        sigCOffsetObtained(QString coffset);
    void        sigVOffsetObtained(QString voffset);
    void        sigAvgRatio(QString voffset);
public slots:
    void        onControlLinkConnected();
    void        onControlLinkDisconnected();

private slots:
    void        onStatusLinkNewDeviceAdded(QString aDeviceIP);
    void        onStatusLinkNewMessageReceived(QString aDeviceIP, QString aMessage);

private:
    QString                         deviceName;
    QString                         samplingTime;
    device_adc_resolution_t         adcResolution;
    device_adc_ch_sampling_time_t   adcChSamplingTime;
    device_adc_clock_div_t          adcClockingDiv;
    device_adc_averaging_t          adcAveraging;

    ControlLink*                    controlLink;
    StatusLink*                     statusLink;
    QString                         voltageOffset;
    QString                         currentOffset;
    QString                         adcInputClk;
    /*This should be removed when stream link is defined*/
    int                             streamID;

};

#endif // DEVICE_H
