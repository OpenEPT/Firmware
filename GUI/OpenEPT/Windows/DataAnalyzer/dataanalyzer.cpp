#include "dataanalyzer.h"
#include "ui_dataanalyzer.h"


#define PLOT_MINIMUM_SIZE_HEIGHT 100
#define PLOT_MINIMUM_SIZE_WIDTH 500

DataAnalyzer::DataAnalyzer(QWidget *parent, QString aWsDirPath) :
    QWidget(parent),
    ui(new Ui::DataAnalyzer)
{
    ui->setupUi(this);

    wsDirPath = aWsDirPath;

    QFont defaultFont("Arial", 10); // Set desired font and size
    setFont(defaultFont);


    // Set up the main layout for DataAnalyzer
    QVBoxLayout *mainLayout = new QVBoxLayout(this);


    // Create a horizontal layout for the button and line edit
    QHBoxLayout *topLayout = new QHBoxLayout;
    QPushButton *reloadProfileNamesPushb = new QPushButton();
    QPushButton *processFilePushb = new QPushButton();


    detectedProfilesLabe    = new QLabel("Detect consumption profiles", this);
    detectedProfilesLabe->setFixedSize(260, 30);
    detectedProfilesLabe->setFont(defaultFont);

    consumptionProfilesCB = new QComboBox();
    consumptionProfilesCB->setFont(defaultFont);
    consumptionProfilesCB->setFixedSize(150, 30);

    connect(consumptionProfilesCB, SIGNAL(currentIndexChanged(int)), this, SLOT(onConsumptionProfileChanged(int)));
    realoadConsumptionProfiles();

    QPixmap buttonIconPng(":/images/NewSet/reload.png");
    QIcon buttonIcon(buttonIconPng);
    reloadProfileNamesPushb->setIcon(buttonIcon);
    reloadProfileNamesPushb->setIconSize(QSize(30,30));
    reloadProfileNamesPushb->setToolTip("Reload consumption profiles");
    reloadProfileNamesPushb->setFixedSize(30, 30);

    connect(reloadProfileNamesPushb, SIGNAL(clicked(bool)), this, SLOT(onRealoadConsumptionProfiles()));

    QPixmap processIconPng(":/images/NewSet/load.png");
    QIcon processIcon(processIconPng);
    processFilePushb->setIcon(processIcon);
    processFilePushb->setIconSize(QSize(30,30));
    processFilePushb->setToolTip("Process consumption profile data");
    processFilePushb->setFixedSize(30, 30);

    connect(processFilePushb, SIGNAL(clicked(bool)), this, SLOT(onLoadConsumptionProfileData()));

    // Add the button and line edit to the horizontal layout
    topLayout->addWidget(detectedProfilesLabe);
    topLayout->addWidget(consumptionProfilesCB);
    topLayout->addWidget(reloadProfileNamesPushb);
    topLayout->addStretch();
    topLayout->addWidget(processFilePushb);

     mainLayout->addLayout(topLayout);

    // Create an internal QMainWindow to handle docking
    mainWindow = new QMainWindow(this);
    mainWindow->setWindowFlags(Qt::Widget);  // Make QMainWindow behave as a regular widget
    mainWindow->setDockNestingEnabled(true); // Allow nested docking if needed

    // Add QMainWindow to the layout
    mainLayout->addWidget(mainWindow);


    createVoltageSubWin();
    createCurrentSubWin();
    createConsumptionSubWin();

}

void DataAnalyzer::createVoltageSubWin() {
    // Create a dock widget
    QDockWidget *dockWidget = new QDockWidget("Voltage", this);
    dockWidget->setAllowedAreas(Qt::AllDockWidgetAreas);

    // Create a content widget with a layout for the dock widget
    QWidget *contentWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(contentWidget);

    voltageChart             = new Plot(PLOT_MINIMUM_SIZE_WIDTH/2, PLOT_MINIMUM_SIZE_HEIGHT, false);
    voltageChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    voltageChart->setTitle("Voltage");
    voltageChart->setYLabel("[V]");
    voltageChart->setXLabel("[ms]");

    layout->addWidget(voltageChart);
    contentWidget->setLayout(layout);


    // Set the content widget in the dock widget
    dockWidget->setWidget(contentWidget);

    // Add the dock widget to the specified area in the main window
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dockWidget);

    // Make dock widgets floatable and closable
    dockWidget->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
}

void DataAnalyzer::createCurrentSubWin()
{
    // Create a dock widget
    QDockWidget *dockWidget = new QDockWidget("Current", this);
    dockWidget->setAllowedAreas(Qt::AllDockWidgetAreas);

    // Create a content widget with a layout for the dock widget
    QWidget *contentWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(contentWidget);


    currentChart             = new Plot(PLOT_MINIMUM_SIZE_WIDTH/2, PLOT_MINIMUM_SIZE_HEIGHT, false);
    currentChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    currentChart->setTitle("Current");
    currentChart->setYLabel("[mA]");
    currentChart->setXLabel("[ms]");

    layout->addWidget(currentChart);
    contentWidget->setLayout(layout);


    // Set the content widget in the dock widget
    dockWidget->setWidget(contentWidget);

    // Add the dock widget to the specified area in the main window
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dockWidget);

    // Make dock widgets floatable and closable
    dockWidget->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
}

void DataAnalyzer::createConsumptionSubWin()
{
    // Create a dock widget
    QDockWidget *dockWidget = new QDockWidget("Consumption", this);
    dockWidget->setAllowedAreas(Qt::AllDockWidgetAreas);

    // Create a content widget with a layout for the dock widget
    QWidget *contentWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(contentWidget);

    consumptionChart             = new Plot(PLOT_MINIMUM_SIZE_WIDTH/2, PLOT_MINIMUM_SIZE_HEIGHT, false);
    consumptionChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    consumptionChart->setTitle("Consumption");
    consumptionChart->setYLabel("[mA]");
    consumptionChart->setXLabel("[ms]");

    layout->addWidget(consumptionChart);
    contentWidget->setLayout(layout);


    // Set the content widget in the dock widget
    dockWidget->setWidget(contentWidget);

    // Add the dock widget to the specified area in the main window
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dockWidget);

    // Make dock widgets floatable and closable
    dockWidget->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
}

QVector<QVector<double> > DataAnalyzer::parseVCData(const QString &filePath)
{
    // Create a QVector to store each column
    QVector<QVector<double>> data(4);  // Initialize with 4 QVectors

    // Open the CSV file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << filePath;
        return data;
    }

    QTextStream in(&file);

    // Skip the first two lines
    in.readLine();
    in.readLine();

    // Read each line and parse the data
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList values = line.split(',');

        // Ensure we have exactly 4 columns in the line
        if (values.size() == 4) {
            bool ok;

            // Parse and add each column to the corresponding QVector
            double voltage = values[0].toDouble(&ok);
            if (ok) data[0].append(voltage);

            double time1 = values[1].toDouble(&ok);
            if (ok) data[1].append(time1);

            double current = values[2].toDouble(&ok);
            if (ok) data[2].append(current);

            double time2 = values[3].toDouble(&ok);
            if (ok) data[3].append(time2);
        } else {
            qWarning() << "Unexpected number of columns in line:" << line;
        }
    }

    file.close();
    return data;
}

QVector<QVector<double> > DataAnalyzer::parseConsumptionData(const QString &filePath)
{
    // Create a QVector to store each column
    QVector<QVector<double>> data(2);  // Initialize with 4 QVectors

    // Open the CSV file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << filePath;
        return data;
    }

    QTextStream in(&file);

    // Skip the first two lines
    in.readLine();
    in.readLine();

    // Read each line and parse the data
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList values = line.split(',');

        // Ensure we have exactly 4 columns in the line
        if (values.size() == 2) {
            bool ok;

            // Parse and add each column to the corresponding QVector
            double voltage = values[0].toDouble(&ok);
            if (ok) data[0].append(voltage);

            double time1 = values[1].toDouble(&ok);
            if (ok) data[1].append(time1);

        } else {
            qWarning() << "Unexpected number of columns in line:" << line;
        }
    }

    file.close();
    return data;
}

QStringList DataAnalyzer::listConsumptionProfiles()
{
    QStringList subdirectories;

    QDir dir(wsDirPath);

    // Check if the main directory exists
    if (!dir.exists()) return subdirectories;

    // Set the filter to only look for directories, excluding "." and ".."
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

    // Iterate through the directory entries and add the directory names to the list
    foreach (const QFileInfo &entry, dir.entryInfoList()) {
        if (entry.isDir()) {
            // Create a QDir object for the subdirectory
            QDir subDir(entry.filePath());
            // Check if OpenEPT.txt exists in the subdirectory
            if (subDir.exists("OpenEPT.txt")) {
                subdirectories << entry.fileName(); // Add the subdirectory name to the list
            }
        }
    }

    return subdirectories;
}

void DataAnalyzer::realoadConsumptionProfiles()
{
    consumptionProfilesCB->clear();
    consumptionProfilesCB->setEnabled(false);
    consumptionProfilesName = listConsumptionProfiles();
    if(consumptionProfilesName.size() == 0)
    {
        detectedProfilesLabe->setText("No detected consumption profile");
    }
    else
    {
        consumptionProfilesCB->addItems(consumptionProfilesName);
        detectedProfilesLabe->setText("Select one consumption profile");
        consumptionProfilesCB->setEnabled(true);
    }
}

DataAnalyzer::~DataAnalyzer()
{
    delete ui;
}

void DataAnalyzer::onRealoadConsumptionProfiles()
{
    realoadConsumptionProfiles();
}

void DataAnalyzer::onConsumptionProfileChanged(int index)
{
    selectedConsumptionProfile = consumptionProfilesCB->itemText(index);
}

void DataAnalyzer::loadConsumptionProfileData()
{

    QString voltageCurrentPath = wsDirPath + "/" + selectedConsumptionProfile + "/vc.csv";
    QString consPath = wsDirPath + "/" + selectedConsumptionProfile + "/cons.csv";
    QVector<QVector<double> > vcData = parseVCData(voltageCurrentPath);
    QVector<QVector<double> > consData = parseConsumptionData(consPath);

    voltageChart->setData(vcData[0],vcData[1]);
    currentChart->setData(vcData[2],vcData[3]);
    consumptionChart->setData(consData[0], consData[1]);
}
void DataAnalyzer::onLoadConsumptionProfileData()
{
    loadConsumptionProfileData();

}
