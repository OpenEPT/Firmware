#include <QPushButton>
#include <QAction>
#include <QMenu>
#include "openept.h"
#include "Windows/Device/devicewnd.h"
#include "ui_openept.h"
#include "Links/controllink.h"


#define BUTTON_WIDTH (25)


OpenEPT::OpenEPT(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::OpenEPT)
{
    ui->setupUi(this);

    dataAnalyzerWnd = new DataAnalyzer();

    addDeviceWnd = new AddDeviceWnd(this);
    addDeviceWnd->setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    addDeviceWnd->setWindowModality(Qt::WindowModal);
    connect(addDeviceWnd, SIGNAL(sigAddDevice(QString,QString)), this, SLOT(onAddDeviceWndAddDevice(QString,QString)), Qt::QueuedConnection);


    connectedDevicesMenu = new QMenu("Connected devices");
    connectedDevicesMenu->setStyleSheet("background-color: rgb(186, 59, 10);");
    ui->menuDevices->addMenu(connectedDevicesMenu);


    connect(ui->actionAddSingleDevice, &QAction::triggered, this,  &OpenEPT::onActionAddSingleDeviceTriggered);
    connect(ui->actionDataAnalyzer, &QAction::triggered, this,  &OpenEPT::onActionOpenAndProcessData);


 }

OpenEPT::~OpenEPT()
{
    onDeviceContainerAllDeviceWndClosed();
    delete ui;
}

void OpenEPT::onActionAddSingleDeviceTriggered()
{
    addDeviceWnd->show();
}

void OpenEPT::onAddDeviceWndAddDevice(QString aIpAddress, QString aPort)
{
    if(addNewDevice(aIpAddress, aPort))
    {
        msgBox.setText("Device sucessfully added");
        msgBox.exec();
    }
    else
    {
        msgBox.setText("Unable to add device");
        msgBox.exec();
    }
}


bool OpenEPT::addNewDevice(QString aIpAddress, QString aPort)
{
    QString deviceName;

    /* Create control link and try to access device*/
    ControlLink* tmpControlLink = new ControlLink();

    /* Try to establish connection with device*/
    if(tmpControlLink->establishLink(aIpAddress, aPort) != CONTROL_LINK_STATUS_ESTABLISHED)
    {
        delete tmpControlLink;
        return false;
    }

    if(!tmpControlLink->getDeviceName(&deviceName))
    {
        delete tmpControlLink;
        return false;
    }


    /* Create device */
    Device  *tmpDevice = new Device();
    tmpDevice->setName(deviceName);
    tmpDevice->controlLinkAssign(tmpControlLink);

    /* Create corresponding device window*/
    DeviceWnd *tmpdeviceWnd = new DeviceWnd(0);
    tmpdeviceWnd->setWindowTitle(deviceName);
    tmpdeviceWnd->setDeviceState(DEVICE_STATE_CONNECTED);

    /* Create device container */
    DeviceContainer *tmpDeviceContainer = new DeviceContainer(NULL,tmpdeviceWnd,tmpDevice);

    connect(tmpDeviceContainer, SIGNAL(sigDeviceClosed(Device*)), this, SLOT(onDeviceContainerDeviceWndClosed(Device*)));

    /* Add device to menu bar */
    QAction* tmpDeviceAction = new QAction(deviceName);
    connectedDevicesMenu->addAction(tmpDeviceAction);

    // Add the child window to the MDI area
    QMdiSubWindow *subWindow = ui->mdiArea->addSubWindow(tmpdeviceWnd);
    subWindow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    subWindow->show();

    deviceList.append(tmpDeviceContainer);

    setTheme();

    return true;
}

void OpenEPT::setTheme()
{
    //setStyleSheet("color:white;background-color:#404241;border-color:#404241");
}


void OpenEPT::onDeviceContainerDeviceWndClosed(Device *aDevice)
{
    QString name;
    aDevice->getName(&name);
    QList<QAction*> actionList = connectedDevicesMenu->actions();
    for(int i = 0; i < actionList.size(); i++)
    {
        if(actionList[i]->text() == name)
        {
            connectedDevicesMenu->removeAction(actionList[i]);
            deviceList.removeAt(i);
            delete aDevice;
            return;
        }
    }

}

void OpenEPT::onDeviceContainerAllDeviceWndClosed()
{
    DeviceContainer* tmpDeviceContainer;
    QList<QAction*> actionList = connectedDevicesMenu->actions();
    for(int i = 0; i < actionList.size(); i++)
    {
        connectedDevicesMenu->removeAction(actionList[i]);
        tmpDeviceContainer = deviceList.at(i);
        deviceList.removeAt(i);
        delete tmpDeviceContainer;
    }
}

void OpenEPT::onActionOpenAndProcessData()
{
    dataAnalyzerWnd->show();
}



