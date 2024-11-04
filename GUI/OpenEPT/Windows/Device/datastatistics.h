#ifndef DATASTATISTICS_H
#define DATASTATISTICS_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QLineEdit>

namespace Ui {
class DataAnalyzer;
}

class DataStatistics : public QWidget
{
    Q_OBJECT

public:
    explicit DataStatistics(QWidget *parent = nullptr);
    ~DataStatistics();

    void    setVoltageStatisticInfo(double average, double max, double min);
    void    setCurrentStatisticInfo(double average, double max, double min);
    void    setConsumptionStatisticInfo(double average, double max, double min);

private:
    Ui::DataAnalyzer *ui;

    void            createVoltageInfoLayout(QFont font);
    void            createCurrentInfoLayout(QFont font);
    void            createConsumptionInfoLayout(QFont font);

    QHBoxLayout     *voltageInfoHL;
    QHBoxLayout     *currentInfoHL;
    QHBoxLayout     *consumptionInfoHL;

    QLineEdit       *averageVoltageLine;
    QLineEdit       *voltageDevMaxLine;
    QLineEdit       *voltageDevMinLine;
    QLineEdit       *averageCurrentLine;
    QLineEdit       *currentDevMaxLine;
    QLineEdit       *currentDevMinLine;
    QLineEdit       *averageConsumptionLine;
    QLineEdit       *consumptionDevMaxLine;
    QLineEdit       *consumptionDevMinLine;
};

#endif // DATASTATISTICS_H
