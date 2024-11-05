#ifndef DATAANALYZER_H
#define DATAANALYZER_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QComboBox>
#include <QMdiArea>
#include <QBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QMdiSubWindow>
#include <QDockWidget>
#include <QMainWindow>
#include "Windows/Plot/plot.h"

namespace Ui {
class DataAnalyzer;
}

class DataAnalyzer : public QWidget
{
    Q_OBJECT

public:
    explicit DataAnalyzer(QWidget *parent = nullptr, QString aWsDirPath="");
    ~DataAnalyzer();

public slots:
    void    onRealoadConsumptionProfiles();

private:
    Ui::DataAnalyzer *ui;
    QMainWindow *mainWindow;

    QLabel*     detectedProfilesLabe;

    QString     wsDirPath;

    Plot    *voltageChart;
    Plot    *currentChart;
    Plot    *consumptionChart;

    void createVoltageSubWin();
    void createCurrentSubWin();
    void createConsumptionSubWin();

    QComboBox               *consumptionProfilesCB;
    QStringList             consumptionProfilesName;

    QVector<QVector<double>> parseData(const QString &filePath);
    QStringList             listSubdirectories();


    void                    realoadConsumptionProfiles();
};

#endif // DATAANALYZER_H
