#include "openept.h"
#include "Processing/dataprocessing.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    OpenEPT w;
    w.setWindowState(Qt::WindowMaximized);
    w.show();


    qRegisterMetaType<dataprocessing_consumption_mode_t>("dataprocessing_consumption_mode_t");
    return a.exec();
}
