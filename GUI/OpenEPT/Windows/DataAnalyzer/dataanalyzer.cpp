#include "dataanalyzer.h"
#include "ui_dataanalyzer.h"


#define PLOT_MINIMUM_SIZE_HEIGHT 100
#define PLOT_MINIMUM_SIZE_WIDTH 500

DataAnalyzer::DataAnalyzer(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DataAnalyzer)
{
    ui->setupUi(this);

    QFont defaultFont("Arial", 10); // Set desired font and size
    setFont(defaultFont);


    // Set up the main layout for DataAnalyzer
    QVBoxLayout *mainLayout = new QVBoxLayout(this);


    // Create a horizontal layout for the button and line edit
    QHBoxLayout *topLayout = new QHBoxLayout;
    QPushButton *loadFilePushb = new QPushButton();
    QPushButton *processFilePushb = new QPushButton();
    QLineEdit *lineEdit = new QLineEdit();
    lineEdit->setPlaceholderText("Consumption data file path");
    lineEdit->setFont(defaultFont);

    QPixmap buttonIconPng(":/images/NewSet/directory.png");
    QIcon buttonIcon(buttonIconPng);
    loadFilePushb->setIcon(buttonIcon);
    loadFilePushb->setIconSize(QSize(30,30));
    loadFilePushb->setToolTip("Zoom in");
    loadFilePushb->setFixedSize(30, 30);

    QPixmap processIconPng(":/images/NewSet/process.png");
    QIcon processIcon(processIconPng);
    processFilePushb->setIcon(processIcon);
    processFilePushb->setIconSize(QSize(30,30));
    processFilePushb->setToolTip("Zoom in");
    processFilePushb->setFixedSize(30, 30);

    // Add the button and line edit to the horizontal layout
    topLayout->addWidget(processFilePushb);
    topLayout->addStretch();
    topLayout->addWidget(lineEdit);
    topLayout->addWidget(loadFilePushb);

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

    QLabel *label = new QLabel("Voltage");
    label->setAlignment(Qt::AlignCenter);

    voltageChart             = new Plot(PLOT_MINIMUM_SIZE_WIDTH/2, PLOT_MINIMUM_SIZE_HEIGHT, false);
    voltageChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    voltageChart->setTitle("Voltage");
    voltageChart->setYLabel("[V]");
    voltageChart->setXLabel("[ms]");

    layout->addWidget(label);
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

    QLabel *label = new QLabel("Voltage");
    label->setAlignment(Qt::AlignCenter);

    currentChart             = new Plot(PLOT_MINIMUM_SIZE_WIDTH/2, PLOT_MINIMUM_SIZE_HEIGHT, false);
    currentChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    currentChart->setTitle("Current");
    currentChart->setYLabel("[mA]");
    currentChart->setXLabel("[ms]");

    layout->addWidget(label);
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

    QLabel *label = new QLabel("Consumption");
    label->setAlignment(Qt::AlignCenter);

    consumptionChart             = new Plot(PLOT_MINIMUM_SIZE_WIDTH/2, PLOT_MINIMUM_SIZE_HEIGHT, false);
    consumptionChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    consumptionChart->setTitle("Consumption");
    consumptionChart->setYLabel("[mA]");
    consumptionChart->setXLabel("[ms]");

    layout->addWidget(label);
    layout->addWidget(consumptionChart);
    contentWidget->setLayout(layout);


    // Set the content widget in the dock widget
    dockWidget->setWidget(contentWidget);

    // Add the dock widget to the specified area in the main window
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dockWidget);

    // Make dock widgets floatable and closable
    dockWidget->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
}

QVector<QVector<double> > DataAnalyzer::parseData(const QString &filePath)
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

DataAnalyzer::~DataAnalyzer()
{
    delete ui;
}
