#include "devicecontainer.h"

DeviceContainer::DeviceContainer(QObject *parent,  DeviceWnd* aDeviceWnd, Device* aDevice)
    : QObject{parent}
{
    deviceWnd       = aDeviceWnd;
    device          = aDevice;
    log             = new Log();
    fileProcessing  = new FileProcessing();
    log->assignLogWidget(deviceWnd->getLogWidget());

    elapsedTime     = 0;
    timer           = new QTimer();

    connect(timer,      SIGNAL(timeout()),                                           this, SLOT(onTimeout()));


    /*Device window signals*/
    connect(deviceWnd,  SIGNAL(sigWndClosed()),                                     this, SLOT(onDeviceWndClosed()));
    connect(deviceWnd,  SIGNAL(sigNewControlMessageRcvd(QString)),                  this, SLOT(onConsoleWndMessageRcvd(QString)));
    connect(deviceWnd,  SIGNAL(sigADCChanged(QString)),                             this, SLOT(onDeviceWndADCChanged(QString)));
    connect(deviceWnd,  SIGNAL(sigResolutionChanged(QString)),                      this, SLOT(onDeviceWndResolutionChanged(QString)));
    connect(deviceWnd,  SIGNAL(sigClockDivChanged(QString)),                        this, SLOT(onDeviceWndClockDivChanged(QString)));
    connect(deviceWnd,  SIGNAL(sigSampleTimeChanged(QString)),                      this, SLOT(onDeviceWndChannelSamplingTimeChanged(QString)));
    connect(deviceWnd,  SIGNAL(sigSamplingPeriodChanged(QString)),                  this, SLOT(onDeviceWndSamplingPeriodChanged(QString)));
    connect(deviceWnd,  SIGNAL(sigAvrRatioChanged(QString)),                        this, SLOT(onDeviceWndAvrRatioChanged(QString)));
    connect(deviceWnd,  SIGNAL(sigVOffsetChanged(QString)),                         this, SLOT(onDeviceWndVOffsetChanged(QString)));
    connect(deviceWnd,  SIGNAL(sigCOffsetChanged(QString)),                         this, SLOT(onDeviceWndCOffsetChanged(QString)));
    connect(deviceWnd,  SIGNAL(sigNewInterfaceSelected(QString)),                   this, SLOT(onDeviceWndInterfaceChanged(QString)));
    connect(deviceWnd,  SIGNAL(sigStartAcquisition()),                              this, SLOT(onDeviceWndAcquisitionStart()));
    connect(deviceWnd,  SIGNAL(sigStopAcquisition()),                               this, SLOT(onDeviceWndAcquisitionStop()));
    connect(deviceWnd,  SIGNAL(sigPauseAcquisition()),                              this, SLOT(onDeviceWndAcquisitionPause()));
    connect(deviceWnd,  SIGNAL(sigRefreshAcquisition()),                            this, SLOT(onDeviceWndAcquisitionRefresh()));
    connect(deviceWnd,  SIGNAL(sigAdvConfigurationReqested()),                      this, SLOT(onDeviceWndAdvConfGet()));
    connect(deviceWnd,  SIGNAL(sigAdvConfigurationChanged(QVariant)),               this, SLOT(onDeviceWndNewConfiguration(QVariant)));
    connect(deviceWnd,  SIGNAL(sigMaxNumberOfBuffersChanged(uint)),                 this, SLOT(onDeviceWndMaxNumberOfBuffersChanged(uint)));
    connect(deviceWnd,  SIGNAL(sigConsumptionTypeChanged(QString)),                 this, SLOT(onDeviceWndConsumptionTypeChanged(QString)));
    connect(deviceWnd,  SIGNAL(sigMeasurementTypeChanged(QString)),                 this, SLOT(onDeviceWndMeasurementTypeChanged(QString)));
    connect(deviceWnd,  SIGNAL(sigPathChanged(QString)),                            this, SLOT(onDeviceWndSamplesSavePathChanged(QString)));

    /*Device signals*/
    connect(device,     SIGNAL(sigControlLinkConnected()),                          this, SLOT(onDeviceControlLinkConnected()));
    connect(device,     SIGNAL(sigControlLinkDisconnected()),                       this, SLOT(onDeviceControlLinkDisconnected()));
    connect(device,     SIGNAL(sigStatusLinkNewDeviceAdded(QString)),               this, SLOT(onDeviceStatusLinkNewDeviceAdded(QString)));
    connect(device,     SIGNAL(sigStatusLinkNewMessageReceived(QString,QString)),   this, SLOT(onDeviceStatusLinkNewMessageReceived(QString,QString)));
    connect(device,     SIGNAL(sigNewResponseReceived(QString, bool)),              this, SLOT(onDeviceHandleControlMsgResponse(QString, bool)));
    connect(device,     SIGNAL(sigResolutionObtained(QString)),                     this, SLOT(onDeviceResolutionObtained(QString)));
    connect(device,     SIGNAL(sigChSampleTimeObtained(QString)),                   this, SLOT(onDeviceChSampleTimeObtained(QString)));
    connect(device,     SIGNAL(sigClockDivObtained(QString)),                       this, SLOT(onDeviceClkDivObtained(QString)));
    connect(device,     SIGNAL(sigSampleTimeObtained(QString)),                     this, SLOT(onDeviceSamplingPeriodObtained(QString)));
    connect(device,     SIGNAL(sigAdcInputClkObtained(QString)),                    this, SLOT(onDeviceAdcInClkObtained(QString)));
    connect(device,     SIGNAL(sigCOffsetObtained(QString)),                        this, SLOT(onDeviceCOffsetObtained(QString)));
    connect(device,     SIGNAL(sigVOffsetObtained(QString)),                        this, SLOT(onDeviceVOffsetObtained(QString)));
    connect(device,     SIGNAL(sigAvgRatio(QString)),                               this, SLOT(onDeviceAvgRatioChanged(QString)));
    connect(device,     SIGNAL(sigSamplingTimeChanged(double)),                     this, SLOT(onDeviceSamplingTimeChanged(double)));
    connect(device,     SIGNAL(sigAcqusitionStarted()),                             this, SLOT(onDeviceAcquisitonStarted()));
    connect(device,     SIGNAL(sigAcqusitionStopped()),                             this, SLOT(onDeviceAcquisitonStopped()));

    connect(device,     SIGNAL(sigVoltageCurrentSamplesReceived(QVector<double>,QVector<double>,QVector<double>, QVector<double>)),
            this, SLOT(onDeviceNewVoltageCurrentSamplesReceived(QVector<double>,QVector<double>,QVector<double>, QVector<double>)));
    connect(device,     SIGNAL(sigNewSamplesBuffersProcessingStatistics(double,uint,uint,uint, unsigned short)), this, SLOT(onDeviceNewSamplesBuffersProcessingStatistics(double,uint,uint,uint, unsigned short)));
    connect(device,     SIGNAL(sigNewConsumptionDataReceived(QVector<double>,QVector<double>, dataprocessing_consumption_mode_t)),
            this, SLOT(onDeviceNewConsumptionDataReceived(QVector<double>,QVector<double>, dataprocessing_consumption_mode_t)));
    connect(device,     SIGNAL(sigNewEBP(QVector<double>,QVector<double>)), this, SLOT(onDeviceNewEBP(QVector<double>,QVector<double>)));
    connect(device,     SIGNAL(sigNewEBPFull(double,double,QString)), this, SLOT(onDeviceNewEBPFull(double,double,QString)));

    log->printLogMessage("Device container successfully created", LOG_MESSAGE_TYPE_INFO);
    device->statusLinkCreate();
    device->epLinkServerCreate();

}

DeviceContainer::~DeviceContainer()
{
    delete deviceWnd;
    delete device;
    delete log;
}

void DeviceContainer::onDeviceControlLinkDisconnected()
{
    log->printLogMessage("Device control link disconnected", LOG_MESSAGE_TYPE_WARNING);
    deviceWnd->setDeviceState(DEVICE_STATE_DISCONNECTED);
}

void DeviceContainer::onDeviceControlLinkConnected()
{
    log->printLogMessage("Device control link established", LOG_MESSAGE_TYPE_INFO);
    deviceWnd->setDeviceState(DEVICE_STATE_CONNECTED);
}

void DeviceContainer::onDeviceStatusLinkNewDeviceAdded(QString aDeviceIP)
{
    log->printLogMessage("Status link sucessfully establish with device(IP: " + aDeviceIP + ")", LOG_MESSAGE_TYPE_INFO);
}

void DeviceContainer::onDeviceStatusLinkNewMessageReceived(QString aDeviceIP, QString aMessage)
{
    log->printLogMessage("New message received from device (IP: " + aDeviceIP + ") :\" " + aMessage + "\"", LOG_MESSAGE_TYPE_INFO, LOG_MESSAGE_DEVICE_TYPE_DEVICE);
}

void DeviceContainer::onDeviceWndClosed()
{
    emit sigDeviceClosed(device);
}

void DeviceContainer::onDeviceWndMaxNumberOfBuffersChanged(unsigned int maxNumber)
{
    if(device->setDataProcessingMaxNumberOfBuffers(maxNumber))
    {
        log->printLogMessage("Max number of samples buffers sucessfully configured", LOG_MESSAGE_TYPE_INFO);
    }
    else
    {
        log->printLogMessage("Unable to sucessfully configure max number of samples buffers", LOG_MESSAGE_TYPE_ERROR);
    }
}

void DeviceContainer::onDeviceWndConsumptionTypeChanged(QString aConsumptionType)
{
    dataprocessing_consumption_mode_t consumptionType;
    if(aConsumptionType == "Current") consumptionType = DATAPROCESSING_CONSUMPTION_MODE_CURRENT;
    if(aConsumptionType == "Cumulative") consumptionType = DATAPROCESSING_CONSUMPTION_MODE_CUMULATIVE;
    if(device->setDataProcessingConsumptionType(consumptionType))
    {
        log->printLogMessage("Consumption type: \"" + aConsumptionType + "\" successfully set", LOG_MESSAGE_TYPE_INFO);
    }
    else
    {
        log->printLogMessage("Unable to sucessfully configure Consumption type", LOG_MESSAGE_TYPE_ERROR);
    }
}

void DeviceContainer::onDeviceWndMeasurementTypeChanged(QString aMeasurementType)
{
    dataprocessing_measurement_mode_t measurementType;
    if(aMeasurementType == "Current") measurementType = DATAPROCESSING_MEASUREMENT_MODE_CURRENT;
    if(aMeasurementType == "Voltage") measurementType = DATAPROCESSING_MEASUREMENT_MODE_VOLTAGE;
    if(device->setDataProcessingMeasurementType(measurementType))
    {
        log->printLogMessage("Measurement type: \"" + aMeasurementType + "\" successfully set", LOG_MESSAGE_TYPE_INFO);
    }
    else
    {
        log->printLogMessage("Unable to sucessfully configure Consumption type", LOG_MESSAGE_TYPE_ERROR);
    }
}

void DeviceContainer::onDeviceWndSamplesSavePathChanged(QString path)
{
    if(fileProcessing->open(FILEPROCESSING_TYPE_SAMPLES, path))
    {
        log->printLogMessage("Samples log file sucessfully opened (Path = " + path + ")", LOG_MESSAGE_TYPE_INFO);
        fileProcessing->setSamplesFileHeader("Voltage and Current samples");
        fileProcessing->setConsumptionFileHeader("Consumption samples");
    }
    else
    {
        log->printLogMessage("Unable to open samples log file (Path = " + path + ")", LOG_MESSAGE_TYPE_ERROR);
    }
}

void DeviceContainer::onConsoleWndMessageRcvd(QString msg)
{
    /* call device funtion sendControl Msg -> */
    device->sendControlMsg(msg);
}

void DeviceContainer::onDeviceHandleControlMsgResponse(QString msg, bool exeStatus)
{
    /* call deviceWnd function with recieved msg from FW <- */
    deviceWnd->printConsoleMsg(msg, exeStatus);
}

void DeviceContainer::onDeviceWndResolutionChanged(QString resolution)
{
    /* call deviceWnd function with recieved msg from FW <- */
    device_adc_resolution_t tmpResolution = getAdcResolutionFromString(resolution);
    if(!device->setResolution(tmpResolution))
    {
        log->printLogMessage("Unable to set resolution: " + resolution, LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Resolution: " + resolution + " sucessfully set", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceWndADCChanged(QString adc)
{
    /* call deviceWnd function with recieved msg from FW <- */
    device_adc_t tmpADC = getAdcFromString(adc);
    if(!device->acquireDeviceConfiguration(tmpADC))
    {
        log->printLogMessage("unable to change ADC Device", LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("ADC Device successfully changed", LOG_MESSAGE_TYPE_INFO);
        deviceWnd->setAdc(adc);
    }
}

void DeviceContainer::onDeviceWndClockDivChanged(QString clockDiv)
{
    /* call deviceWnd function with recieved msg from FW <- */
    device_adc_clock_div_t tmpClockDiv = getAdcClockDivFromString(clockDiv);
    if(!device->setClockDiv(tmpClockDiv))
    {
        log->printLogMessage("Unable to set clock div: " + clockDiv, LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Clock div: " + clockDiv + " sucessfully set", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceWndChannelSamplingTimeChanged(QString stime)
{
    /* call deviceWnd function with recieved msg from FW <- */
    device_adc_ch_sampling_time_t tmpSampleTime = getAdcChSamplingTimeFromString(stime);
    if(!device->setChSampleTime(tmpSampleTime))
    {
        log->printLogMessage("Unable to set sample time: " + stime, LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Sample time: " + stime + " sucessfully set", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceWndAvrRatioChanged(QString avgRatio)
{
    /* call deviceWnd function with recieved msg from FW <- */
    device_adc_averaging_t tmpAveragingRatio = getAdcAvgRatioFromString(avgRatio);
    if(!device->setAvrRatio(tmpAveragingRatio))
    {
        log->printLogMessage("Unable to set averaging ratio: " + avgRatio, LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Averaging ratio: " + avgRatio + " sucessfully set", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceWndSamplingPeriodChanged(QString time)
{
    bool conversionOk;
    double  numericValue = time.toDouble(&conversionOk);

    if(!device->setSamplingPeriod(time))
    {
        log->printLogMessage("Unable to set sampling time: " + time, LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Sampling time: " + time + " sucessfully set", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceWndInterfaceChanged(QString interfaceIp)
{
    int streamID = -1;
    if(!device->createStreamLink(interfaceIp, 11223, &streamID))
    {
        log->printLogMessage("Unable to create stream link: ", LOG_MESSAGE_TYPE_ERROR);
        deviceWnd->setDeviceInterfaceSelectionState(DEVICE_INTERFACE_SELECTION_STATE_UNDEFINED);
    }
    else
    {
        log->printLogMessage("Stream link ( sid="+ QString::number(streamID) + " ) sucessfully created: ", LOG_MESSAGE_TYPE_INFO);
        if(!device->establishEPLink(interfaceIp))
        {
            log->printLogMessage("Unable to create ep link: ", LOG_MESSAGE_TYPE_ERROR);
        }
        else
        {
            log->printLogMessage("Ep link ( port="+ QString::number(8000) + " ) sucessfully created: ", LOG_MESSAGE_TYPE_INFO);
        }
        deviceWnd->setDeviceInterfaceSelectionState(DEVICE_INTERFACE_SELECTION_STATE_SELECTED);
        deviceWnd->setAdc("Int");
        device->acquireDeviceConfiguration(DEVICE_ADC_INTERNAL);
    }
}

void DeviceContainer::onDeviceWndVOffsetChanged(QString off)
{
    bool conversionOk;
    int  numericValue = off.toInt(&conversionOk);
    if(!(conversionOk && numericValue >0))
    {
        return;
    }

    if(!device->setVOffset(off))
    {
        log->printLogMessage("Unable to set voltage offset: " + off, LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Voltage offset: " + off + " sucessfully set", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceWndCOffsetChanged(QString off)
{
    bool conversionOk;
    int  numericValue = off.toInt(&conversionOk);
    if(!(conversionOk && numericValue >0))
    {
        return;
    }

    if(!device->setCOffset(off))
    {
        log->printLogMessage("Unable to set current offset: " + off, LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Current offset: " + off + " sucessfully set", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceWndAcquisitionStart()
{
    if(!device->acquisitionStart())
    {
        log->printLogMessage("Unable to start acquistion", LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Acquisition sucessfully started", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceWndAcquisitionStop()
{
    if(!device->acquisitionStop())
    {
        log->printLogMessage("Unable to stop acquistion", LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Acquisition sucessfully stoped", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceWndAcquisitionPause()
{
    if(!device->acquisitionPause())
    {
        log->printLogMessage("Unable to pause acquistion", LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Acquisition sucessfully paused", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceWndAcquisitionRefresh()
{
    device->acquireDeviceConfiguration();
}

void DeviceContainer::onDeviceWndAdvConfGet()
{
    device->acquireDeviceConfiguration();
}

void DeviceContainer::onDeviceWndNewConfiguration(QVariant newConfig)
{
    advConfigurationData config = newConfig.value<advConfigurationData>();

    if(config.resolution != "")
    {
        device_adc_resolution_t adcRes = getAdcResolutionFromString(config.resolution);
        if(adcRes == DEVICE_ADC_RESOLUTION_UKNOWN)
        {
            log->printLogMessage("Unable to  obtain resolution", LOG_MESSAGE_TYPE_ERROR);
            return;
        }
        if(!device->setResolution(adcRes))
        {
            log->printLogMessage("Unable to set resolution: " + config.resolution, LOG_MESSAGE_TYPE_ERROR);
        }
        else
        {
            log->printLogMessage("Resolution: " + config.resolution + "bit  - sucessfully set", LOG_MESSAGE_TYPE_INFO);
            deviceWnd->setResolution(config.resolution);
        }
    }

    if(config.clockDiv != "")
    {
        device_adc_clock_div_t clkDiv = getAdcClockDivFromString(config.clockDiv);
        if(clkDiv == DEVICE_ADC_CLOCK_DIV_UKNOWN)
        {
            log->printLogMessage("Unable to obtain clock div", LOG_MESSAGE_TYPE_ERROR);
            return;
        }
        if(!device->setClockDiv(clkDiv))
        {
            log->printLogMessage("Unable to set clock div: " + config.clockDiv, LOG_MESSAGE_TYPE_ERROR);
        }
        else
        {
            log->printLogMessage("Clock div: " + config.clockDiv + " - sucessfully set", LOG_MESSAGE_TYPE_INFO);
            deviceWnd->setClkDiv(config.clockDiv);

        }
    }

    if(config.averaginRatio != "")
    {
        device_adc_averaging_t avgRatio = getAdcAvgRatioFromString(config.averaginRatio);
        if(avgRatio == DEVICE_ADC_AVERAGING_UKNOWN)
        {
            log->printLogMessage("Unable to obtain averaging ratio", LOG_MESSAGE_TYPE_ERROR);
            return;
        }
        if(!device->setAvrRatio(avgRatio))
        {
            log->printLogMessage("Unable to set averaging ratio: " + config.averaginRatio, LOG_MESSAGE_TYPE_ERROR);
        }
        else
        {
            log->printLogMessage("Averaging ratio: " + config.averaginRatio + " - sucessfully set", LOG_MESSAGE_TYPE_INFO);
            deviceWnd->setChAvgRatio(config.averaginRatio);

        }
    }

    if(config.chSTime != "")
    {
        device_adc_ch_sampling_time_t chSTime = getAdcChSamplingTimeFromString(config.chSTime);
        if(chSTime == DEVICE_ADC_SAMPLING_TIME_UKNOWN)
        {
            log->printLogMessage("Unable to obatin channel sampling time", LOG_MESSAGE_TYPE_ERROR);
            return;
        }
        if(!device->setChSampleTime(chSTime))
        {
            log->printLogMessage("Unable to set channels sampling time: " + config.chSTime, LOG_MESSAGE_TYPE_ERROR);
        }
        else
        {
            log->printLogMessage("Channels sampling time: " + config.chSTime + " - sucessfully set", LOG_MESSAGE_TYPE_INFO);
            deviceWnd->setChSamplingTime(config.chSTime);
        }
    }

    if(config.currentOffset != "")
    {
        if(!device->setCOffset(config.currentOffset))
        {
            log->printLogMessage("Unable to set current offset: " + config.currentOffset, LOG_MESSAGE_TYPE_ERROR);
        }
        else
        {
            log->printLogMessage("Current offset: " + config.currentOffset + "[mV] - sucessfully set", LOG_MESSAGE_TYPE_INFO);
            deviceWnd->setCOffset(config.currentOffset);
        }
    }

    if(config.voltageOffset != "")
    {
        if(!device->setVOffset(config.voltageOffset))
        {
            log->printLogMessage("Unable to set voltage offset: " + config.voltageOffset, LOG_MESSAGE_TYPE_ERROR);
        }
        else
        {
            log->printLogMessage("Voltage offset: " + config.voltageOffset + "[mV] - sucessfully set", LOG_MESSAGE_TYPE_INFO);
            deviceWnd->setVOffset(config.voltageOffset);
        }
    }

    if(config.samplingTime != "")
    {
        if(!device->setSamplingPeriod(config.samplingTime))
        {
            log->printLogMessage("Unable to set sampling time: " + config.samplingTime, LOG_MESSAGE_TYPE_ERROR);
        }
        else
        {
            log->printLogMessage("Sampling time: " + config.samplingTime + "[ms] - sucessfully set", LOG_MESSAGE_TYPE_INFO);
            deviceWnd->setSamplingPeriod(config.samplingTime);
        }
    }

}

void DeviceContainer::onDeviceResolutionObtained(QString resolution)
{
    if(!deviceWnd->setResolution(resolution))
    {
        log->printLogMessage("Unable to show obtained resolution: " + resolution, LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Resolution sucessfully obtained and presented ", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceClkDivObtained(QString clkDiv)
{
    if(!deviceWnd->setClkDiv(clkDiv))
    {
        log->printLogMessage("Unable to show obtained clock div: " + clkDiv, LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Clock div sucessfully obained and presented ", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceChSampleTimeObtained(QString stime)
{
    if(!deviceWnd->setChSamplingTime(stime))
    {
        log->printLogMessage("Unable to show obtained channels sampling time: " + stime, LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Channels sampling time sucessfully obained and presented ", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceSamplingPeriodObtained(QString stime)
{
    if(!deviceWnd->setSamplingPeriod(stime))
    {
        log->printLogMessage("Unable to show obtained sampling time: " + stime + "us", LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Sampling time sucessfully obained and presented ", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceAdcInClkObtained(QString inClk)
{
    if(!deviceWnd->setInCkl(inClk))
    {
        log->printLogMessage("Unable to show obtained adc input clock: " + inClk + "MHz", LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Adc input clock sucessfully obained and presented ", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceAvgRatioChanged(QString aAvgRatio)
{
    if(!deviceWnd->setChAvgRatio(aAvgRatio))
    {
        log->printLogMessage("Unable to show obtained avg ratio: " + aAvgRatio, LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Average ratio scuessfully obtained", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceCOffsetObtained(QString coffset)
{
    if(!deviceWnd->setCOffset(coffset))
    {
        log->printLogMessage("Unable to show obtained current offset: " + coffset + "mA", LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Current offset sucessfully obained and presented ", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceVOffsetObtained(QString voffset)
{
    if(!deviceWnd->setVOffset(voffset))
    {
        log->printLogMessage("Unable to show obtained voltage offset: " + voffset + "mV", LOG_MESSAGE_TYPE_ERROR);
    }
    else
    {
        log->printLogMessage("Voltage offset sucessfully obained and presented ", LOG_MESSAGE_TYPE_INFO);
    }
}

void DeviceContainer::onDeviceSamplingTimeChanged(double value)
{
    deviceWnd->setStatisticsSamplingTime(value);
}

void DeviceContainer::onDeviceAcquisitonStarted()
{
    elapsedTime = 0;
    timer->start(1000);

}

void DeviceContainer::onDeviceAcquisitonStopped()
{
    elapsedTime = 0;
    timer->stop();
    deviceWnd->setStatisticsElapsedTime(elapsedTime);
}

void DeviceContainer::onTimeout()
{
    elapsedTime += 1;
    deviceWnd->setStatisticsElapsedTime(elapsedTime);
}

void DeviceContainer::onDeviceNewVoltageCurrentSamplesReceived(QVector<double> voltage, QVector<double> current, QVector<double> voltageKeys, QVector<double> currentKeys)
{
    deviceWnd->plotVoltageValues(voltage, voltageKeys);
    deviceWnd->plotCurrentValues(current, currentKeys);
    fileProcessing->appendSampleDataQueued(voltage, voltageKeys, current, currentKeys);
}

void DeviceContainer::onDeviceNewConsumptionDataReceived(QVector<double> consumption, QVector<double> keys, dataprocessing_consumption_mode_t mode)
{
    deviceWnd->plotConsumptionValues(consumption, keys);
    fileProcessing->appendConsumptionQueued(consumption, keys);
}

void DeviceContainer::onDeviceNewSamplesBuffersProcessingStatistics(double dropRate, unsigned int dropPacketsNo, unsigned int fullReceivedBuffersNo, unsigned int lastBufferID, unsigned short ebp)
{
    deviceWnd->setStatisticsData(dropRate, dropPacketsNo, fullReceivedBuffersNo, lastBufferID);
}

void DeviceContainer::onDeviceNewEBP(QVector<double> ebpValues, QVector<double> keys)
{
    //deviceWnd->plotAppendConsumptionEBP(ebpValues, keys);
}

void DeviceContainer::onDeviceNewEBPFull(double value, double key, QString name)
{
    deviceWnd->plotConsumptionEBPWithName(value, key, name);
    log->printLogMessage("New Energy point received", LOG_MESSAGE_TYPE_INFO);
}

device_adc_resolution_t DeviceContainer::getAdcResolutionFromString(QString resolution)
{
    device_adc_resolution_t returnResolution = DEVICE_ADC_RESOLUTION_UKNOWN;
    QStringList *resolutionOptions = deviceWnd->getResolutionOptions();
    switch(resolutionOptions->indexOf(resolution))
    {
    case 0:
        returnResolution = DEVICE_ADC_RESOLUTION_UKNOWN;
        break;
    case 1:
        returnResolution = DEVICE_ADC_RESOLUTION_16BIT;
        break;
    case 2:
        returnResolution = DEVICE_ADC_RESOLUTION_14BIT;
        break;
    case 3:
        returnResolution = DEVICE_ADC_RESOLUTION_12BIT;
        break;
    case 4:
        returnResolution = DEVICE_ADC_RESOLUTION_10BIT;
        break;
    default:
        returnResolution = DEVICE_ADC_RESOLUTION_UKNOWN;
        break;
    }
    return returnResolution;
}

device_adc_clock_div_t DeviceContainer::getAdcClockDivFromString(QString clkDiv)
{
    device_adc_clock_div_t returnClkDiv = DEVICE_ADC_CLOCK_DIV_UKNOWN;
    QStringList *clkDivOptions = deviceWnd->getClockDivOptions();
    switch(clkDivOptions->indexOf(clkDiv))
    {
    case 0:
        returnClkDiv = DEVICE_ADC_CLOCK_DIV_UKNOWN;
        break;
    case 1:
        returnClkDiv = DEVICE_ADC_CLOCK_DIV_1;
        break;
    case 2:
        returnClkDiv = DEVICE_ADC_CLOCK_DIV_2;
        break;
    case 3:
        returnClkDiv = DEVICE_ADC_CLOCK_DIV_4;
        break;
    case 4:
        returnClkDiv = DEVICE_ADC_CLOCK_DIV_8;
        break;
    case 5:
        returnClkDiv = DEVICE_ADC_CLOCK_DIV_16;
        break;
    case 6:
        returnClkDiv = DEVICE_ADC_CLOCK_DIV_32;
        break;
    case 7:
        returnClkDiv = DEVICE_ADC_CLOCK_DIV_64;
        break;
    case 8:
        returnClkDiv = DEVICE_ADC_CLOCK_DIV_128;
        break;
    case 9:
        returnClkDiv = DEVICE_ADC_CLOCK_DIV_256;
        break;
    default:
        returnClkDiv = DEVICE_ADC_CLOCK_DIV_UKNOWN;
        break;
    }
    return returnClkDiv;
}

device_adc_ch_sampling_time_t DeviceContainer::getAdcChSamplingTimeFromString(QString chstime)
{
    device_adc_ch_sampling_time_t returnSTime = DEVICE_ADC_SAMPLING_TIME_UKNOWN;
    QStringList *stimeOptions = deviceWnd->getChSamplingTimeOptions();
    switch(stimeOptions->indexOf(chstime))
    {
    case 0:
        returnSTime = DEVICE_ADC_SAMPLING_TIME_UKNOWN;
        break;
    case 1:
        returnSTime = DEVICE_ADC_SAMPLING_TIME_1C5;
        break;
    case 2:
        returnSTime = DEVICE_ADC_SAMPLING_TIME_2C5;
        break;
    case 3:
        returnSTime = DEVICE_ADC_SAMPLING_TIME_8C5;
        break;
    case 4:
        returnSTime = DEVICE_ADC_SAMPLING_TIME_16C5;
        break;
    case 5:
        returnSTime = DEVICE_ADC_SAMPLING_TIME_32C5;
        break;
    case 6:
        returnSTime = DEVICE_ADC_SAMPLING_TIME_64C5;
        break;
    case 7:
        returnSTime = DEVICE_ADC_SAMPLING_TIME_387C5;
        break;
    case 9:
        returnSTime = DEVICE_ADC_SAMPLING_TIME_810C5;
        break;
    default:
        returnSTime = DEVICE_ADC_SAMPLING_TIME_UKNOWN;
        break;
    }
    return returnSTime;
}

device_adc_averaging_t DeviceContainer::getAdcAvgRatioFromString(QString avgRatio)
{
    device_adc_averaging_t returnAvgRatio = DEVICE_ADC_AVERAGING_UKNOWN;
    QStringList *avgOptions = deviceWnd->getChAvgRationOptions();
    switch(avgOptions->indexOf(avgRatio))
    {
    case 0:
        returnAvgRatio = DEVICE_ADC_AVERAGING_UKNOWN;
        break;
    case 1:
        returnAvgRatio = DEVICE_ADC_AVERAGING_DISABLED;
        break;
    case 2:
        returnAvgRatio = DEVICE_ADC_AVERAGING_2;
        break;
    case 3:
        returnAvgRatio = DEVICE_ADC_AVERAGING_4;
        break;
    case 4:
        returnAvgRatio = DEVICE_ADC_AVERAGING_8;
        break;
    case 5:
        returnAvgRatio = DEVICE_ADC_AVERAGING_16;
        break;
    case 6:
        returnAvgRatio = DEVICE_ADC_AVERAGING_32;
        break;
    case 7:
        returnAvgRatio = DEVICE_ADC_AVERAGING_64;
        break;
    case 8:
        returnAvgRatio = DEVICE_ADC_AVERAGING_128;
        break;
    case 9:
        returnAvgRatio = DEVICE_ADC_AVERAGING_256;
        break;
    case 10:
        returnAvgRatio = DEVICE_ADC_AVERAGING_512;
        break;
    case 11:
        returnAvgRatio = DEVICE_ADC_AVERAGING_1024;
        break;
    default:
        returnAvgRatio = DEVICE_ADC_AVERAGING_UKNOWN;
        break;
    }
    return returnAvgRatio;
}

device_adc_t DeviceContainer::getAdcFromString(QString adc)
{
    device_adc_t returnAdc = DEVICE_ADC_INTERNAL;
    QStringList *adcOptions = deviceWnd->getADCOptions();
    switch(adcOptions->indexOf(adc))
    {
    case 0:
        returnAdc = DEVICE_ADC_UNKNOWN;
        break;
    case 1:
        returnAdc = DEVICE_ADC_INTERNAL;
        break;
    case 2:
        returnAdc = DEVICE_ADC_EXTERNAL;
        break;
    default:
        returnAdc = DEVICE_ADC_UNKNOWN;
        break;
    }
    return returnAdc;
}
