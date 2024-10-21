#include <QSpacerItem>
#include "advanceconfigurationwnd.h"
#include "ui_advanceconfigurationwnd.h"

#define LINEDIT_WIDTH                       50
#define LABEL_WIDTH                         220
#define ADVANCEWNDCONFIG_ADC_INPUT_CLOCK    100
#define ADVANCEWNDCONFIG_ADC_CLOCK          100
#define ADVANCEWNDCONFIG_ADC_CLOCK          100
#define ADVANCEWNDCONFIG_STREAM_SIZE        100
#define ADVANCEWNDCONFIG_VOLTAGE_OFFSET     100
#define ADVANCEWNDCONFIG_CURRENT_OFFSET     100
#define ADVANCEWNDCONFIG_SAMPLING_TIME      100
#define ADVANCEWNDCONFIG_DEVICE_PORT        50000

AdvanceConfigurationWnd::AdvanceConfigurationWnd(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AdvanceConfigurationWnd)
{
    ui->setupUi(this);

    QStringList selectProfileOptions=(
        QStringList()<<
                        ""<<
                        "Static"<<
                        "Ramp"
                        );
    ui->selectProfileComb->addItems(selectProfileOptions);
    connect(ui->selectProfileComb, SIGNAL(currentIndexChanged(int)), this, SLOT(onLoadProfileChanged(int)));

    /* Set default Value for ADC Input Clock Line*/
    ui->adcInputClockLine->setText(QString::number(ADVANCEWNDCONFIG_ADC_INPUT_CLOCK));

    /* Set default Value for ADC Clock Line*/
    ui->adcClockLine->setText(QString::number(ADVANCEWNDCONFIG_ADC_CLOCK));

    /* Set default Value for Stream size Line*/
    ui->streamSizeLine->setText(QString::number(ADVANCEWNDCONFIG_STREAM_SIZE));

    /* Set default Value for Voltage Offset Line*/
    ui->voltageOffsetLine->setText(QString::number(ADVANCEWNDCONFIG_VOLTAGE_OFFSET));

    /* Set default Value for Current Offset Line*/
    ui->currentOffsetLine->setText(QString::number(ADVANCEWNDCONFIG_CURRENT_OFFSET));

    /* Set default Value for Stream size Line*/
    ui->deviceIpAddressLine->setText("192.168.1.2");

    /* Set default Value for Voltage Offset Line*/
    ui->devicePortLine->setText(QString::number(ADVANCEWNDCONFIG_DEVICE_PORT));

    /* Set default Value for Current Offset Line*/
    ui->deviceNameLine->setText("ADevice 1");

    ui->samplingPeriodLine->setText(QString::number(ADVANCEWNDCONFIG_SAMPLING_TIME));

    /* Uknown profile layout */
    QLabel*  uknownLabel = new QLabel();
    uknownLabel->setText("Select profile");
    uknownLabel->setAlignment(Qt::AlignCenter);
    uknownLabel->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);

    uknownProfileLayout = new QHBoxLayout();
    uknownProfileLayout->addWidget(uknownLabel);

    uknownProfileWidg   = new QWidget();
    uknownProfileWidg->setLayout(uknownProfileLayout);

    /* Static profile layout */
    /**/
    QLabel*  averageCurrentLabel = new QLabel();
    averageCurrentLabel->setText("Average current [mA]:");
    averageCurrentLabel->setAlignment(Qt::AlignLeft);
    averageCurrentLabel->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    averageCurrentLabel->setFixedWidth(LABEL_WIDTH);

    staticProfileAverageCurrentLine  = new QLineEdit();
    staticProfileAverageCurrentLine->setFixedWidth(LINEDIT_WIDTH);
    staticProfileAverageCurrentLine->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    staticProfileAverageCurrentLine->setText(QString::number(100));

    QLabel*  averageCurrentDevLabel = new QLabel();
    averageCurrentDevLabel->setText("Average current dev [%]:");
    averageCurrentDevLabel->setAlignment(Qt::AlignLeft);
    averageCurrentDevLabel->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    averageCurrentDevLabel->setFixedWidth(LABEL_WIDTH);

    staticProfileAverageCurrentDeviationLine = new QLineEdit();
    staticProfileAverageCurrentDeviationLine->setFixedWidth(LINEDIT_WIDTH);
    staticProfileAverageCurrentDeviationLine->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    staticProfileAverageCurrentDeviationLine->setText(QString::number(10));



    QHBoxLayout *averageCurrentHorl = new QHBoxLayout();
    averageCurrentHorl->addWidget(averageCurrentLabel);
    averageCurrentHorl->addWidget(staticProfileAverageCurrentLine);
    averageCurrentHorl->addWidget(averageCurrentDevLabel);
    averageCurrentHorl->addWidget(staticProfileAverageCurrentDeviationLine);
    averageCurrentHorl->addStretch();


    /**/
    QLabel*  averageCurrentChangePeriodLabe = new QLabel();
    averageCurrentChangePeriodLabe->setText("Average current change period [ms]:");
    averageCurrentChangePeriodLabe->setAlignment(Qt::AlignLeft);
    averageCurrentChangePeriodLabe->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    averageCurrentChangePeriodLabe->setFixedWidth(LABEL_WIDTH);

    staticProfileAverageCurrentChangePeriodLine  = new QLineEdit();
    staticProfileAverageCurrentChangePeriodLine->setFixedWidth(LINEDIT_WIDTH);
    staticProfileAverageCurrentChangePeriodLine->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    staticProfileAverageCurrentChangePeriodLine->setText(QString::number(100));

    QLabel*  AverageCurrentDeviationLabe = new QLabel();
    AverageCurrentDeviationLabe->setText("Average current change period dev [%]:");
    AverageCurrentDeviationLabe->setAlignment(Qt::AlignLeft);
    AverageCurrentDeviationLabe->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    AverageCurrentDeviationLabe->setFixedWidth(LABEL_WIDTH);

    staticProfileAverageCurrentDeviationLine = new QLineEdit();
    staticProfileAverageCurrentDeviationLine->setFixedWidth(LINEDIT_WIDTH);
    staticProfileAverageCurrentDeviationLine->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    staticProfileAverageCurrentDeviationLine->setText(QString::number(10));


    QHBoxLayout *averageCurrentChangePeriodHorl = new QHBoxLayout();
    averageCurrentChangePeriodHorl->addWidget(averageCurrentChangePeriodLabe);
    averageCurrentChangePeriodHorl->addWidget(staticProfileAverageCurrentChangePeriodLine);
    averageCurrentChangePeriodHorl->addWidget(AverageCurrentDeviationLabe);
    averageCurrentChangePeriodHorl->addWidget(staticProfileAverageCurrentDeviationLine);
    averageCurrentChangePeriodHorl->addStretch();

    /**/
    QLabel*  staticProfileDurationLabe = new QLabel();
    staticProfileDurationLabe->setText("Profile duration [ms]:");
    staticProfileDurationLabe->setAlignment(Qt::AlignLeft);
    staticProfileDurationLabe->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    staticProfileDurationLabe->setFixedWidth(LABEL_WIDTH);

    staticProfileDurationLine  = new QLineEdit();
    staticProfileDurationLine->setFixedWidth(LINEDIT_WIDTH);
    staticProfileDurationLine->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    staticProfileDurationLine->setText(QString::number(100));

    QLabel*  staticProfileDurationDevLabe = new QLabel();
    staticProfileDurationDevLabe->setText("Profile duration dev [%]:");
    staticProfileDurationDevLabe->setAlignment(Qt::AlignLeft);
    staticProfileDurationDevLabe->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    staticProfileDurationDevLabe->setFixedWidth(LABEL_WIDTH);

    staticProfileDurationDevLine = new QLineEdit();
    staticProfileDurationDevLine->setFixedWidth(LINEDIT_WIDTH);
    staticProfileDurationDevLine->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    staticProfileDurationDevLine->setText(QString::number(10));


    QHBoxLayout *staticProfileDurationHorl = new QHBoxLayout();
    staticProfileDurationHorl->addWidget(staticProfileDurationLabe);
    staticProfileDurationHorl->addWidget(staticProfileDurationLine);
    staticProfileDurationHorl->addWidget(staticProfileDurationDevLabe);
    staticProfileDurationHorl->addWidget(staticProfileDurationDevLine);
    staticProfileDurationHorl->addStretch();

    staticProfileLayout = new QVBoxLayout();
    staticProfileLayout->addLayout(averageCurrentHorl);
    staticProfileLayout->addLayout(averageCurrentChangePeriodHorl);
    staticProfileLayout->addLayout(staticProfileDurationHorl);

    staticProfileWidg   = new QWidget();
    staticProfileWidg->setLayout(staticProfileLayout);

    SetProfileTypeUknown();
    activeLoadProfileType = LOAD_PROFILE_TYPE_UNKNOW;

    ui->optionsLabe->setText("Plase update configuration");

    //SetTextRed(ui->averagingRatioComb, ui->voltageOffsetLine, ui->currentOffsetLine, ui->samplingTimeLine);

    connect(ui->adcResolutionComb,  SIGNAL(currentIndexChanged(int)),   this, SLOT(onAdvResolutionChanged(int)));
    connect(ui->adcClockDivComb,    SIGNAL(currentIndexChanged(int)),   this, SLOT(onAdvClkDivChanged(int)));
    connect(ui->adcSampleTimeComb,  SIGNAL(currentIndexChanged(int)),   this, SLOT(onAdvSampleTimeChanged(int)));
    connect(ui->averagingRatioComb, SIGNAL(currentIndexChanged(int)),   this, SLOT(onAdvAvrRatioChanged(int)));

    connect(ui->samplingPeriodLine,   SIGNAL(textEdited(QString)),        this, SLOT(onAdvSamplingTimeChanged(QString)));
    connect(ui->voltageOffsetLine,  SIGNAL(textEdited(QString)),        this, SLOT(onAdvVOffsetChanged(QString)));
    connect(ui->currentOffsetLine,  SIGNAL(textEdited(QString)),        this, SLOT(onAdvCOffsetChanged(QString)));

    connect(ui->configurePusb,      SIGNAL(clicked(bool)),              this, SLOT(onAdvConfigurationPressed(void)));
    connect(ui->acquirePusb,        SIGNAL(clicked(bool)),              this, SLOT(onAdvConfigurationRequsted(void)));
}

void AdvanceConfigurationWnd::assignAdcOptionsList(QStringList *aList)
{
    adcOptions = aList;
    ui->adcComb->addItems(*aList);
    setDeviceMode(ADVCONFIG_ADC_MODE_INTERNAL);
}

bool AdvanceConfigurationWnd::setDeviceMode(advconfig_adc_mode_t aMode)
{
    ui->adcStack->setCurrentIndex(aMode);
    ui->adcComb->setCurrentIndex(aMode+1);
    return true;
}

void    AdvanceConfigurationWnd::SetProfileTypeStatic()
{
    staticProfileWidg->show();
    ui->selectProfileVerlProfileConfigurationHorl->addWidget(staticProfileWidg);
}

void    AdvanceConfigurationWnd::RemoveProfileTypeStatic()
{
    staticProfileWidg->hide();
    ui->selectProfileVerlProfileConfigurationHorl->removeWidget(staticProfileWidg);
}

void    AdvanceConfigurationWnd::SetProfileTypeRamp()
{

}
void    AdvanceConfigurationWnd::RemoveProfileTypeRamp()
{

}

void    AdvanceConfigurationWnd::SetProfileTypeUknown()
{
    uknownProfileWidg->show();
    ui->selectProfileVerlProfileConfigurationHorl->addWidget(uknownProfileWidg);
}
void    AdvanceConfigurationWnd::RemoveProfileTypeUknown()
{
    uknownProfileWidg->hide();
    ui->selectProfileVerlProfileConfigurationHorl->removeWidget(uknownProfileWidg);
}
void    AdvanceConfigurationWnd::RemoveProfileType(loadProfileType_t aProfileType)
{
    switch(aProfileType)
    {
    case LOAD_PROFILE_TYPE_UNKNOW:
        RemoveProfileTypeUknown();
        break;
    case LOAD_PROFILE_TYPE_STATIC:
        RemoveProfileTypeStatic();
        break;
    case LOAD_PROFILE_TYPE_RAMP:
        RemoveProfileTypeRamp();
        break;
    }
    activeLoadProfileType = aProfileType;
}
void    AdvanceConfigurationWnd::SetProfileType(loadProfileType_t aProfileType)
{
    switch(aProfileType)
    {
    case LOAD_PROFILE_TYPE_UNKNOW:
        SetProfileTypeUknown();
        break;
    case LOAD_PROFILE_TYPE_STATIC:
        SetProfileTypeStatic();
        break;
    case LOAD_PROFILE_TYPE_RAMP:
        SetProfileTypeRamp();
        break;
    }
    activeLoadProfileType = aProfileType;
}

void    AdvanceConfigurationWnd::onLoadProfileChanged(int loadIndex)
{
    RemoveProfileType(activeLoadProfileType);
    switch(loadIndex)
    {
    case 0:
        SetProfileType(LOAD_PROFILE_TYPE_UNKNOW);
        break;
    case 1:
        SetProfileType(LOAD_PROFILE_TYPE_STATIC);
        break;
    case 2:
        SetProfileType(LOAD_PROFILE_TYPE_RAMP);
        break;
    default:
        break;
    }
}
AdvanceConfigurationWnd::~AdvanceConfigurationWnd()
{
    delete ui;
}

bool AdvanceConfigurationWnd::setADCInClk(QString aInClk)
{
    ui->adcInputClockLine->setText(aInClk);
    return true;
}

bool    AdvanceConfigurationWnd::setClockDiv(QString aClkDiv)
{
    // Check if entry exists
    int index = clockDivOptions->indexOf(aClkDiv);
    if( index < 0) return false;

    ui->adcClockDivComb->blockSignals(true);
    ui->adcClockDivComb->setCurrentIndex(index);
    ui->adcClockDivComb->blockSignals(false);

    // Set text color to black
    QPalette palette = ui->adcResolutionComb->palette();
    palette.setColor(QPalette::Text, Qt::black); // Set text color to black
    ui->adcClockDivComb->setPalette(palette);

    return true;
}

bool    AdvanceConfigurationWnd::setResolution(QString aResolution)
{

    // Check if entry exists
    int index = resolutionOptions->indexOf(aResolution);
    if( index < 0) return false;
    ui->adcResolutionComb->blockSignals(true);
    ui->adcResolutionComb->setCurrentIndex(index);
    ui->adcResolutionComb->blockSignals(false);

    // Set text color to black
    QPalette palette = ui->adcResolutionComb->palette();
    palette.setColor(QPalette::Text, Qt::black);
    ui->adcResolutionComb->setPalette(palette);

    return true;
}


bool    AdvanceConfigurationWnd::setChSampleTime(QString aSTime)
{
    // Check if entry exists
    int index = sampleTimeOptions->indexOf(aSTime);
    if( index < 0) return false;
    ui->adcSampleTimeComb->blockSignals(true);
    ui->adcSampleTimeComb->setCurrentIndex(index);
    ui->adcSampleTimeComb->blockSignals(false);

    // Set text color to black
    QPalette palette = ui->adcSampleTimeComb->palette();
    palette.setColor(QPalette::Text, Qt::black); // Set text color to black
    ui->adcSampleTimeComb->setPalette(palette);

    return true;
}



bool    AdvanceConfigurationWnd::setAvgRatio(QString aAvgRatio)
{
    // Check if entry exists
    int index = avgRatioOptions->indexOf(aAvgRatio);
    if( index < 0) return false;
    ui->averagingRatioComb->blockSignals(true);
    ui->averagingRatioComb->setCurrentIndex(index);
    ui->averagingRatioComb->blockSignals(false);

    // Set text color to black
    QPalette palette = ui->averagingRatioComb->palette();
    palette.setColor(QPalette::Text, Qt::black);
    ui->averagingRatioComb->setPalette(palette);

    return true;
}

bool    AdvanceConfigurationWnd::setVOffset(QString aVOffset)
{
    ui->voltageOffsetLine->setText(aVOffset);

    // Set text color to black
    QPalette palette = ui->voltageOffsetLine->palette();
    palette.setColor(QPalette::Text, Qt::black);
    ui->voltageOffsetLine->setPalette(palette);

    return true;
}

bool    AdvanceConfigurationWnd::setCOffset(QString aCOffset)
{
    ui->currentOffsetLine->setText(aCOffset);

    // Set text color to black
    QPalette palette = ui->currentOffsetLine->palette();
    palette.setColor(QPalette::Text, Qt::black);
    ui->currentOffsetLine->setPalette(palette);

    return true;
}

bool AdvanceConfigurationWnd::setSamplingTime(QString sTime)
{
    ui->samplingPeriodLine->setText(sTime);

    // Set text color to black
    QPalette palette = ui->samplingPeriodLine->palette();
    palette.setColor(QPalette::Text, Qt::black);
    ui->samplingPeriodLine->setPalette(palette);

    return true;
}

void    AdvanceConfigurationWnd::onAdvResolutionChanged(int index)
{
    QPalette palette = ui->adcResolutionComb->palette();
    palette.setColor(QPalette::Text, Qt::red); // Set text color to red
    ui->adcResolutionComb->setPalette(palette);
    LabelChangedNotUpdated();
}

void    AdvanceConfigurationWnd::onAdvClkDivChanged(int index)
{
    QPalette palette = ui->adcClockDivComb->palette();
    palette.setColor(QPalette::Text, Qt::red); // Set text color to red
    ui->adcClockDivComb->setPalette(palette);
    LabelChangedNotUpdated();
}

void    AdvanceConfigurationWnd::onAdvAvrRatioChanged(int index)
{
    QPalette palette = ui->averagingRatioComb->palette();
    palette.setColor(QPalette::Text, Qt::red); // Set text color to red
    ui->averagingRatioComb->setPalette(palette);
    LabelChangedNotUpdated();
}
void    AdvanceConfigurationWnd::onAdvSampleTimeChanged(int index)
{
    QPalette palette = ui->adcSampleTimeComb->palette();
    palette.setColor(QPalette::Text, Qt::red); // Set text color to red
    ui->adcSampleTimeComb->setPalette(palette);
    LabelChangedNotUpdated();
}

void    AdvanceConfigurationWnd::onAdvSamplingTimeChanged(QString time)
{
    QPalette palette = ui->samplingPeriodLine->palette();
    palette.setColor(QPalette::Text, Qt::red); // Set text color to red
    ui->samplingPeriodLine->setPalette(palette);
    LabelChangedNotUpdated();
}

void    AdvanceConfigurationWnd::onAdvVOffsetChanged(QString off)
{
    QPalette palette = ui->voltageOffsetLine->palette();
    palette.setColor(QPalette::Text, Qt::red); // Set text color to red
    ui->voltageOffsetLine->setPalette(palette);
    LabelChangedNotUpdated();
}

void    AdvanceConfigurationWnd::onAdvCOffsetChanged(QString off)
{
    QPalette palette = ui->currentOffsetLine->palette();
    palette.setColor(QPalette::Text, Qt::red); // Set text color to red
    ui->currentOffsetLine->setPalette(palette);
    LabelChangedNotUpdated();
}

void    AdvanceConfigurationWnd::onAdvConfigurationPressed(void)
{
    advConfigurationData cdata;
    QVariant data;
    QPalette palette;
    cdata.chSTime = ui->adcSampleTimeComb->currentText();

    /* Send data to device only if change happened */
    if(ui->adcResolutionComb->palette().color(QPalette::Text) == Qt::red)
    {
        cdata.resolution = ui->adcResolutionComb->currentText();
    }
    else
    {
        cdata.resolution = "";
    }
    if(ui->adcClockDivComb->palette().color(QPalette::Text) == Qt::red)
    {
        cdata.clockDiv = ui->adcClockDivComb->currentText();
    }
    else
    {
        cdata.clockDiv = "";
    }
    if(ui->adcSampleTimeComb->palette().color(QPalette::Text) == Qt::red)
    {
        cdata.chSTime = ui->adcSampleTimeComb->currentText();
    }
    else
    {
        cdata.chSTime = "";
    }
    if(ui->averagingRatioComb->palette().color(QPalette::Text) == Qt::red)
    {
        cdata.averaginRatio = ui->averagingRatioComb->currentText();
    }
    else
    {
        cdata.averaginRatio = "";
    }
    if(ui->samplingPeriodLine->palette().color(QPalette::Text) == Qt::red)
    {
        cdata.samplingTime = ui->samplingPeriodLine->text();
    }
    else
    {
        cdata.samplingTime = "";
    }
    if(ui->currentOffsetLine->palette().color(QPalette::Text) == Qt::red)
    {
        cdata.currentOffset = ui->currentOffsetLine->text();
    }
    else
    {
        cdata.currentOffset = "";
    }
    if(ui->voltageOffsetLine->palette().color(QPalette::Text) == Qt::red)
    {
        cdata.voltageOffset = ui->voltageOffsetLine->text();
    }
    else
    {
        cdata.voltageOffset = "";
    }
    data.setValue(cdata);
    LabelChangedUpdated();
    emit sigAdvConfigurationChanged(data);
}

void    AdvanceConfigurationWnd::onAdvConfigurationRequsted(void)
{
    emit sigAdvConfigurationRequested();
}

void     AdvanceConfigurationWnd::assignResolutionOptionsList(QStringList *aList)
{
    resolutionOptions = aList;
    ui->adcResolutionComb->addItems(*aList);
    ui->adcResolutionComb->setCurrentIndex(1);
}

void     AdvanceConfigurationWnd::assignClockDivOptionsList(QStringList *aList)
{
    clockDivOptions = aList;
    ui->adcClockDivComb->addItems(*aList);
    ui->adcClockDivComb->setCurrentIndex(1);
}

void     AdvanceConfigurationWnd::assignSampleTimeOptionsList(QStringList *aList)
{
    sampleTimeOptions = aList;
    ui->adcSampleTimeComb->addItems(*aList);
    ui->adcSampleTimeComb->setCurrentIndex(1);
}

void AdvanceConfigurationWnd::assignAvgRatioOptionsList(QStringList *aList)
{
    avgRatioOptions = aList;
    ui->averagingRatioComb->addItems(*aList);
    ui->averagingRatioComb->setCurrentIndex(1);
}



void    AdvanceConfigurationWnd::LabelChangedNotUpdated()
{
    ui->optionsLabe->setText("Configuration changed but not updated");
    QPalette palette = ui->optionsLabe->palette();
    palette.setColor(QPalette::Text, Qt::red); // Set text color to red
    ui->optionsLabe->setPalette(palette);
}

void    AdvanceConfigurationWnd::LabelChangedUpdated()
{
    QPalette palette = ui->optionsLabe->palette();
    palette.setColor(QPalette::Text, Qt::black); // Set text color to red
    ui->optionsLabe->setPalette(palette);
    ui->optionsLabe->setText("Configuration updated");
}
void    AdvanceConfigurationWnd::SetTextRed(QComboBox* cb, QLineEdit* le1, QLineEdit* le2, QLineEdit* le3)
{
    QPalette palette = cb->palette();
    palette.setColor(QPalette::Text, Qt::red); // Set text color to red
    cb->setPalette(palette);
    le1->setPalette(palette);
    le2->setPalette(palette);
    le3->setPalette(palette);
}
