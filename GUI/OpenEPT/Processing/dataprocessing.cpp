#include "qmath.h"
#include "dataprocessing.h"

DataProcessing::DataProcessing(QObject *parent)
    : QObject{parent}
{
    dataProcessingThread = new QThread(this);
    this->moveToThread(dataProcessingThread);
    dataProcessingThread->setObjectName("Data processing thread");
    dataProcessingThread->start();
    currentNumberOfBuffers          = 0;
    lastBufferUsedPositionIndex     = 0;
    maxNumberOfBuffers              = DATAPROCESSING_DEFAULT_NUMBER_OF_BUFFERS;
    samplesBufferSize               = DATAPROCESSING_DEFAULT_SAMPLES_BUFFER_SIZE/2;

    setAcquisitionStatus(DATAPROCESSING_ACQUISITION_STATUS_INACTIVE);
    setConsumptionMode(DATAPROCESSING_CONSUMPTION_MODE_CURRENT);
}

bool DataProcessing::setNumberOfBuffersToCollect(unsigned int numberOfBaffers)
{
    if(acquisitionStatus == DATAPROCESSING_ACQUISITION_STATUS_ACTIVE) return false;

    maxNumberOfBuffers = numberOfBaffers;

    initBuffers();

    return true;
}

bool DataProcessing::setSamplesBufferSize(unsigned int size)
{
    if(acquisitionStatus == DATAPROCESSING_ACQUISITION_STATUS_ACTIVE) return false;

    initBuffers();

    return true;
}

bool DataProcessing::setSamplingPeriod(double aSamplingPeriod)
{
    if(acquisitionStatus == DATAPROCESSING_ACQUISITION_STATUS_ACTIVE) return false;
    samplingPeriod = aSamplingPeriod/1000.0; //convert us to ms
    return true;
}

bool DataProcessing::setSamplingTime(double aSamplingTime)
{
    if(acquisitionStatus == DATAPROCESSING_ACQUISITION_STATUS_ACTIVE) return false;
    samplingTime = aSamplingTime/1000.0; //convert us to ms
    return true;
}

bool DataProcessing::setResolution(double aResolution)
{
    if(acquisitionStatus == DATAPROCESSING_ACQUISITION_STATUS_ACTIVE) return false;
    voltageInc = (double)DATAPROCESSING_DEFAULT_ADC_VOLTAGE_REF/qPow(2,aResolution);
    currentInc = (double)DATAPROCESSING_DEFAULT_ADC_VOLTAGE_REF/qPow(2,aResolution);
    return true;
}

bool DataProcessing::setConsumptionMode(dataprocessing_consumption_mode_t aConsumptionMode)
{
    if(acquisitionStatus == DATAPROCESSING_ACQUISITION_STATUS_ACTIVE) return false;
    consumptionMode = aConsumptionMode;
    return true;

}

bool DataProcessing::setAcquisitionStatus(dataprocessing_acquisition_status_t aAcquisitionStatus)
{
    acquisitionStatus = aAcquisitionStatus;

    switch(acquisitionStatus)
    {
    case DATAPROCESSING_ACQUISITION_STATUS_ACTIVE:
        break;
    case DATAPROCESSING_ACQUISITION_STATUS_INACTIVE:
        break;
    default:
        break;
    }

    lastReceivedPacketID        = 0;
    dropPacketsNo               = 0;
    firstPacketReceived         = false;
    receivedPacketCounter       = 0;
    lastCumulativeCurrentConsumptionValue = 0;
    initBuffers();

    return true;
}

void DataProcessing::onNewSampleBufferReceived(QVector<double> rawData, int packetID, int magic)
{
    double          keyStartValue = packetID*rawData.size()/2*samplingPeriod;
    double          dropRate = 0;
    unsigned int    i = 0;
    unsigned int    j = 0;

    /* Calculate packet statistics */
    if(firstPacketReceived)
    {
        if((lastReceivedPacketID + 1) != packetID)
        {
            dropPacketsNo += packetID - lastReceivedPacketID - 1;
            lastReceivedPacketID = packetID;
        }
        else
        {
            lastReceivedPacketID = packetID;
        }
    }
    else
    {
        lastReceivedPacketID = packetID;
        firstPacketReceived = true;
    }
    receivedPacketCounter += 1;
    dropRate = (double)dropPacketsNo / (double)(receivedPacketCounter + dropPacketsNo) * 100;

    /* Take data */
    for(; i < rawData.size();)
    {
        voltageDataCollected[lastBufferUsedPositionIndex] = rawData[i]*voltageInc;
        if(i == 0)
        {
            voltageKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue;
            currentKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + samplingTime;
            consumptionKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + samplingTime;
        }
        else
        {
            voltageKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + (double)j*samplingPeriod;
            currentKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + (double)j*samplingPeriod + samplingTime;
            consumptionKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + (double)j*samplingPeriod + samplingTime;
        }
        currentDataCollected[lastBufferUsedPositionIndex] = rawData[i+1]*currentInc;
        currentConsumptionDataCollected[lastBufferUsedPositionIndex] = rawData[i+1]*(samplingPeriod)/3600000; //mAh
        lastCumulativeCurrentConsumptionValue += rawData[i+1]*(samplingPeriod)/3600000;                         //This value remember last consumption in case when buffers are restarted
        cumulativeConsumptionDataCollected[lastBufferUsedPositionIndex] = lastCumulativeCurrentConsumptionValue;
        i                           += 2;
        lastBufferUsedPositionIndex += 1;
        j +=1;
    }

    currentNumberOfBuffers += 1;
    if(currentNumberOfBuffers == maxNumberOfBuffers)
    {
        emit sigNewVoltageCurrentSamplesReceived(voltageDataCollected, currentDataCollected, voltageKeysDataCollected, currentKeysDataCollected);
        emit sigSamplesBufferReceiveStatistics(dropRate, receivedPacketCounter, lastReceivedPacketID);
        switch(consumptionMode)
        {
        case DATAPROCESSING_CONSUMPTION_MODE_CURRENT:
            emit sigNewConsumptionDataReceived(currentConsumptionDataCollected, consumptionKeysDataCollected, DATAPROCESSING_CONSUMPTION_MODE_CURRENT);
            break;
        case DATAPROCESSING_CONSUMPTION_MODE_CUMULATIVE:
            emit sigNewConsumptionDataReceived(cumulativeConsumptionDataCollected, consumptionKeysDataCollected, DATAPROCESSING_CONSUMPTION_MODE_CUMULATIVE);
            break;
        }
        initBuffers();
    }
}

void DataProcessing::initBuffers()
{
    initVoltageBuffer();
    initCurrentBuffer();
    initConsumptionBuffer();
    initKeyBuffer();
}

void DataProcessing::initVoltageBuffer()
{
    voltageDataCollected.resize(maxNumberOfBuffers*samplesBufferSize);
    voltageDataCollected.fill(0);
    currentNumberOfBuffers = 0;
    lastBufferUsedPositionIndex = 0;
}

void DataProcessing::initCurrentBuffer()
{
    currentDataCollected.resize(maxNumberOfBuffers*samplesBufferSize);
    currentDataCollected.fill(0);
    currentNumberOfBuffers = 0;
    lastBufferUsedPositionIndex = 0;
}

void DataProcessing::initConsumptionBuffer()
{
    cumulativeConsumptionDataCollected.resize(maxNumberOfBuffers*samplesBufferSize);
    currentConsumptionDataCollected.resize(maxNumberOfBuffers*samplesBufferSize);
    cumulativeConsumptionDataCollected.fill(0);
    currentConsumptionDataCollected.fill(0);
    lastBufferUsedPositionIndex = 0;
}

void DataProcessing::initKeyBuffer()
{
    voltageKeysDataCollected.resize(maxNumberOfBuffers*samplesBufferSize);
    currentKeysDataCollected.resize(maxNumberOfBuffers*samplesBufferSize);
    consumptionKeysDataCollected.resize(maxNumberOfBuffers*samplesBufferSize);
    voltageKeysDataCollected.fill(0);
    currentKeysDataCollected.fill(0);
    consumptionKeysDataCollected.fill(0);
    currentNumberOfBuffers = 0;
    lastBufferUsedPositionIndex = 0;
}
