#ifndef PLOT_H
#define PLOT_H

#include <QWidget>
#include <QPushButton>
#include "Chart/qcustomplot.h"

class Plot : public QWidget
{
    Q_OBJECT
public:
    explicit    Plot(int mw, int mh, bool aEnableTracking = true, QWidget *parent = nullptr);
    void        scatterAddGraph();
    void        scatterAddData(QVector<double> data, QVector<double> keys);
    void        scatterAddDataWithName(double value, double keys, QString name);
    void        setData(QVector<double> data, QVector<double> keys);
    void        appendData(QVector<double> data, QVector<double> keys);
    void        setYRange(double min, double max);
    void        setYLabel(QString label);
    void        setXRange(double min, double max);
    void        setXLabel(QString label);
    void        setTitle(QString aTitle);
    void        clear();

signals:

private slots:
    void        onZoomIn();
    void        onZoomOut();
    void        onZoomExpand();
    void        onZoomArea();
    void        onMoveGraph();
    void        onTrackGraph();

private:
    QCustomPlot *plot;

    QPushButton *zoomIn;
    QPushButton *zoomOut;
    QPushButton *zoomExpand;
    QPushButton *zoomArea;
    QPushButton *moveGraph;
    QPushButton *trackGraph;

    QCPTextElement *title;

    QVector<double> xData;
    QVector<double> yData;

    bool        enableTracking;
    bool        replotActive;

    void        setButtonStyle();

};

#endif // PLOT_H
