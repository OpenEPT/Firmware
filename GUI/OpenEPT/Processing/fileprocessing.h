#ifndef FILEPROCESSING_H
#define FILEPROCESSING_H

#include <QObject>
#include <QFile>
#include <QThread>
#include <QSemaphore>

typedef enum
{
    FILEPROCESSING_TYPE_UKNOWN,
    FILEPROCESSING_TYPE_LOG,
    FILEPROCESSING_TYPE_SAMPLES
}fileprocessing_type_t;



class FileProcessing : public QObject
{
    Q_OBJECT
public:
    explicit                FileProcessing(QObject *parent = nullptr);

    bool                    open(fileprocessing_type_t aType, QString aPath);
    bool                    setSamplesFileHeader(QString header);
    bool                    setConsumptionFileHeader(QString header);
    bool                    setSummaryFileHeader(QString header);
    bool                    appendSampleData(QVector<double>* voltage, QVector<double>* voltageKeys, QVector<double>* current, QVector<double>* currentKeys);
    bool                    appendConsumptionData(QVector<double>* consumption, QVector<double>* consumptionKeys);
    bool                    appendSampleDataQueued(QVector<double> voltage, QVector<double> voltageKeys, QVector<double> current, QVector<double> currentKeys);
    bool                    appendConsumptionQueued(QVector<double> consumption, QVector<double> consumptionKeys);
    bool                    close();

signals:
    void                    sigAppendSampleData(QVector<double> voltage, QVector<double> voltageKeys, QVector<double> current, QVector<double> currentKeys);
    void                    sigAppendConsumptionData(QVector<double> consumption, QVector<double> consumptionKeys);

private slots:
    void                    onThreadStart();

public slots:
    void                    onAppendSampleData(QVector<double> voltage, QVector<double> voltageKeys, QVector<double> current, QVector<double> currentKeys);
    void                    onAppendConsumptionData(QVector<double> consumption, QVector<double> consumptionKeys);

private:
    QString                 summaryFilePath;
    QString                 samplesFilePath;
    QString                 consumptionFilePath;
    fileprocessing_type_t   type;
    QFile                   *summaryFile;
    QFile                   *samplesFile;
    QFile                   *consumptionFile;
    QThread                 *thread;
    QSemaphore              *sync;

};

#endif // FILEPROCESSING_H
