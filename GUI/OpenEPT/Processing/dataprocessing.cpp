#include <QDebug>
#include <QtCore>
#include "fftw/fftw3.h"
#include <cmath>
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
    deviceMode                      = DATAPROCESSING_DEVICE_MODE_INT;
    maxNumberOfBuffers              = DATAPROCESSING_DEFAULT_NUMBER_OF_BUFFERS;
    samplesBufferSize               = DATAPROCESSING_DEFAULT_SAMPLES_BUFFER_SIZE/2;

    voltageStat.average             = 0;
    currentStat.average             = 0;
    voltageStat.max                 = -10;
    currentStat.max                 = -10;
    voltageStat.min                 = 10;
    currentStat.min                 = 10;
    maxVoltageF                      = -10;
    maxCurrentF                      = -10;
    minVoltageF                      = 10;
    minCurrentF                      = 10;
    minMax.resize(2);

    setAcquisitionStatus(DATAPROCESSING_ACQUISITION_STATUS_INACTIVE);
    setConsumptionMode(DATAPROCESSING_CONSUMPTION_MODE_CURRENT);
    setMeasurementMode(DATAPROCESSING_MEASUREMENT_MODE_VOLTAGE);
}

void DataProcessing::setDeviceMode(dataprocessing_device_mode_t mode)
{
    deviceMode = mode;
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

// Function to apply FFT, zero amplitudes below a threshold, and inverse FFT
void DataProcessing::processSignalWithFFT(const QVector<double> &inputSignal, double threshold,QVector<double> &outputSignal, QVector<double>& amplitudeSpectrum, double sampling_time_ms, QVector<double>& frequencies, QVector<double> &minmax)
{
    int N = inputSignal.size();
    minmax[0] = 0;
    minmax[1] = 10;

    // Allocate arrays for FFT and IFFT
    fftw_complex* fftOutput = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
    fftw_complex* filteredFFT = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
    fftw_complex* outFFT = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);

    // Step 1: Compute FFT and get amplitude spectrum
    signalFFT(inputSignal, fftOutput, amplitudeSpectrum, sampling_time_ms, frequencies);

    // Step 2: Apply threshold filtering in the frequency domain
    for (int i = 0; i < N / 2; ++i) {
//        double amplitude = amplitudeSpectrum[i];
//        if (amplitude < threshold) {
//            // Set components below threshold to zero
//            filteredFFT[i][0] = 0.0;
//            filteredFFT[i][1] = 0.0;
//        } else {
            // Retain components above threshold
            filteredFFT[i][0] = fftOutput[i][0];
            filteredFFT[i][1] = fftOutput[i][1];
//        }
    }

    // Step 3: Perform inverse FFT
    outputSignal.resize(N);
    fftw_plan ifftPlan = fftw_plan_dft_1d(N, filteredFFT, outFFT, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(ifftPlan);

    // Normalize the output and store in outputSignal
    for (int i = 0; i < N; ++i) {
        outputSignal[i] = outFFT[i][0] / N; // Only real part is needed
        if(outputSignal[i] > minmax[0]) minmax[0] = outputSignal[i];
        if(outputSignal[i] < minmax[1]) minmax[1] = outputSignal[i];
    }

    amplitudeSpectrum[0] = 0.0;
    // Clean up
    fftw_destroy_plan(ifftPlan);
    fftw_free(fftOutput);
    fftw_free(filteredFFT);
}

void DataProcessing::signalFFT(const QVector<double>& inputSignal, fftw_complex* fftOutput, QVector<double>& amplitudeSpectrum, double sampling_time_ms, QVector<double>& frequencies)
{
    int N = inputSignal.size();
    amplitudeSpectrum.resize(N / 2);
    frequencies.resize(N / 2);

    // Allocate input array for FFTW
    fftw_complex* in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);

    // Populate the input array with the real signal (imaginary part = 0)
    for (int i = 0; i < N; ++i) {
        in[i][0] = inputSignal[i]; // real part
        in[i][1] = 0.0;            // imaginary part
    }

    // Plan and execute FFT
    fftw_plan fftPlan = fftw_plan_dft_1d(N, in, fftOutput, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(fftPlan);

    // Set the DC component to zero and calculate normalized amplitude spectrum
    for (int i = 0; i < N / 2; ++i) {
        double real = fftOutput[i][0];
        double imag = fftOutput[i][1];
        amplitudeSpectrum[i] = (2.0 / N) * std::sqrt(real * real + imag * imag); // Normalize amplitude
        frequencies[i] = (i / (N * sampling_time_ms)) * 1000; // Convert ms to seconds
    }

    // Clean up
    fftw_destroy_plan(fftPlan);
    fftw_free(in);
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
    samplingTime = aSamplingTime;
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

bool DataProcessing::setMeasurementMode(dataprocessing_measurement_mode_t aMeasurementMode)
{
    if(acquisitionStatus == DATAPROCESSING_ACQUISITION_STATUS_ACTIVE) return false;
    measurementMode = aMeasurementMode;
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
    ebpNo                       = 0;

    voltageStat.average             = 0;
    currentStat.average             = 0;
    voltageStat.max                 = -10;
    currentStat.max                 = -10;
    voltageStat.min                 = 10;
    currentStat.min                 = 10;


    lastCumulativeCurrentConsumptionValue = 0;
    initBuffers();

    return true;
}

void DataProcessing::onNewSampleBufferReceived(QVector<double> rawData, int packetID, int magic)
{
    double          keyStartValue = 0; ;
    double          dropRate = 0;
    unsigned int    i = 0;
    unsigned int    j = 0;
    unsigned short  ebp = (unsigned short)(magic >> 16);
    unsigned int size = rawData.size();
    unsigned int halfSize = rawData.size()/2;

    if(ebp != 0)
    {
        qDebug() << "EBP detected: " + ebpNo;
        ebpNo += 1;
    }

    /*set ebp flags*/
    ebpFlags[currentNumberOfBuffers] = ebp;

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
    if(deviceMode == DATAPROCESSING_DEVICE_MODE_INT){
        keyStartValue = packetID*rawData.size()/2*samplingPeriod;
        for(; i < rawData.size();)
        {
            voltageDataCollected[lastBufferUsedPositionIndex] = rawData[i]*voltageInc;
            if(i == 0)
            {
                voltageKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue;
                currentKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + samplingTime;
                consumptionKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + samplingTime;
                if(ebp != 0 && ebpNo > 0)
                {
                    ebpValue.append(lastCumulativeCurrentConsumptionValue);
                    ebpValueKey.append(keyStartValue);
                    emit sigEBPValue(packetID, lastCumulativeCurrentConsumptionValue, keyStartValue);
                }
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
            j += 1;
        }
    }
    else
    {
        keyStartValue = packetID*rawData.size()/2*samplingPeriod;
        for(; i < halfSize;)
        {
            double rawVoltage = rawData[i];
            double rawCurrent= rawData[i+halfSize];
            short a = (((short) rawVoltage) >> 8) & 0x00FF;
            short b = (((short) rawVoltage) << 8) & 0xFF00;
            short c = a | b;
            int d = (int) c;
            double swapDataVoltage = (double)d;
            a = (((short) rawCurrent) >> 8) & 0x00FF;
            b = (((short) rawCurrent) << 8) & 0xFF00;
            c = a | b;
            d = (int) c;
            double swapDataCurrent = (double)d;
            double voltageValue = DATAPROCESSING_DEFAULT_ADC_VOLTAGE_OFF + swapDataVoltage*voltageInc;
            double currentValue = swapDataCurrent*currentInc/(DATAPROCESSING_DEFAULT_SHUNT*DATAPROCESSING_DEFAULT_GAIN)*1000.0; //mA
            if(voltageValue > voltageStat.max) voltageStat.max = voltageValue;
            if(voltageValue < voltageStat.min) voltageStat.min = voltageValue;
            if(currentValue > currentStat.max) currentStat.max = currentValue;
            if(currentValue < currentStat.min) currentStat.min = currentValue;

            voltageDataCollected[lastBufferUsedPositionIndex] = voltageValue;
            voltageStat.average += voltageValue;
            currentStat.average += currentValue;

            if(i == 0)
            {
                voltageKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue;
                currentKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue;
                consumptionKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue;
                if(ebp != 0 && ebpNo > 0)
                {
                    ebpValue.append(lastCumulativeCurrentConsumptionValue);
                    ebpValueKey.append(keyStartValue);
                    emit sigEBPValue(packetID, lastCumulativeCurrentConsumptionValue, keyStartValue);
                }
            }
            else
            {
                voltageKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + (double)j*samplingPeriod;
                currentKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + (double)j*samplingPeriod;
                consumptionKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + (double)j*samplingPeriod;
//                voltageKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + (double)j;
//                currentKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + (double)j;
//                consumptionKeysDataCollected[lastBufferUsedPositionIndex] = keyStartValue + (double)j;
            }
            currentDataCollected[lastBufferUsedPositionIndex] = currentValue;
            currentConsumptionDataCollected[lastBufferUsedPositionIndex] = swapDataCurrent*(samplingPeriod)/3600000; //mAh
            lastCumulativeCurrentConsumptionValue += swapDataCurrent*(samplingPeriod)/3600000;                         //This value remember last consumption in case when buffers are restarted
            cumulativeConsumptionDataCollected[lastBufferUsedPositionIndex] = lastCumulativeCurrentConsumptionValue;            
            //fftKeysDataCollected[lastBufferUsedPositionIndex] = lastBufferUsedPositionIndex*1/(samplingPeriod)*1000;

            i                           += 1;
            lastBufferUsedPositionIndex += 1;
            j += 1;
        }

    }

    currentNumberOfBuffers += 1;
    if(currentNumberOfBuffers == maxNumberOfBuffers)
    {

//        qDebug() << "---------------------------------------";
//        qDebug() << "MinV, MaxV =" << QString::number(minVoltage) << "," <<  QString::number(maxVoltage);
//        qDebug() << "MinC, MaxC =" << QString::number(minCurrent) << "," <<  QString::number(maxCurrent);
//        qDebug() << "V-Dev =" << QString::number(maxVoltage - minVoltage);
//        qDebug() << "I-Dev =" << QString::number(maxCurrent - minCurrent);
        processSignalWithFFT(voltageDataCollected, 0.0005, voltageDataCollectedFiltered, fftDataCollectedVoltage, samplingPeriod, fftKeysDataCollected, minMax);
        if(minMax[0] > maxVoltageF) maxVoltageF = minMax[0];
        if(minMax[1] < minVoltageF) minVoltageF = minMax[1];
        processSignalWithFFT(currentDataCollected, 0.002, currentDataCollectedFiltered, fftDataCollectedCurrent, samplingPeriod, fftKeysDataCollected, minMax);
        if(minMax[0] > maxCurrentF) maxCurrentF = minMax[0];
        if(minMax[1] < minCurrentF) minCurrentF = minMax[1];
//        qDebug() << "MinVF, MaxVF =" << QString::number(minVoltageF) << "," <<  QString::number(maxVoltageF);
//        qDebug() << "MinCF, MaxCF =" << QString::number(minCurrentF) << "," <<  QString::number(maxCurrentF);
//        qDebug() << "V-DevF =" << QString::number(maxVoltageF - minVoltageF);
//        qDebug() << "I-DevF =" << QString::number(maxCurrentF - minCurrentF);

        voltageStat.average /= (lastBufferUsedPositionIndex + 1);
        currentStat.average /= (lastBufferUsedPositionIndex + 1);
        consumptionStat.average = 0;
        consumptionStat.max = lastCumulativeCurrentConsumptionValue;
        consumptionStat.min = 0;

        switch(consumptionMode)
        {
        case DATAPROCESSING_CONSUMPTION_MODE_CURRENT:

            switch(measurementMode)
            {
            case DATAPROCESSING_MEASUREMENT_MODE_VOLTAGE:
                emit sigNewVoltageCurrentSamplesReceived(voltageDataCollected, voltageDataCollectedFiltered, voltageKeysDataCollected, voltageKeysDataCollected);
                emit sigNewConsumptionDataReceived(fftDataCollectedVoltage, fftKeysDataCollected, DATAPROCESSING_CONSUMPTION_MODE_CURRENT);

                break;
            case DATAPROCESSING_MEASUREMENT_MODE_CURRENT:
                emit sigNewVoltageCurrentSamplesReceived(currentDataCollected, currentDataCollectedFiltered, currentKeysDataCollected, currentKeysDataCollected);
                emit sigNewConsumptionDataReceived(fftDataCollectedCurrent, fftKeysDataCollected, DATAPROCESSING_CONSUMPTION_MODE_CURRENT);
                 break;
            }
            break;
        case DATAPROCESSING_CONSUMPTION_MODE_CUMULATIVE:
            emit sigNewVoltageCurrentSamplesReceived(voltageDataCollected, currentDataCollected, voltageKeysDataCollected, currentKeysDataCollected);
            emit sigNewConsumptionDataReceived(cumulativeConsumptionDataCollected, consumptionKeysDataCollected, DATAPROCESSING_CONSUMPTION_MODE_CUMULATIVE);
            break;
        }
        emit sigEBP(ebpValue, ebpValueKey);
        emit sigSamplesBufferReceiveStatistics(dropRate, dropPacketsNo, receivedPacketCounter, lastReceivedPacketID, ebpNo);
        emit sigSignalStatistics(voltageStat, currentStat, consumptionStat);
        initBuffers();
    }
}

void DataProcessing::initBuffers()
{
    initVoltageBuffer();
    initFFTBuffer();
    initCurrentBuffer();
    initConsumptionBuffer();
    initKeyBuffer();
    initEBPBuffer();
    initStatData();
}

void DataProcessing::initStatData()
{
    voltageStat.average = 0;
    currentStat.average = 0;
}

void DataProcessing::initVoltageBuffer()
{
    voltageDataCollected.resize(maxNumberOfBuffers*samplesBufferSize);
    voltageDataCollected.fill(0);
    currentNumberOfBuffers = 0;
    lastBufferUsedPositionIndex = 0;
}
void DataProcessing::initFFTBuffer()
{
    fftDataCollectedVoltage.resize(maxNumberOfBuffers*samplesBufferSize);
    fftDataCollectedVoltage.fill(0);
    fftDataCollectedCurrent.resize(maxNumberOfBuffers*samplesBufferSize);
    fftDataCollectedCurrent.fill(0);
    voltageDataCollectedFiltered.resize(maxNumberOfBuffers*samplesBufferSize);
    voltageDataCollectedFiltered.fill(0);
    currentDataCollectedFiltered.resize(maxNumberOfBuffers*samplesBufferSize);
    currentDataCollectedFiltered.fill(0);
    fftKeysDataCollected.resize(maxNumberOfBuffers*samplesBufferSize);
    fftKeysDataCollected.fill(0);
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

void DataProcessing::initEBPBuffer()
{
    ebpFlags.resize(maxNumberOfBuffers);
//    ebpValue.resize(maxNumberOfBuffers);
//    ebpValueKey.resize(maxNumberOfBuffers);
    ebpFlags.fill(0);
    ebpNo = 0;
}
