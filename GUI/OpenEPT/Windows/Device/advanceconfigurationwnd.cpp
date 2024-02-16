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

    /* Set default Value for ADC Resolution Comb
    advResolutionOptions=(
        QStringList()
        <<""
        <<"16 Bit"
        <<"14 Bit"
        <<"12 Bit"
        <<"10 Bit"
        );
    ui->adcResolutionComb->addItems(advResolutionOptions);*/

    /* Set default Value for ADC Clock Div Comb
    advClockDivOptions=(
        QStringList()
        <<""
        <<"1"
        <<"2"
        <<"4"
        <<"6"
        <<"8"
        <<"10"
        <<"12"
        <<"16"
        <<"32"
        <<"64"
        <<"128"
        );
    ui->adcClockDivComb->addItems(advClockDivOptions);
*/
    /* Set default Value for ADC Sample Time Comb
    advSampleTimeOptions=(
        QStringList()
        <<""
        <<"1C5"
        <<"2C5"
        <<"8C5"
        <<"16C5"
        <<"32C5"
        <<"64C5"
        <<"387C5"
        <<"810C5"
        );
    ui->adcSampleTimeComb->addItems(advSampleTimeOptions);
*/
    /* Set default Value for Averaging Ratio Comb*/
    QStringList averagingRatioOptions=(
        QStringList()
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
        );
    ui->averagingRatioComb->addItems(averagingRatioOptions);

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
    connect(ui->adcResolutionComb, SIGNAL(currentIndexChanged(int)), this, SLOT(onAdvResolutionChanged(int)));
    connect(ui->adcClockDivComb, SIGNAL(currentIndexChanged(int)), this, SLOT(onAdvClkDivChanged(int)));
    connect(ui->adcSampleTimeComb, SIGNAL(currentIndexChanged(int)), this, SLOT(onAdvSampleTimeChanged(int)));
    connect(ui->configurePusb, SIGNAL(clicked(bool)), this, SLOT(onAdvConfigurationPressed(void)));
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

void    AdvanceConfigurationWnd::SetResolutionFromDevWnd(int index)
{
    QPalette palette = ui->adcResolutionComb->palette();
    palette.setColor(QPalette::Text, Qt::black); // Set text color to red
    ui->adcResolutionComb->setPalette(palette);
    ui->adcResolutionComb->blockSignals(true);
    ui->adcResolutionComb->setCurrentIndex(index);
    ui->adcResolutionComb->blockSignals(false);
}

void    AdvanceConfigurationWnd::SetClockDivFromDevWnd(int index)
{
    QPalette palette = ui->adcClockDivComb->palette();
    palette.setColor(QPalette::Text, Qt::black); // Set text color to red
    ui->adcClockDivComb->setPalette(palette);
    ui->adcClockDivComb->blockSignals(true);
    ui->adcClockDivComb->setCurrentIndex(index);
    ui->adcClockDivComb->blockSignals(false);
}

void    AdvanceConfigurationWnd::SetSampleTimeFromDevWnd(int index)
{
    QPalette palette = ui->adcSampleTimeComb->palette();
    palette.setColor(QPalette::Text, Qt::black); // Set text color to red
    ui->adcSampleTimeComb->setPalette(palette);
    ui->adcSampleTimeComb->blockSignals(true);
    ui->adcSampleTimeComb->setCurrentIndex(index);
    ui->adcSampleTimeComb->blockSignals(false);
}

void    AdvanceConfigurationWnd::onAdvResolutionChanged(int index)
{
    QPalette palette = ui->adcResolutionComb->palette();
    palette.setColor(QPalette::Text, Qt::red); // Set text color to red
    ui->adcResolutionComb->setPalette(palette);
    //emit sigAdvResolutionChanged(index);
}

void    AdvanceConfigurationWnd::onAdvClkDivChanged(int index)
{
    QPalette palette = ui->adcClockDivComb->palette();
    palette.setColor(QPalette::Text, Qt::red); // Set text color to red
    ui->adcClockDivComb->setPalette(palette);
    //emit sigAdvClockDivChanged(index);
}

void    AdvanceConfigurationWnd::onAdvSampleTimeChanged(int index)
{
    QPalette palette = ui->adcSampleTimeComb->palette();
    palette.setColor(QPalette::Text, Qt::red); // Set text color to red
    ui->adcSampleTimeComb->setPalette(palette);
    //emit sigAdvSampleTimeChanged(index);
}

void    AdvanceConfigurationWnd::onAdvConfigurationPressed(void)
{
    advConfigurationData cdata;
    QVariant data;
    cdata.resolution = ui->adcResolutionComb->currentText();
    cdata.sampleTime = ui->adcSampleTimeComb->currentText();
    cdata.clockDiv = ui->adcClockDivComb->currentText();
    cdata.averaginRatio = ui->averagingRatioComb->currentText();
    cdata.voltageOffset = ui->voltageOffsetLine->text();
    cdata.currentOffset = ui->currentOffsetLine->text();
    data.setValue(cdata);
    emit sigAdvConfigurationChanged(data);
}

void     AdvanceConfigurationWnd::assignResolutionList(const QStringList *items)
{
    ui->adcResolutionComb->addItems(*items);
}

void     AdvanceConfigurationWnd::assignClockDivList(const QStringList *items)
{
    ui->adcClockDivComb->addItems(*items);
}

void     AdvanceConfigurationWnd::assignSampleTimeList(const QStringList *items)
{
    ui->adcSampleTimeComb->addItems(*items);
}
