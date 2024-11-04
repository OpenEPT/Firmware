
#include "datastatistics.h"
#include "ui_datastatistics.h"

DataStatistics::DataStatistics(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DataAnalyzer)
{
    ui->setupUi(this);

    QFont defaultFont("Arial", 10); // Set desired font and size
    setFont(defaultFont);


    QVBoxLayout *mainVL = new QVBoxLayout(ui->AverageData);


    createVoltageInfoLayout(defaultFont);


    QFrame *hLine = new QFrame();
    hLine->setFrameShape(QFrame::HLine);  // Set the shape to horizontal line
    hLine->setFrameShadow(QFrame::Sunken);  // Optional: adds a 3D sunken look
    hLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);  // Expands horizontally

    QFrame *hLine1 = new QFrame();
    hLine1->setFrameShape(QFrame::HLine);  // Set the shape to horizontal line
    hLine1->setFrameShadow(QFrame::Sunken);  // Optional: adds a 3D sunken look
    hLine1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);  // Expands horizontally


    QHBoxLayout *hLineHL = new QHBoxLayout();
    hLineHL->addWidget(hLine);

    QHBoxLayout *hLineHL1 = new QHBoxLayout();
    hLineHL1->addWidget(hLine1);


    createCurrentInfoLayout(defaultFont);
    createConsumptionInfoLayout(defaultFont);

    mainVL->addLayout(voltageInfoHL);
    mainVL->addLayout(hLineHL);
    mainVL->addLayout(currentInfoHL);
    mainVL->addLayout(hLineHL1);
    mainVL->addLayout(consumptionInfoHL);
    mainVL->addStretch();

    // Set the first tab's layout
    ui->AverageData->setLayout(mainVL);

    ui->AverageData->setStyleSheet("QTabWidget::pane { background: transparent; }");
    ui->tabWidget->setStyleSheet("QTabWidget::pane { background: transparent; }");

    ui->tabWidget->setFont(defaultFont);
    ui->AverageData->setFont(defaultFont);
    ui->tabWidget->setTabText(0, "Data statistics");

    ui->verticalLayout->addWidget(ui->tabWidget);

    setLayout(ui->verticalLayout);
    setFixedSize(600, 300); // Set desired minimum width and height
}

DataStatistics::~DataStatistics()
{
    delete ui;
}

void DataStatistics::setVoltageStatisticInfo(double average, double max, double min)
{
    averageVoltageLine->setText(QString::number(average, 'f', 3));
    voltageDevMaxLine->setText(QString::number(max, 'f', 3));
    voltageDevMinLine->setText(QString::number(min, 'f', 3));
}

void DataStatistics::setCurrentStatisticInfo(double average, double max, double min)
{
    averageCurrentLine->setText(QString::number(average, 'f', 3));
    currentDevMaxLine->setText(QString::number(max, 'f', 3));
    currentDevMinLine->setText(QString::number(min, 'f', 3));
}

void DataStatistics::setConsumptionStatisticInfo(double average, double max, double min)
{
    averageConsumptionLine->setText(QString::number(average, 'f', 3));
    consumptionDevMaxLine->setText(QString::number(max, 'f', 3));
    consumptionDevMinLine->setText(QString::number(min, 'f', 3));
}

void DataStatistics::createVoltageInfoLayout(QFont font)
{
    // Create the vertical layout for the first tab
    voltageInfoHL = new QHBoxLayout();
    voltageInfoHL->setSizeConstraint(QLayout::SetMinimumSize);


    // Create the horizontal layout
    QVBoxLayout *averageVoltageVL = new QVBoxLayout();
    averageVoltageVL->setSizeConstraint(QLayout::SetMinimumSize);

    QHBoxLayout *averageVoltageHL = new QHBoxLayout();
    averageVoltageHL->setSizeConstraint(QLayout::SetMinimumSize);

    // Create the label and line edit
    QLabel *averageVoltageLabe = new QLabel("Average Voltage [V]", this);
    averageVoltageLabe->setFixedSize(210, 30);
    averageVoltageLabe->setFont(font);

    averageVoltageLine = new QLineEdit(this);
    averageVoltageLine->setFixedSize(80, 30);
    averageVoltageLine->setFont(font);
    averageVoltageLine->setReadOnly(true);


    averageVoltageHL->addWidget(averageVoltageLabe);
    averageVoltageHL->addWidget(averageVoltageLine);

    averageVoltageVL->addLayout(averageVoltageHL);



    QVBoxLayout *voltageDevInfoVL = new QVBoxLayout();

    voltageDevInfoVL->setSizeConstraint(QLayout::SetMinimumSize);

    QHBoxLayout *maxDevHL = new QHBoxLayout();

    QLabel *voltageDevMaxLabe = new QLabel("Max [V]", this);
    voltageDevMaxLabe->setFixedSize(80, 30);
    voltageDevMaxLabe->setFont(font);

    voltageDevMaxLine = new QLineEdit(this);
    voltageDevMaxLine->setFixedSize(80, 30);
    voltageDevMaxLine->setFont(font);
    voltageDevMaxLine->setReadOnly(true);

    maxDevHL->addWidget(voltageDevMaxLabe);
    maxDevHL->addWidget(voltageDevMaxLine);


    QHBoxLayout *minDevHL = new QHBoxLayout();

    QLabel *voltageDevMinLabe = new QLabel("Min [V]", this);
    voltageDevMinLabe->setFixedSize(80, 30);
    voltageDevMinLabe->setFont(font);

    voltageDevMinLine = new QLineEdit(this);
    voltageDevMinLine->setFixedSize(80, 30);
    voltageDevMinLine->setFont(font);
    voltageDevMinLine->setReadOnly(true);

    minDevHL->addWidget(voltageDevMinLabe);
    minDevHL->addWidget(voltageDevMinLine);

    voltageDevInfoVL->addLayout(maxDevHL);
    voltageDevInfoVL->addLayout(minDevHL);

    QSpacerItem *voltageInfoHSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Add horizontal layout to the vertical layout
    voltageInfoHL->addLayout(averageVoltageVL);
    voltageInfoHL->addLayout(voltageDevInfoVL);
    voltageInfoHL->addItem(voltageInfoHSpacer);
}

void DataStatistics::createCurrentInfoLayout(QFont font)
{
    // Create the vertical layout for the first tab
    currentInfoHL = new QHBoxLayout();
    currentInfoHL->setSizeConstraint(QLayout::SetMinimumSize);


    // Create the horizontal layout
    QVBoxLayout *averageCurrentVL = new QVBoxLayout();
    averageCurrentVL->setSizeConstraint(QLayout::SetMinimumSize);

    QHBoxLayout *averageCurrentHL = new QHBoxLayout();
    averageCurrentHL->setSizeConstraint(QLayout::SetMinimumSize);

    // Create the label and line edit
    QLabel *averageCurrentLabe = new QLabel("Average Current [mA]", this);
    averageCurrentLabe->setFixedSize(210, 30);
    averageCurrentLabe->setFont(font);

    averageCurrentLine = new QLineEdit(this);
    averageCurrentLine->setFixedSize(80, 30);
    averageCurrentLine->setFont(font);
    averageCurrentLine->setReadOnly(true);


    averageCurrentHL->addWidget(averageCurrentLabe);
    averageCurrentHL->addWidget(averageCurrentLine);

    averageCurrentVL->addLayout(averageCurrentHL);



    QVBoxLayout *currentDevInfoVL = new QVBoxLayout();

    currentDevInfoVL->setSizeConstraint(QLayout::SetMinimumSize);

    QHBoxLayout *maxDevHL = new QHBoxLayout();

    QLabel *currentDevMaxLabe = new QLabel("Max [mA]", this);
    currentDevMaxLabe->setFixedSize(80, 30);
    currentDevMaxLabe->setFont(font);

    currentDevMaxLine = new QLineEdit(this);
    currentDevMaxLine->setFixedSize(80, 30);
    currentDevMaxLine->setFont(font);
    currentDevMaxLine->setReadOnly(true);

    maxDevHL->addWidget(currentDevMaxLabe);
    maxDevHL->addWidget(currentDevMaxLine);


    QHBoxLayout *minDevHL = new QHBoxLayout();

    QLabel *currentDevMinLabe = new QLabel("Min [mA]", this);
    currentDevMinLabe->setFixedSize(80, 30);
    currentDevMinLabe->setFont(font);

    currentDevMinLine = new QLineEdit(this);
    currentDevMinLine->setFixedSize(80, 30);
    currentDevMinLine->setFont(font);
    currentDevMinLine->setReadOnly(true);

    minDevHL->addWidget(currentDevMinLabe);
    minDevHL->addWidget(currentDevMinLine);

    currentDevInfoVL->addLayout(maxDevHL);
    currentDevInfoVL->addLayout(minDevHL);

    QSpacerItem *currentInfoHSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Add horizontal layout to the vertical layout
    currentInfoHL->addLayout(averageCurrentVL);
    currentInfoHL->addLayout(currentDevInfoVL);
    currentInfoHL->addItem(currentInfoHSpacer);
}

void DataStatistics::createConsumptionInfoLayout(QFont font)
{
    // Create the vertical layout for the first tab
    consumptionInfoHL = new QHBoxLayout();
    consumptionInfoHL->setSizeConstraint(QLayout::SetMinimumSize);


    // Create the horizontal layout
    QVBoxLayout *averageConsumptionVL = new QVBoxLayout();
    averageConsumptionVL->setSizeConstraint(QLayout::SetMinimumSize);

    QHBoxLayout *averageConsumptionHL = new QHBoxLayout();
    averageConsumptionHL->setSizeConstraint(QLayout::SetMinimumSize);

    // Create the label and line edit
    QLabel *averageConsumptionLabe = new QLabel("Average Consumption [mAh]", this);
    averageConsumptionLabe->setFixedSize(210, 30);
    averageConsumptionLabe->setFont(font);

    averageConsumptionLine = new QLineEdit(this);
    averageConsumptionLine->setFixedSize(80, 30);
    averageConsumptionLine->setFont(font);
    averageConsumptionLine->setReadOnly(true);


    averageConsumptionHL->addWidget(averageConsumptionLabe);
    averageConsumptionHL->addWidget(averageConsumptionLine);

    averageConsumptionVL->addLayout(averageConsumptionHL);



    QVBoxLayout *consumptionDevInfoVL = new QVBoxLayout();

    consumptionDevInfoVL->setSizeConstraint(QLayout::SetMinimumSize);

    QHBoxLayout *maxDevHL = new QHBoxLayout();

    QLabel *consumptionDevMaxLabe = new QLabel("Max [mAh]", this);
    consumptionDevMaxLabe->setFixedSize(80, 30);
    consumptionDevMaxLabe->setFont(font);

    consumptionDevMaxLine = new QLineEdit(this);
    consumptionDevMaxLine->setFixedSize(80, 30);
    consumptionDevMaxLine->setFont(font);
    consumptionDevMaxLine->setReadOnly(true);

    maxDevHL->addWidget(consumptionDevMaxLabe);
    maxDevHL->addWidget(consumptionDevMaxLine);


    QHBoxLayout *minDevHL = new QHBoxLayout();

    QLabel *consumptionDevMinLabe = new QLabel("Min [mAh]", this);
    consumptionDevMinLabe->setFixedSize(80, 30);
    consumptionDevMinLabe->setFont(font);

    consumptionDevMinLine = new QLineEdit(this);
    consumptionDevMinLine->setFixedSize(80, 30);
    consumptionDevMinLine->setFont(font);
    consumptionDevMinLine->setReadOnly(true);

    minDevHL->addWidget(consumptionDevMinLabe);
    minDevHL->addWidget(consumptionDevMinLine);

    consumptionDevInfoVL->addLayout(maxDevHL);
    consumptionDevInfoVL->addLayout(minDevHL);

    QSpacerItem *consumptionInfoHSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Add horizontal layout to the vertical layout
    consumptionInfoHL->addLayout(averageConsumptionVL);
    consumptionInfoHL->addLayout(consumptionDevInfoVL);
    consumptionInfoHL->addItem(consumptionInfoHSpacer);
}
