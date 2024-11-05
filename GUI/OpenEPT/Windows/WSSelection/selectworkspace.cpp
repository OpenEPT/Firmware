#include "selectworkspace.h"
#include "ui_selectworkspace.h"

SelectWorkspace::SelectWorkspace(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SelectWorkspace)
{
    ui->setupUi(this);

    QFont defaultFont("Arial", 10); // Set desired font and size
    setFont(defaultFont);

    setWindowTitle("Select working directory");

    QVBoxLayout *mainVL = new QVBoxLayout(this);

    // Create ws label
    QHBoxLayout *wsLabelHB = new QHBoxLayout();

    QLabel *wsSelectLabe = new QLabel("Select wrokspace directory", this);
    wsSelectLabe->setFixedSize(210, 30);
    wsSelectLabe->setFont(defaultFont);

    wsLabelHB->addWidget(wsSelectLabe);
    wsLabelHB->addStretch();

    /**/
    QHBoxLayout *wslineHB = new QHBoxLayout();

    workspacePathLine = new QLineEdit(this);
    workspacePathLine->setFixedSize(180, 30);
    workspacePathLine->setFont(defaultFont);

    QPushButton *selecWSDirPushb = new QPushButton();

    QPixmap selecWSDirPng(":/images/NewSet/directory.png");
    QIcon selecWSDirIcon(selecWSDirPng);
    selecWSDirPushb->setIcon(selecWSDirIcon);
    selecWSDirPushb->setIconSize(QSize(30,30));
    selecWSDirPushb->setToolTip("Select working directory");
    selecWSDirPushb->setFixedSize(30, 30);



    connect(selecWSDirPushb, SIGNAL(clicked(bool)), this, SLOT(onSelectWorkingDir()));

    wslineHB->addWidget(workspacePathLine);
    wslineHB->addWidget(selecWSDirPushb);
    wslineHB->addStretch();


    // Create the button box with standard OK and Cancel buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    // Connect the accepted and rejected signals to handle the buttons
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(onAccept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(onReject()));


    /**/
    QHBoxLayout *wsDialogButtonsHB = new QHBoxLayout();
    wsDialogButtonsHB->addWidget(buttonBox);

    mainVL->addLayout(wsLabelHB);
    mainVL->addLayout(wslineHB);
    mainVL->addLayout(wsDialogButtonsHB);
    mainVL->addStretch();


    QPixmap windowPng(":/images/main.png");
    QIcon windowIcon(windowPng);

    setWindowIcon(windowIcon);

    setFixedSize(250, 130);

}

QString SelectWorkspace::getWSPath()
{
    return workspacePath;
}

SelectWorkspace::~SelectWorkspace()
{
    delete ui;
}

void SelectWorkspace::onAccept()
{
    accept();
}

void SelectWorkspace::onReject()
{
    reject();
}

void SelectWorkspace::onSelectWorkingDir()
{
    QString directory = QFileDialog::getExistingDirectory(
        nullptr,
        "Select Directory",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

    workspacePath = directory;

    workspacePathLine->setText(workspacePath);



}
