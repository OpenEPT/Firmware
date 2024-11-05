#include <QtOpenGL>
#include "plot.h"

#define BUTTONS_SIZE 30

Plot::Plot(int mw, int mh, bool aEnableTracking, QWidget *parent)
    : QWidget{parent}
{
    /* Set parent */
    this->setParent(parent);

    /* Create plot*/
    plot = new QCustomPlot();
    plot->setMinimumSize(mw, mh);
    plot->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    plot->setAntialiasedElements(QCP::aeAll);

    plot->addGraph(); // blue line
    plot->graph(0)->setPen(QPen(QColor(40, 110, 255)));
    plot->setInteraction(QCP::iSelectPlottables, true);
    plot->setInteraction(QCP::iRangeDrag, true);
    plot->setInteraction(QCP::iRangeZoom, true);

    //plot->setBackground(QBrush(QColor("#7a7d7c")));

    zoomIn      = new QPushButton();
    zoomOut     = new QPushButton();
    zoomExpand  = new QPushButton();
    zoomArea    = new QPushButton();
    moveGraph   = new QPushButton();
    trackGraph  = new QPushButton();

    QPixmap zoomInPng(":/images/NewSet/zoom_in.png");
    QIcon zoomInIcon(zoomInPng);
    zoomIn->setIcon(zoomInIcon);
    zoomIn->setIconSize(QSize(15,15));
    zoomIn->setToolTip("Zoom in");
    zoomIn->setFixedSize(BUTTONS_SIZE, BUTTONS_SIZE);


    QPixmap zoomOutPng(":/images/NewSet/zoom_out.png");
    QIcon zoomOutIcon(zoomOutPng);
    zoomOut->setIcon(zoomOutIcon);
    zoomOut->setIconSize(QSize(15,15));
    zoomOut->setToolTip("Zoom out");
    zoomOut->setFixedSize(BUTTONS_SIZE, BUTTONS_SIZE);

    QPixmap zoomExpandPng(":/images/NewSet/expand.png");
    QIcon zoomExpandIcon(zoomExpandPng);
    zoomExpand->setIcon(zoomExpandIcon);
    zoomExpand->setIconSize(QSize(15,15));
    zoomExpand->setToolTip("Fit to full data");
    zoomExpand->setFixedSize(BUTTONS_SIZE, BUTTONS_SIZE);

    QPixmap zoomAreaPng(":/images/NewSet/zoom_area.png");
    QIcon zoomAreaIcon(zoomAreaPng);
    zoomArea->setIcon(zoomAreaIcon);
    zoomArea->setIconSize(QSize(15,15));
    zoomArea->setToolTip("Zoom area");
    zoomArea->setFixedSize(BUTTONS_SIZE, BUTTONS_SIZE);

    QPixmap moveGraphPng(":/images/NewSet/moveGraph.png");
    QIcon moveGraphIcon(moveGraphPng);
    moveGraph->setIcon(moveGraphIcon);
    moveGraph->setIconSize(QSize(15,15));
    moveGraph->setToolTip("Move graph");
    moveGraph->setFixedSize(BUTTONS_SIZE, BUTTONS_SIZE);

    QPixmap trackGraphPng(":/images/NewSet/tracking_graph.png");
    QIcon trackGraphIcon(trackGraphPng);
    trackGraph->setIcon(trackGraphIcon);
    trackGraph->setIconSize(QSize(15,15));
    trackGraph->setToolTip("Enable graph tracking");
    trackGraph->setFixedSize(BUTTONS_SIZE, BUTTONS_SIZE);


    QVBoxLayout *buttonsLayout = new QVBoxLayout();
    buttonsLayout->addWidget(zoomIn);
    buttonsLayout->addWidget(zoomOut);
    buttonsLayout->addWidget(zoomExpand);
    buttonsLayout->addWidget(zoomArea);
    buttonsLayout->addWidget(moveGraph);
    buttonsLayout->addWidget(trackGraph);
    buttonsLayout->setAlignment(Qt::AlignCenter);


    QHBoxLayout *plotLayout = new QHBoxLayout(this);
    plotLayout->addLayout(buttonsLayout);
    plotLayout->addWidget(plot);
    plotLayout->setAlignment(Qt::AlignLeft);
    plotLayout->setSpacing(3);

    plot->plotLayout()->insertRow(0);
    title = new QCPTextElement(plot, "NN", QFont("Helvetica", 16));
    plot->plotLayout()->addElement(0, 0, title);

    enableTracking      = aEnableTracking;
    replotActive        = true;

    connect(zoomIn, SIGNAL(pressed()), this, SLOT(onZoomIn()));
    connect(zoomOut, SIGNAL(pressed()), this, SLOT(onZoomOut()));
    connect(zoomExpand, SIGNAL(pressed()), this, SLOT(onZoomExpand()));
    connect(zoomArea, SIGNAL(pressed()), this, SLOT(onZoomArea()));
    connect(moveGraph, SIGNAL(pressed()), this, SLOT(onMoveGraph()));
    connect(trackGraph, SIGNAL(pressed()), this, SLOT(onTrackGraph()));
    setButtonStyle();
}

void        Plot::scatterAddGraph()
{
    plot->addGraph();

    plot->graph(1)->setLineStyle(QCPGraph::lsNone);  // No line
    plot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, 10));  // Red circle marker, size 10

}

void Plot::scatterAddData(QVector<double> data, QVector<double> keys)
{
    plot->graph(1)->setData(keys, data);

    for(int i = 0; i < data.size(); i++)
    {
        QCPItemText *textLabel = new QCPItemText(plot);

        // Set text label position above each point
        textLabel->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        textLabel->position->setType(QCPItemPosition::ptPlotCoords);  // Position in plot coordinates
        textLabel->position->setCoords(keys[i], data[i] + 0.5);  // Set position slightly above the point

        // Set text style and content
        textLabel->setText(QString::number(i));  // Set the text (label)
        textLabel->setFont(QFont("Arial", 10));  // Set font and size
        textLabel->setColor(Qt::blue);  // Set text color
    }

    plot->replot();
}

void Plot::scatterAddDataWithName(double value, double keys, QString name)
{
    plot->graph(1)->addData(keys, value);
    QCPItemText *textLabel = new QCPItemText(plot);

    // Set text label position above each point
    textLabel->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    textLabel->position->setType(QCPItemPosition::ptPlotCoords);  // Position in plot coordinates
    textLabel->position->setCoords(keys, value + 0.5);  // Set position slightly above the point

    // Set text style and content
    textLabel->setText(name);  // Set the text (label)
    textLabel->setFont(QFont("Times", 10));  // Set font and size
    textLabel->setColor(Qt::blue);  // Set text color

    plot->replot();
}
void        Plot::setData(QVector<double> data, QVector<double> keys)
{
    xData = keys;
    yData = data;
    if(replotActive)
    {
        plot->graph(0)->setData(xData, yData, true);
        plot->rescaleAxes(true);
        plot->replot();
    }
}
void        Plot::appendData(QVector<double> data, QVector<double> keys)
{
    //plot->graph(0)->addData(keys, data);
    xData.append(keys);
    yData.append(data);
    if(xData.size() > 10000000)
    {
        xData.remove(0,data.size());
        yData.remove(0,data.size());
    }
    if(replotActive)
    {
        double minxValue = keys.at(keys.size()-1) - 10000 ;
        if(minxValue < 0) minxValue = 0;
        double maxxValue = keys.at(keys.size()-1);
        plot->graph(0)->setData(xData, yData, true);
        plot->xAxis->setRange(minxValue, maxxValue);
        plot->yAxis->rescale(true);
        plot->replot();
    }

}
void        Plot::setYRange(double min, double max)
{
    plot->yAxis->setRange(min, max);
    plot->replot();
}
void        Plot::setYLabel(QString label)
{
    plot->yAxis->setLabel(label);
    plot->replot();
}
void        Plot::setXRange(double min, double max)
{
    plot->xAxis->setRange(min, max);
    plot->replot();
}
void        Plot::setXLabel(QString label)
{
    plot->xAxis->setLabel(label);
    plot->replot();
}
void        Plot::setTitle(QString aTitle)
{
    title->setText(aTitle);
    plot->replot();
}
void        Plot::clear()
{
    plot->graph(0)->data()->clear();
    plot->replot();
    xData.clear();
    yData.clear();

}
void        Plot::onZoomIn()
{
    plot->setInteraction(QCP::iRangeDrag, true);
    plot->setInteraction(QCP::iRangeZoom, true);
    plot->xAxis->scaleRange(.85, plot->xAxis->range().center());
    plot->yAxis->scaleRange(.85, plot->yAxis->range().center());
    plot->replot();
    setButtonStyle();
}
void        Plot::onZoomOut()
{
    plot->setInteraction(QCP::iRangeDrag, true);
    plot->setInteraction(QCP::iRangeZoom, true);
    plot->xAxis->scaleRange(1.25, plot->xAxis->range().center());
    plot->yAxis->scaleRange(1.25, plot->yAxis->range().center());
    plot->replot();
    setButtonStyle();
}
void        Plot::onZoomExpand()
{
    plot->setInteraction(QCP::iRangeDrag, false);
    plot->setInteraction(QCP::iRangeZoom, false);
    plot->rescaleAxes(true);
    plot->setSelectionRectMode(QCP::srmZoom);
    plot->replot();
    setButtonStyle();
}
void        Plot::onZoomArea()
{
    plot->setInteraction(QCP::iRangeDrag, true);
    plot->setInteraction(QCP::iRangeZoom, true);
    plot->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
    plot->axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
    plot->setSelectionRectMode(QCP::srmZoom);
    plot->replot();
    setButtonStyle();
}
void        Plot::onMoveGraph()
{
    plot->setInteraction(QCP::iRangeDrag, true);
    plot->setInteraction(QCP::iRangeZoom, false);
    plot->setInteraction(QCP::iSelectPlottables, true);
    plot->setSelectionRectMode(QCP::srmNone);
    plot->replot();
    setButtonStyle();
}
void       Plot::onTrackGraph()
{
    enableTracking = enableTracking == false? true : false;
    replotActive = enableTracking;
    setButtonStyle();
}

void Plot::setButtonStyle()
{
    if(enableTracking)
    {
        trackGraph->setStyleSheet("background-color:  rgb(255,197,172);");
        zoomIn->setEnabled(false);
        zoomOut->setEnabled(false);
        moveGraph->setEnabled(false);
        zoomExpand->setEnabled(false);
        zoomArea->setEnabled(false);
    }
    else
    {
        trackGraph->setStyleSheet("background-color:  rgb(255,255,255);");
        zoomIn->setEnabled(true);
        zoomOut->setEnabled(true);
        moveGraph->setEnabled(true);
        zoomExpand->setEnabled(true);
        zoomArea->setEnabled(true);
    }

}
