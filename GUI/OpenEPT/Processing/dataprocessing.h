#ifndef DATAPROCESSING_H
#define DATAPROCESSING_H

#include <QObject>
#include <QThread>


#define DATAPROCESSING_DEFAULT_NUMBER_OF_BUFFERS        100
#define DATAPROCESSING_DEFAULT_SAMPLES_BUFFER_SIZE      500
#define DATAPROCESSING_DEFAULT_SAMPLE_SIZE              2
#define DATAPROCESSING_DEFAULT_ADC_VOLTAGE_REF          2*4.097
//#define DATAPROCESSING_DEFAULT_ADC_VOLTAGE_REF          3.28
#define DATAPROCESSING_DEFAULT_ADC_VOLTAGE_OFF          3.2
#define DATAPROCESSING_DEFAULT_SHUNT                    0.045
#define DATAPROCESSING_DEFAULT_GAIN                     10

typedef enum
{
    DATAPROCESSING_ACQUISITION_STATUS_ACTIVE,
    DATAPROCESSING_ACQUISITION_STATUS_INACTIVE
}dataprocessing_acquisition_status_t;

typedef enum
{
    DATAPROCESSING_CONSUMPTION_MODE_CURRENT,
    DATAPROCESSING_CONSUMPTION_MODE_CUMULATIVE,
}dataprocessing_consumption_mode_t;

typedef enum
{
    DATAPROCESSING_DEVICE_MODE_INT,
    DATAPROCESSING_DEVICE_MODE_EXT
}dataprocessing_device_mode_t;


Q_DECLARE_METATYPE(dataprocessing_consumption_mode_t);


class DataProcessing : public QObject
{
    Q_OBJECT
public:
    explicit                            DataProcessing(QObject *parent = nullptr);
    void                                setDeviceMode(dataprocessing_device_mode_t mode);
    bool                                setNumberOfBuffersToCollect(unsigned int numberOfBaffers);
    bool                                setSamplesBufferSize(unsigned int size);
    bool                                setSamplingPeriod(double aSamplingPeriod);                  //us
    bool                                setSamplingTime(double aSamplingTime);                      //us
    bool                                setResolution(double aResolution);
    bool                                setConsumptionMode(dataprocessing_consumption_mode_t aConsumptionMode);

    bool                                setAcquisitionStatus(dataprocessing_acquisition_status_t aAcquisitionStatus);

signals:
    void                                sigNewVoltageCurrentSamplesReceived(QVector<double> voltage, QVector<double> current, QVector<double> voltageKeys, QVector<double> currentKeys);
    void                                sigNewConsumptionDataReceived(QVector<double> consumption, QVector<double> keys, dataprocessing_consumption_mode_t consumptionMode);
    void                                sigSamplesBufferReceiveStatistics(double dropRate, unsigned int dopPacketsNo, unsigned int fullPacketCounter, unsigned int lastPacketID, unsigned short ebp);
    void                                sigEBP(QVector<double> ebpValues, QVector<double> ebpKeys);
    void                                sigEBPValue(unsigned int ebpID, double ebpValues, double ebpKeys);

public slots:
    void                                onNewSampleBufferReceived(QVector<double> rawData, int packetID,int magic);

private:
    void                                initBuffers();
    void                                initVoltageBuffer();
    void                                initCurrentBuffer();
    void                                initConsumptionBuffer();
    void                                initKeyBuffer();
    void                                initEBPBuffer();
    QVector<double>                     processSignalWithFFT(const QVector<double> &inputSignal, double threshold);
    void                                signalFFT(const QVector<double> &inputSignal, QVector<double>& amplitudeSpectrum);

    /* Stream link received data in separate worker thread*/
    QThread                             *dataProcessingThread;

    /* */
    unsigned int                        currentNumberOfBuffers;
    unsigned int                        lastBufferUsedPositionIndex;
    unsigned int                        receivedPacketCounter;      //During one session
    unsigned int                        lastReceivedPacketID;
    unsigned int                        dropPacketsNo;          //Number of dropped packets
    unsigned int                        ebpNo;                  //Number of energy breakPoint
    bool                                firstPacketReceived;    //

    /* */
    double                              samplingPeriod;         //ms
    double                              samplingTime;           //ms
    double                              voltageInc;
    double                              currentInc;

    /* */
    unsigned int                        maxNumberOfBuffers;
    unsigned int                        samplesBufferSize;

    /* */
    QVector<double>                     voltageDataCollected;
    QVector<double>                     currentDataCollected;
    QVector<double>                     currentConsumptionDataCollected;
    QVector<double>                     cumulativeConsumptionDataCollected;
    double                              lastCumulativeCurrentConsumptionValue;
    QVector<double>                     voltageKeysDataCollected;
    QVector<double>                     currentKeysDataCollected;
    QVector<double>                     consumptionKeysDataCollected;
    QVector<bool>                       ebpFlags;
    QVector<double>                     ebpValue;
    QVector<double>                     ebpValueKey;


    /**/
    dataprocessing_acquisition_status_t acquisitionStatus;
    dataprocessing_consumption_mode_t   consumptionMode;
    dataprocessing_device_mode_t        deviceMode;

};

#endif // DATAPROCESSING_H
