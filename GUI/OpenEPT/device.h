#ifndef DEVICE_H
#define DEVICE_H

#include <QObject>
#include <QString>
#include "Links/controllink.h"
#include "Links/statuslink.h"
#include "Links/streamlink.h"
#include "Processing/dataprocessing.h"
#include "Links/eplink.h"
#include "Processing/epprocessing.h"

/* Resolution sample time offset based on STM32H755ZI offset */
#define     DEVICE_ADC_RESOLUTION_16BIT_STIME_OFFSET    8.5
#define     DEVICE_ADC_RESOLUTION_14BIT_STIME_OFFSET    7.5
#define     DEVICE_ADC_RESOLUTION_12BIT_STIME_OFFSET    6.5
#define     DEVICE_ADC_RESOLUTION_10BIT_STIME_OFFSET    5.5
#define     DEVICE_ADC_TIMER_INPUT_CLK                  200000000 //Hz
#define     DEVICE_ADC_DEFAULT_SAMPLING_PERIOD          0.000001 //s


typedef enum{
    DEVICE_ADC_UNKNOWN                 = 0,
    DEVICE_ADC_INTERNAL                = 1,
    DEVICE_ADC_EXTERNAL                = 2
}device_adc_t;

typedef enum{
    DEVICE_ADC_RESOLUTION_UKNOWN       = 1,
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
    DEVICE_ADC_CLOCK_DIV_128           = 128,
    DEVICE_ADC_CLOCK_DIV_256           = 256
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
    void        epLinkServerCreate();
    bool        establishEPLink(QString ip);
    void        sendControlMsg(QString msg);
    bool        setADC(device_adc_t aAdc);
    bool        setResolution(device_adc_resolution_t resolution);
    bool        getResolution(device_adc_resolution_t* resolution = NULL);
    bool        setClockDiv(device_adc_clock_div_t clockDiv);
    bool        getClockDiv(device_adc_clock_div_t* clockDiv = NULL);
    bool        setChSampleTime(device_adc_ch_sampling_time_t sampleTime);
    bool        getChSampleTime(device_adc_ch_sampling_time_t* sampleTime=NULL);
    bool        setAvrRatio(device_adc_averaging_t averagingRatio);
    bool        getAvrRatio(device_adc_averaging_t* averagingRatio=NULL);
    bool        setSamplingPeriod(QString time);
    bool        getSamplingPeriod(QString* time = NULL);
    bool        setVOffset(QString off);
    bool        getVOffset(QString* off=NULL);
    bool        setCOffset(QString off);
    bool        getCOffset(QString* off=NULL);
    bool        getADCInputClk(QString* clk = NULL);
    double      obtainSamplingTime();    //This function determine time interval from start of until the acquisition end. Dont mix it with acquisiton (sampling) period
    bool        acquireDeviceConfiguration(device_adc_t aAdc = DEVICE_ADC_INTERNAL);

    bool        setDataProcessingMaxNumberOfBuffers(unsigned int maxNumber);
    bool        setDataProcessingConsumptionType(dataprocessing_consumption_mode_t aConsumptionMode);
    bool        setDataProcessingMeasurementType(dataprocessing_measurement_mode_t aMeasurementMode);

signals:
    void        sigControlLinkConnected();
    void        sigControlLinkDisconnected();
    void        sigStatusLinkNewDeviceAdded(QString aDeviceIP);
    void        sigStatusLinkNewMessageReceived(QString aDeviceIP, QString aMessage);
    void        sigNewResponseReceived(QString response, bool executionStatus);

    void        sigResolutionObtained(QString resolution);
    void        sigChSampleTimeObtained(QString chstime);
    void        sigSampleTimeObtained(QString stime);
    void        sigClockDivObtained(QString clkDiv);
    void        sigAdcInputClkObtained(QString inClk);
    void        sigCOffsetObtained(QString coffset);
    void        sigVOffsetObtained(QString voffset);
    void        sigAvgRatio(QString voffset);
    void        sigSamplingTimeChanged(double value);
    void        sigVoltageCurrentSamplesReceived(QVector<double> voltage, QVector<double> current, QVector<double> voltageKeys, QVector<double> currentKeys);
    void        sigNewConsumptionDataReceived(QVector<double> consumption, QVector<double> keys, dataprocessing_consumption_mode_t mode);
    void        sigNewStatisticsReceived(dataprocessing_dev_info_t voltageStat, dataprocessing_dev_info_t currentStat, dataprocessing_dev_info_t consumptioStat);
    void        sigNewSamplesBuffersProcessingStatistics(double dropRate,  unsigned int dropPacketsNo, unsigned int fullReceivedBuffersNo, unsigned int lastBufferID, unsigned short ebp);
    void        sigNewEBP(QVector<double> ebpValues, QVector<double> ebpKeys);
    void        sigNewEBPFull(double value, double key, QString name);
    void        sigAcqusitionStarted();
    void        sigAcqusitionStopped();
public slots:
    void        onControlLinkConnected();
    void        onControlLinkDisconnected();

private slots:
    void        onStatusLinkNewDeviceAdded(QString aDeviceIP);
    void        onStatusLinkNewMessageReceived(QString aDeviceIP, QString aMessage);
    void        onEPLinkNewDeviceAdded(QString aDeviceIP);
    void        onEPLinkNewMessageReceived(QString aDeviceIP, QString aMessage);
    void        onNewVoltageCurrentSamplesReceived(QVector<double> voltage, QVector<double> current, QVector<double> voltageKeys, QVector<double> currentKeys);
    void        onNewSamplesBuffersProcessingStatistics(double dropRate,  unsigned int dropPacketsNo, unsigned int fullReceivedBuffersNo, unsigned int lastBufferID, unsigned short ebp);
    void        onNewConsumptionDataReceived(QVector<double> consumption, QVector<double> keys, dataprocessing_consumption_mode_t mode);
    void        onNewStatisticsReceived(dataprocessing_dev_info_t voltageStat, dataprocessing_dev_info_t currentStat, dataprocessing_dev_info_t consumptioStat);
    void        onNewEBP(QVector<double> ebpValues, QVector<double> ebpKeys);
    void        onNewEBPFull(double value, double key, QString name);
private:
    QString                         deviceName;
    double                          samplingPeriod;                //ms
    device_adc_resolution_t         adcResolution;
    device_adc_ch_sampling_time_t   adcChSamplingTime;
    device_adc_clock_div_t          adcClockingDiv;
    device_adc_averaging_t          adcAveraging;
    device_adc_t                    adc;
    double                          adcInputClkValue;

    double                          adcResolutionSampleTimeOffset;
    double                          adcSampleTimeOffset;
    double                          adcSampleTime;                  //s

    ControlLink*                    controlLink;
    StatusLink*                     statusLink;
    StreamLink*                     streamLink;
    EPLink*                         energyPointLink;
    DataProcessing*                 dataProcessing;
    EPProcessing*                   energyPointProcessing;
    QString                         voltageOffset;
    QString                         currentOffset;
    QString                         adcInputClk;
    /*This should be removed when stream link is defined*/
    int                             streamID;

};

#endif // DEVICE_H
