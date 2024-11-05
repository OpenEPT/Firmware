#include "fileprocessing.h"
#include <QTextStream>
#include <QDebug>

FileProcessing::FileProcessing(QObject *parent)
    : QObject{parent}
{
    type = FILEPROCESSING_TYPE_UKNOWN;
    sync = new QSemaphore(0);
}

bool FileProcessing::open(fileprocessing_type_t aType, QString aPath)
{
    summaryFilePath = aPath + "/OpenEPT.txt";
    summaryFile = new QFile(summaryFilePath);
    samplesFilePath = aPath + "/vc.csv";
    consumptionFilePath = aPath + "/cons.csv";
    type = aType;
    switch(type)
    {
    case FILEPROCESSING_TYPE_UKNOWN:
        break;
    case FILEPROCESSING_TYPE_LOG:
        samplesFile = new QFile(aPath);
        if(!samplesFile->open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        break;
    case FILEPROCESSING_TYPE_SAMPLES:
        thread = new QThread(this);
        this->moveToThread(thread);
        connect(thread, SIGNAL(started()), this, SLOT(onThreadStart()));
        thread->start();
        /*Wait until files are created*/
        sync->acquire();
        break;
    }
    return true;
}

bool FileProcessing::setSamplesFileHeader(QString header)
{
    if(!samplesFile->isOpen())
    {
        if(!samplesFile->open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    }
    QTextStream out(samplesFile);
    samplesFileHeader = header;
    out << header << "\n";
    out << "------------\n";
    switch(type)
    {
    case FILEPROCESSING_TYPE_UKNOWN:
        out << "Error\n";
        break;
    case FILEPROCESSING_TYPE_LOG:
        out << "Log\n";
        break;
    case FILEPROCESSING_TYPE_SAMPLES:
        out << "Voltage, VolTime, Current, CurTime\n";
        break;
    }
    samplesFile->close();
    return true;
}

bool FileProcessing::setConsumptionFileHeader(QString header)
{
    if(!consumptionFile->isOpen())
    {
        if(!consumptionFile->open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    }
    QTextStream out(consumptionFile);
    consumptionFileHeader = header;
    out << header << "\n";
    out << "------------\n";
    switch(type)
    {
    case FILEPROCESSING_TYPE_UKNOWN:
        out << "Error\n";
        break;
    case FILEPROCESSING_TYPE_LOG:
        out << "Log\n";
        break;
    case FILEPROCESSING_TYPE_SAMPLES:
        out << "Consumption, ConTime\n";
        break;
    }
    consumptionFile->close();
    return true;
}

bool FileProcessing::setSummaryFileHeader(QString header)
{
    if(!summaryFile->isOpen())
    {
        if(!summaryFile->open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    }
    QTextStream out(summaryFile);
    summaryFileHeader = header;
    out << header << "\n";
    out << "------------\n";
    summaryFile->close();
    return true;
}

bool FileProcessing::appendSummaryFile(QString content)
{
    if(!summaryFile->isOpen())
    {
        if(!summaryFile->open(QIODevice::WriteOnly | QIODevice::Text| QIODevice::Append)) return false;
    }
    QTextStream out(summaryFile);
    out << content << "\n";
    summaryFile->close();
    return true;
}

bool FileProcessing::appendSampleData(QVector<double>* voltage, QVector<double>* voltageKeys, QVector<double>* current, QVector<double>* currentKeys)
{
    if(!samplesFile->isOpen())
    {
        if(!samplesFile->open(QIODevice::WriteOnly | QIODevice::Text| QIODevice::Append)) return false;
    }
    QTextStream out(samplesFile);
    //TODO: Check QVectors sizes;
    for(unsigned int i = 0; i < voltage->size(); i++)
    {
        out << QString::asprintf("%1.5f", voltage->at(i))      <<  ",";
        out << QString::asprintf("%1.7f", voltageKeys->at(i))      << ",";
        out << QString::asprintf("%1.5f", current->at(i))      << ",";
        out << QString::asprintf("%1.7f", currentKeys->at(i))      << "\n";
    }
    samplesFile->close();
    return true;
}

bool FileProcessing::appendConsumptionData(QVector<double> *consumption, QVector<double> *consumptionKeys)
{
    if(!consumptionFile->isOpen())
    {
        if(!consumptionFile->open(QIODevice::WriteOnly | QIODevice::Text| QIODevice::Append)) return false;
    }
    QTextStream out(consumptionFile);
    //TODO: Check QVectors sizes;
    for(unsigned int i = 0; i < consumption->size(); i++)
    {
        out << QString::asprintf("%1.5f", consumption->at(i))      <<  ",";
        out << QString::asprintf("%1.7f", consumptionKeys->at(i))  << "\n";
    }
    consumptionFile->close();
    return true;
}

bool FileProcessing::appendSampleDataQueued(QVector<double> voltage, QVector<double> voltageKeys, QVector<double> current, QVector<double> currentKeys)
{
    emit sigAppendSampleData(voltage, voltageKeys, current, currentKeys);
    return true;
}

bool FileProcessing::appendConsumptionQueued(QVector<double> consumption, QVector<double> consumptionKeys)
{
    emit sigAppendConsumptionData(consumption, consumptionKeys);
    return true;
}

bool FileProcessing::close()
{
    if(!samplesFile->isOpen()) return false;
    samplesFile->close();
    return true;
}

bool FileProcessing::reOpenFiles()
{
    if(samplesFile->exists())
    {
        samplesFile->remove();
        setSamplesFileHeader(samplesFileHeader);
    }
    if(consumptionFile->exists())
    {
        consumptionFile->remove();
        setConsumptionFileHeader(consumptionFileHeader);
    }
    if(summaryFile->exists())
    {
        summaryFile->remove();
        setSummaryFileHeader(summaryFileHeader);
    }
    return true;
}

void FileProcessing::onThreadStart()
{
    switch(type)
    {
    case FILEPROCESSING_TYPE_UKNOWN:
        break;
    case FILEPROCESSING_TYPE_LOG:
        break;
    case FILEPROCESSING_TYPE_SAMPLES:
        samplesFile = new QFile(samplesFilePath);
        consumptionFile = new QFile(consumptionFilePath);
        samplesFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append);
        consumptionFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append);
        qDebug() << samplesFile;
        qRegisterMetaType<QVector<int> >("QVector<double>");
        connect(this, SIGNAL(sigAppendSampleData(QVector<double>,QVector<double>,QVector<double>,QVector<double>)), this, SLOT(onAppendSampleData(QVector<double>,QVector<double>,QVector<double>,QVector<double>)), Qt::QueuedConnection);
        connect(this, SIGNAL(sigAppendConsumptionData(QVector<double>,QVector<double>)),                            this, SLOT(onAppendConsumptionData(QVector<double>,QVector<double>)), Qt::QueuedConnection);
        sync->release();
        break;
    }
}

void FileProcessing::onAppendSampleData(QVector<double> voltage, QVector<double> voltageKeys, QVector<double> current, QVector<double> currentKeys)
{
    appendSampleData(&voltage, &voltageKeys, &current, &currentKeys);
}

void FileProcessing::onAppendConsumptionData(QVector<double> consumption, QVector<double> consumptionKeys)
{
    appendConsumptionData(&consumption, &consumptionKeys);
}
