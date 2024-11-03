#include "devicewnd.h"
#include "ui_devicewnd.h"
#include "Windows/Console/consolewnd.h"
#include <QFileDialog>
#include <QNetworkInterface>
#include <QOpenGLWidget>

/*TODO: Declare this in config file*/
#define PLOT_MINIMUM_SIZE_WIDTH     600
#define PLOT_MINIMUM_SIZE_HEIGHT    200


DeviceWnd::DeviceWnd(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DeviceWnd)
{

    QFont appFont = this->font();  // Get the default font
    appFont.setPointSize(10);    // Set font size to 14 (or any desired size)
    this->setFont(appFont);
    ui->setupUi(this);
    adcOptions = new QStringList();
    *adcOptions
        <<""
        <<"Int"
        <<"Ext"
        ;
    ui->adcComb->addItems(*adcOptions);

    resolutionOptions = new QStringList();
    *resolutionOptions
        <<""
        <<"16"
        <<"14"
        <<"12"
        <<"10"
        ;
    ui->resolutionComb->addItems(*resolutionOptions);

    /* Set default Value for ADC Clock Div Comb*/
    clockDivOptions = new QStringList();
    *clockDivOptions
        <<""
        <<"1"
        <<"2"
        <<"4"
        <<"8"
        <<"16"
        <<"32"
        <<"64"
        <<"128"
        <<"256"
        ;
    ui->clockDivComb->addItems(*clockDivOptions);

    /* Set default Value for ADC Sample Time Comb*/
    sampleTimeOptions = new QStringList();
    *sampleTimeOptions
        <<""
        <<"1C5"
        <<"2C5"
        <<"8C5"
        <<"16C5"
        <<"32C5"
        <<"64C5"
        <<"387C5"
        <<"810C5"
        ;
    ui->sampleTimeComb->addItems(*sampleTimeOptions);

    /* Set default Value for ADC Averaging Options Comb*/
    averaginOptions = new QStringList();
    *averaginOptions
        <<""
        <<"1"
        <<"2"
        <<"4"
        <<"8"
        <<"16"
        <<"32"
        <<"64"
        <<"128"
        <<"256"
        <<"512"
        <<"1024"
        ;

    networkInterfacesNames = new QStringList();
    *networkInterfacesNames << "";

    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

    foreach(QNetworkInterface a, interfaces)
    {
        QList<QNetworkAddressEntry> allEntries = a.addressEntries();
        if(a.flags() & QNetworkInterface::IsUp){
            qDebug() << "Interface Name:" << a.name();
            QNetworkAddressEntry entry;
            foreach (entry, allEntries) {
                if( entry.ip().protocol() == QAbstractSocket::IPv4Protocol){
                    *networkInterfacesNames <<   "<" + a.name() + ">:" + entry.ip().toString();
                }
            }
        }
    }

    ui->streamServerInterfComb->addItems(*networkInterfacesNames);

    advanceConfigurationWnd  = new AdvanceConfigurationWnd();
    advanceConfigurationWnd->hide();
    //Prethodno se lista kreira dinamicki
    advanceConfigurationWnd->assignAdcOptionsList(adcOptions);
    advanceConfigurationWnd->assignResolutionOptionsList(resolutionOptions);
    advanceConfigurationWnd->assignClockDivOptionsList(clockDivOptions);
    advanceConfigurationWnd->assignSampleTimeOptionsList(sampleTimeOptions);
    advanceConfigurationWnd->assignAvgRatioOptionsList(averaginOptions);

    connect(ui->advanceOptionPusb, SIGNAL(clicked(bool)), this, SLOT(onAdvanceConfigurationButtonPressed(bool)));

    voltageChart             = new Plot(PLOT_MINIMUM_SIZE_WIDTH/2, PLOT_MINIMUM_SIZE_HEIGHT);
    voltageChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    voltageChart->setTitle("Voltage");
    voltageChart->setYLabel("[V]");
    voltageChart->setXLabel("[ms]");
    /* Make plot for consumption presentation */
    currentChart         = new Plot(PLOT_MINIMUM_SIZE_WIDTH/2, PLOT_MINIMUM_SIZE_HEIGHT);
    currentChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    currentChart->setTitle("Current");
    currentChart->setYLabel("[mA]");
    currentChart->setXLabel("[ms]");


    consumptionChart         = new Plot(PLOT_MINIMUM_SIZE_WIDTH, PLOT_MINIMUM_SIZE_HEIGHT);
    consumptionChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    consumptionChart->setTitle("Consumption");
    consumptionChart->setYLabel("[mAh]");
    consumptionChart->setXLabel("[ms]");

    consumptionChart->scatterAddGraph();

    ui->maxNumOfPacketsLine->setText(QString::number(DEVICEWND_DEFAULT_MAX_NUMBER_OF_BUFFERS));
    ui->statisticsPacketCounterLabe2->setText(QString::number(0));
    ui->statisticsDropRateProb->setValue(0);
    ui->statisticsSamplingPeriodLabe2->setText(QString::number(0));
    ui->samplingPeriodLine->setText(QString::number(0));

    /*Group consumption type selecrion radio buttons*/
    consumptionTypeSelection = new QButtonGroup();
    consumptionTypeSelection->addButton(ui->currentRadb);
    consumptionTypeSelection->addButton(ui->cumulativeRadb);
    consumptionTypeSelection->setId(ui->currentRadb, 1);
    consumptionTypeSelection->setId(ui->cumulativeRadb, 2);
    measurementTypeSelection = new QButtonGroup();
    measurementTypeSelection->addButton(ui->measurementTypeCurrentRadb);
    measurementTypeSelection->addButton(ui->measurementTypeVoltageRadb);
    measurementTypeSelection->setId(ui->measurementTypeVoltageRadb, 1);
    measurementTypeSelection->setId(ui->measurementTypeCurrentRadb, 2);

    setStatisticsElapsedTime(0);

    ui->currentRadb->setChecked(true);
    setConsumptionType(DEVICE_CONSUMPTION_TYPE_CURRENT);
    ui->measurementTypeVoltageRadb->setChecked(true);
    setMeasurementType(DEVICE_MEASUREMENT_TYPE_VOLTAGE);

    setDeviceState(DEVICE_STATE_UNDEFINED);

    consoleWnd  = new ConsoleWnd();

    ui->GraphicsTopHorl->addWidget(voltageChart);
    ui->GraphicsTopHorl->addWidget(currentChart);
    ui->GraphicsBottomVerl->addWidget(consumptionChart, Qt::AlignCenter);
    setDeviceInterfaceSelectionState(DEVICE_INTERFACE_SELECTION_STATE_UNDEFINED);

    connect(ui->saveToFileCheb, SIGNAL(stateChanged(int)), this, SLOT(onSaveToFileChanged(int)));
    connect(ui->pathPusb, SIGNAL(clicked(bool)), this, SLOT(onPathInfo()));
    connect(ui->startPusb, SIGNAL(clicked(bool)), this, SLOT(onStartAcquisition()));
    connect(ui->pausePusb, SIGNAL(clicked(bool)), this, SLOT(onPauseAcquisition()));
    connect(ui->stopPusb, SIGNAL(clicked(bool)), this, SLOT(onStopAcquisiton()));
    connect(ui->refreshPusb, SIGNAL(clicked(bool)), this, SLOT(onRefreshAcquisiton()));
    connect(ui->ConsolePusb, SIGNAL(clicked(bool)), this, SLOT(onConsolePressed()));

    connect(ui->clockDivComb,           SIGNAL(currentTextChanged(QString)),        this, SLOT(onClockDivChanged(QString)));
    connect(ui->sampleTimeComb,         SIGNAL(currentTextChanged(QString)),        this, SLOT(onSampleTimeChanged(QString)));
    connect(ui->resolutionComb,         SIGNAL(currentTextChanged(QString)),        this, SLOT(onResolutionChanged(QString)));
    connect(ui->samplingPeriodLine,     SIGNAL(returnPressed()),                    this, SLOT(onSamplingPeriodChanged()));
    connect(ui->adcComb,                SIGNAL(currentTextChanged(QString)),        this, SLOT(onADCChanged(QString)));
    connect(ui->streamServerInterfComb, SIGNAL(currentTextChanged(QString)),        this, SLOT(onInterfaceChanged(QString)));
    connect(ui->maxNumOfPacketsLine,    SIGNAL(editingFinished()),                  this, SLOT(onMaxNumberOfBuffersChanged()));
    connect(consumptionTypeSelection,   SIGNAL(buttonClicked(QAbstractButton*)),    this, SLOT(onConsumptionTypeChanged(QAbstractButton*)));
    connect(measurementTypeSelection,   SIGNAL(buttonClicked(QAbstractButton*)),    this, SLOT(onMeasurementTypeChanged(QAbstractButton*)));


    connect(advanceConfigurationWnd, SIGNAL(sigAdvConfigurationChanged(QVariant)), this, SLOT(onAdvConfigurationChanged(QVariant)));
    connect(advanceConfigurationWnd, SIGNAL(sigAdvConfigurationRequested()), this, SLOT(onAdvConfigurationReqested()));


    connect(consoleWnd, SIGNAL(sigControlMsgSend(QString)), this, SLOT(onNewControlMsgRcvd(QString)));
}

void    DeviceWnd::onNewControlMsgRcvd(QString text)
{
    /* emit signal to deviceContrainer -> */
    emit sigNewControlMessageRcvd(text);
}

void DeviceWnd::onPathInfo()
{
    QString selfilter = tr("CSV files (*.csv)" );
    QString chosenPath = QFileDialog::getSaveFileName(
        this,
        "Select directory to save files",
        QDir::homePath(),
        selfilter,
        &selfilter);
    ui->pathLine->setText(chosenPath);
    qDebug() << chosenPath;
    emit sigPathChanged(chosenPath);
}

void DeviceWnd::onInterfaceChanged(QString interfaceInfo)
{
    QString ip;
    QStringList interfaceInfoParts;
    interfaceInfoParts = interfaceInfo.split(":");
    ip = interfaceInfoParts[1];
    emit sigNewInterfaceSelected(ip);
}

void DeviceWnd::onAdvConfigurationChanged(QVariant aConfig)
{
    emit sigAdvConfigurationChanged(aConfig);
}

void DeviceWnd::onAdvConfigurationReqested(void)
{
    emit sigAdvConfigurationReqested();
}

void DeviceWnd::onMaxNumberOfBuffersChanged()
{
    QString maxNumberOfSamplesBuffers = ui->maxNumOfPacketsLine->text();
    emit sigMaxNumberOfBuffersChanged(maxNumberOfSamplesBuffers.toInt());

}

void DeviceWnd::onConsumptionTypeChanged(QAbstractButton* button)
{
    int id = consumptionTypeSelection->id(button);
    switch(id)
    {
    case 1:
        emit sigConsumptionTypeChanged("Current");
        setConsumptionType(DEVICE_CONSUMPTION_TYPE_CURRENT);
        break;
    case 2:
        emit sigConsumptionTypeChanged("Cumulative");
        setConsumptionType(DEVICE_CONSUMPTION_TYPE_CUMULATIVE);
        break;
    default:
        emit sigConsumptionTypeChanged("Undef");
        setConsumptionType(DEVICE_CONSUMPTION_TYPE_UNDEF);
        break;
    }
}

void DeviceWnd::onMeasurementTypeChanged(QAbstractButton *button)
{
    int id = measurementTypeSelection->id(button);
    switch(id)
    {
    case 1:
        emit sigMeasurementTypeChanged("Voltage");
        setMeasurementType(DEVICE_MEASUREMENT_TYPE_VOLTAGE);
        break;
    case 2:
        emit sigMeasurementTypeChanged("Current");
        setMeasurementType(DEVICE_MEASUREMENT_TYPE_CURRENT);
        break;
    default:
        emit sigMeasurementTypeChanged("Undef");
        setMeasurementType(DEVICE_MEASUREMENT_TYPE_UNDEF);
        break;
    }
}

void DeviceWnd::onResolutionChanged(QString resolution)
{
    advanceConfigurationWnd->setResolution(resolution);
    emit sigResolutionChanged(ui->resolutionComb->currentText());
}

void DeviceWnd::onADCChanged(QString adc)
{
    emit sigADCChanged(ui->adcComb->currentText());
}

void DeviceWnd::onClockDivChanged(QString aClockDiv)
{
    advanceConfigurationWnd->setClockDiv(aClockDiv);
    emit sigClockDivChanged(ui->clockDivComb->currentText());
}

void DeviceWnd::onSampleTimeChanged(QString aSTime)
{
    advanceConfigurationWnd->setChSampleTime(aSTime);
    emit sigSampleTimeChanged(ui->sampleTimeComb->currentText());
}

void DeviceWnd::onSamplingPeriodChanged()
{
    QString time = ui->samplingPeriodLine->text();
    advanceConfigurationWnd->setSamplingTime(time);
    ui->statisticsSamplingPeriodLabe2->setText(time);
    emit sigSamplingPeriodChanged(time);
}


void    DeviceWnd::onAdvanceConfigurationButtonPressed(bool pressed)
{
    advanceConfigurationWnd->show();
}

void    DeviceWnd::onSaveToFileChanged(int value)
{
    if(value == Qt::Checked)
    {
        ui->pathPusb->setEnabled(true);
        ui->pathLab->setEnabled(true);
        ui->pathLine->setEnabled(true);
        emit saveToFileEnabled(true);
    }else
    {
        ui->pathPusb->setEnabled(false);
        ui->pathLab->setEnabled(false);
        ui->pathLine->setEnabled(false);
        emit saveToFileEnabled(false);
    }
}

void DeviceWnd::onConsolePressed()
{
    consoleWnd->show();
}

void DeviceWnd::onStartAcquisition()
{
    emit sigStartAcquisition();
}

void DeviceWnd::onPauseAcquisition()
{
    emit sigPauseAcquisition();
}

void DeviceWnd::onStopAcquisiton()
{
    voltageChart->clear();
    currentChart->clear();
    consumptionChart->clear();
    emit sigStopAcquisition();
}

void DeviceWnd::onRefreshAcquisiton()
{
    emit sigRefreshAcquisition();
}

void DeviceWnd::setDeviceStateDisconnected()
{
    ui->startPusb->setEnabled(false);
    ui->stopPusb->setEnabled(false);
    ui->pausePusb->setEnabled(false);
    ui->refreshPusb->setEnabled(false);
    ui->dischargeControlPusb1->setEnabled(false);
    ui->dischargeControlPusb2->setEnabled(false);
    ui->deviceConnectedLabe->setText("Disconnected");
    ui->deviceConnectedLabe->setStyleSheet("QLabel { background-color : red; color : black; }");
}

void DeviceWnd::setDeviceStateConnected()
{
    ui->startPusb->setEnabled(true);
    ui->stopPusb->setEnabled(true);
    ui->pausePusb->setEnabled(true);
    ui->refreshPusb->setEnabled(true);
    ui->dischargeControlPusb1->setEnabled(true);
    ui->dischargeControlPusb2->setEnabled(true);
    ui->deviceConnectedLabe->setText("Connected");
    ui->deviceConnectedLabe->setStyleSheet("QLabel { background-color : green; color : black; }");
}
void    DeviceWnd::closeEvent(QCloseEvent *event)
{
    emit sigWndClosed();
}

DeviceWnd::~DeviceWnd()
{
    delete ui;
}

QPlainTextEdit *DeviceWnd::getLogWidget()
{
    return ui->loggingQpte;
}

void DeviceWnd::setDeviceState(device_state_t aDeviceState)
{
    deviceState = aDeviceState;
    switch(deviceState)
    {
    case DEVICE_STATE_UNDEFINED:
        setDeviceStateDisconnected();
        break;
    case DEVICE_STATE_CONNECTED:
        setDeviceStateConnected();
        break;
    case DEVICE_STATE_DISCONNECTED:
        setDeviceStateDisconnected();
        break;
    }
}

void DeviceWnd::printConsoleMsg(QString msg, bool exeStatus)
{
    /* call consoleWnd print Message to display recieved msg form FW <- */
    consoleWnd->printMessage(msg, exeStatus);
}

void DeviceWnd::setDeviceInterfaceSelectionState(device_interface_selection_state_t selectionState)
{
    switch(selectionState)
    {
    case DEVICE_INTERFACE_SELECTION_STATE_UNDEFINED:
        ui->adcComb->setEnabled(false);
        ui->samplingPeriodLine->setEnabled(false);
        ui->resolutionComb->setEnabled(false);
        ui->clockDivComb->setEnabled(false);
        ui->sampleTimeComb->setEnabled(false);
        ui->advanceOptionPusb->setEnabled(false);
        ui->maxNumOfPacketsLine->setEnabled(false);
        ui->saveToFileCheb->setEnabled(false);
        ui->dischargeControlPusb1->setEnabled(false);
        ui->dischargeControlPusb2->setEnabled(false);
        ui->pathPusb->setEnabled(false);
        ui->streamServerInterfComb->setEnabled(true);
        break;
    case DEVICE_INTERFACE_SELECTION_STATE_SELECTED:
        ui->adcComb->setEnabled(true);
        ui->samplingPeriodLine->setEnabled(true);
        ui->resolutionComb->setEnabled(true);
        ui->clockDivComb->setEnabled(true);
        ui->sampleTimeComb->setEnabled(true);
        ui->advanceOptionPusb->setEnabled(true);
        ui->maxNumOfPacketsLine->setEnabled(true);
        ui->saveToFileCheb->setEnabled(true);
        ui->dischargeControlPusb1->setEnabled(true);
        ui->dischargeControlPusb2->setEnabled(true);
        ui->pathPusb->setEnabled(true);
        ui->streamServerInterfComb->setEnabled(false);
        break;
    }
    interfaceState = selectionState;
}

void DeviceWnd::setDeviceMode(device_mode_t mode)
{
    switch(mode)
    {
    case DEVICE_MODE_EXTERNAL:
//        ui->sampleTimeComb->setEnabled(false);
//        ui->clockDivComb->setEnabled(false);
//        ui->resolutionComb->setEnabled(false);
        break;
    case DEVICE_MODE_INTERNAL:
//        ui->sampleTimeComb->setEnabled(true);
//        ui->clockDivComb->setEnabled(true);
//        ui->resolutionComb->setEnabled(true);
        break;
    }
}

bool DeviceWnd::setAdc(QString adc)
{
    if(!adcOptions->contains(adc)) return false;
    //if(!advanceConfigurationWnd->setChSampleTime(sTime)) return false;

    ui->adcComb->blockSignals(true);
    ui->adcComb->setCurrentIndex(adcOptions->indexOf(adc));
    ui->adcComb->blockSignals(false);
    if(adc == "Ext")
    {
        setDeviceMode(DEVICE_MODE_EXTERNAL);
        advanceConfigurationWnd->setDeviceMode(ADVCONFIG_ADC_MODE_EXTERNAL);
    }
    else
    {
        setDeviceMode(DEVICE_MODE_INTERNAL);
        advanceConfigurationWnd->setDeviceMode(ADVCONFIG_ADC_MODE_INTERNAL);
    }
}

QStringList *DeviceWnd::getChSamplingTimeOptions()
{
    return sampleTimeOptions;
}

QStringList *DeviceWnd::getChAvgRationOptions()
{
    return averaginOptions;
}

QStringList *DeviceWnd::getClockDivOptions()
{
    return clockDivOptions;
}

QStringList *DeviceWnd::getResolutionOptions()
{
    return resolutionOptions;
}

QStringList *DeviceWnd::getADCOptions()
{
    return adcOptions;
}

bool DeviceWnd::setChSamplingTime(QString sTime)
{
    if(!sampleTimeOptions->contains(sTime)) return false;
    if(!advanceConfigurationWnd->setChSampleTime(sTime)) return false;

    ui->sampleTimeComb->blockSignals(true);
    ui->sampleTimeComb->setCurrentIndex(sampleTimeOptions->indexOf(sTime));
    ui->sampleTimeComb->blockSignals(false);
    return true;
}

bool DeviceWnd::setChAvgRatio(QString avgRatio)
{
    if(!averaginOptions->contains(avgRatio)) return false;
    if(!advanceConfigurationWnd->setAvgRatio(avgRatio)) return false;
    return true;
}

bool DeviceWnd::setClkDiv(QString clkDiv)
{
    if(!clockDivOptions->contains(clkDiv)) return false;
    if(!advanceConfigurationWnd->setClockDiv(clkDiv)) return false;

    ui->clockDivComb->blockSignals(true);
    ui->clockDivComb->setCurrentIndex(clockDivOptions->indexOf(clkDiv));
    ui->clockDivComb->blockSignals(false);

    return true;
}

bool DeviceWnd::setResolution(QString resolution)
{
    if(!resolutionOptions->contains(resolution)) return false;
    if(!advanceConfigurationWnd->setResolution(resolution)) return false;

    ui->resolutionComb->blockSignals(true);
    ui->resolutionComb->setCurrentIndex(resolutionOptions->indexOf(resolution));
    ui->resolutionComb->blockSignals(false);
    return true;
}

bool DeviceWnd::setSamplingPeriod(QString stime)
{
    if(!advanceConfigurationWnd->setSamplingTime(stime)) return false;
    ui->samplingPeriodLine->setText(stime);
    ui->statisticsSamplingPeriodLabe2->setText(stime);
    return true;
}

bool DeviceWnd::setInCkl(QString inClk)
{
    advanceConfigurationWnd->setADCInClk(inClk);
    return true;
}

bool DeviceWnd::setCOffset(QString coffset)
{
    advanceConfigurationWnd->setCOffset(coffset);
    return true;
}

bool DeviceWnd::setVOffset(QString voffset)
{
    advanceConfigurationWnd->setVOffset(voffset);
    return true;
}

void DeviceWnd::setStatisticsData(double dropRate, unsigned int dropPacketsNo, unsigned int fullReceivedBuffersNo, unsigned int lastBufferID)
{
    ui->statisticsPacketCounterLabe2->setText(QString::number(fullReceivedBuffersNo));
    ui->statisticsDropRateNoLabe->setText(QString::number(dropPacketsNo));
    ui->statisticsDropRateProb->setValue(dropRate);
}

void DeviceWnd::setStatisticsSamplingTime(double stime)
{
    ui->statisticsSamplingTimeLabe2->setText(QString::number(stime, 'f', 6));
}

void DeviceWnd::setStatisticsElapsedTime(int elapsedTime)
{
    int hours = elapsedTime / 3600;
    int minutes = (elapsedTime % 3600) / 60;
    int seconds = elapsedTime % 60;

    ui->statisticsAcquisitionDurationValueLabe->setText(QString("%1:%2:%3")
                                                            .arg(hours, 2, 10, QChar('0'))
                                                            .arg(minutes, 2, 10, QChar('0'))
                                                            .arg(seconds, 2, 10, QChar('0')));
}

void DeviceWnd::setConsumptionType(device_consumption_type_t actype)
{
    cType = actype;
    QRadioButton *button = qobject_cast<QRadioButton*>(measurementTypeSelection->button(1));
    switch(cType)
    {
    case DEVICE_CONSUMPTION_TYPE_CURRENT:
        consumptionChart->setTitle("Spectrum");
        consumptionChart->setXLabel("Frequency [Hz]");
        for (QAbstractButton* button : measurementTypeSelection->buttons())
        {
            button->setDisabled(false);
        }

        button->setChecked(true);
        emit sigMeasurementTypeChanged("Voltage");
        setMeasurementType(DEVICE_MEASUREMENT_TYPE_VOLTAGE);
        break;
    case DEVICE_CONSUMPTION_TYPE_CUMULATIVE:
        consumptionChart->setTitle("Consumption");
        consumptionChart->setXLabel("[ms]");
        voltageChart->setTitle("Voltage");
        voltageChart->setYLabel("[V]");
        voltageChart->setXLabel("[ms]");
        currentChart->setTitle("Current");
        currentChart->setYLabel("[mA]");
        currentChart->setXLabel("[ms]");
        for (QAbstractButton* button : measurementTypeSelection->buttons())
        {
            button->setDisabled(true);
        }
        break;
    case DEVICE_CONSUMPTION_TYPE_UNDEF:
        consumptionChart->setTitle("-");
        consumptionChart->setXLabel("-");
        break;
    }
}

void DeviceWnd::setMeasurementType(device_measurement_type_t amtype)
{
    mType = amtype;
    switch(mType)
    {
    case DEVICE_MEASUREMENT_TYPE_CURRENT:
        voltageChart->setTitle("Current");
        voltageChart->setYLabel("[mA]");
        voltageChart->setXLabel("[ms]");
        currentChart->setTitle("Current filtered");
        currentChart->setYLabel("[mA]");
        currentChart->setXLabel("[ms]");
        break;
    case DEVICE_MEASUREMENT_TYPE_VOLTAGE:
        voltageChart->setTitle("Voltage");
        voltageChart->setYLabel("[V]");
        voltageChart->setXLabel("[ms]");
        currentChart->setTitle("Voltage filtered");
        currentChart->setYLabel("[V]");
        currentChart->setXLabel("[ms]");
        break;
    case DEVICE_MEASUREMENT_TYPE_UNDEF:
        voltageChart->setTitle("-");
        currentChart->setTitle("-");
        break;
    }
}

bool DeviceWnd::plotVoltageValues(QVector<double> values, QVector<double> keys)
{
    switch(cType)
    {
    case DEVICE_CONSUMPTION_TYPE_CURRENT:
        voltageChart->setData(values, keys);
        break;
    case DEVICE_CONSUMPTION_TYPE_CUMULATIVE:
        voltageChart->appendData(values, keys);
        break;
    case DEVICE_CONSUMPTION_TYPE_UNDEF:
        break;
    }
    return true;
}

bool DeviceWnd::plotCurrentValues(QVector<double> values, QVector<double> keys)
{
    switch(cType)
    {
    case DEVICE_CONSUMPTION_TYPE_CURRENT:
        currentChart->setData(values, keys);
        break;
    case DEVICE_CONSUMPTION_TYPE_CUMULATIVE:
        currentChart->appendData(values, keys);
        break;
    case DEVICE_CONSUMPTION_TYPE_UNDEF:
        break;
    }
    return true;
}

bool DeviceWnd::plotConsumptionValues(QVector<double> values, QVector<double> keys)
{
    switch(cType)
    {
    case DEVICE_CONSUMPTION_TYPE_CURRENT:
        consumptionChart->setData(values, keys);
        break;
    case DEVICE_CONSUMPTION_TYPE_CUMULATIVE:
        consumptionChart->appendData(values, keys);
        break;
    case DEVICE_CONSUMPTION_TYPE_UNDEF:
        break;
    }
    return true;
}

bool DeviceWnd::plotConsumptionEBP(QVector<double> values, QVector<double> keys)
{
    if(values.size() > 0)
    {
        consumptionChart->scatterAddData(values, keys);
    }
    return true;
}

bool DeviceWnd::plotConsumptionEBPWithName(double value, double key, QString name)
{
    consumptionChart->scatterAddDataWithName(value, key, name);
    return true;
}
