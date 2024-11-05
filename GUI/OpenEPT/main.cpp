#include "openept.h"
#include "Processing/dataprocessing.h"
#include "Windows/WSSelection/selectworkspace.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    SelectWorkspace wsSelection;
    if(wsSelection.exec() == QDialog::Accepted){;
        OpenEPT w(wsSelection.getWSPath());
        w.setWindowState(Qt::WindowMaximized);
        w.show();


        qRegisterMetaType<dataprocessing_consumption_mode_t>("dataprocessing_consumption_mode_t");
        qRegisterMetaType<dataprocessing_dev_info_t>("dataprocessing_dev_info_t");
        return a.exec();
    }
    else
    {
        return -1;
    }
}
