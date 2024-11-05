#ifndef DATAANALYZER_H
#define DATAANALYZER_H

#include <QWidget>
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
    explicit DataAnalyzer(QWidget *parent = nullptr);
    ~DataAnalyzer();

private:
    Ui::DataAnalyzer *ui;
    QMainWindow *mainWindow;

    Plot    *voltageChart;
    Plot    *currentChart;
    Plot    *consumptionChart;

    void createVoltageSubWin();
    void createCurrentSubWin();
    void createConsumptionSubWin();

    QVector<QVector<double>> parseData(const QString &filePath);
};

#endif // DATAANALYZER_H
