#ifndef SELECTWORKSPACE_H
#define SELECTWORKSPACE_H

#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QStandardPaths>

namespace Ui {
class SelectWorkspace;
}

class SelectWorkspace : public QDialog
{
    Q_OBJECT

public:
    explicit SelectWorkspace(QWidget *parent = nullptr);
    QString  getWSPath();
    ~SelectWorkspace();

public slots:

    void    onAccept();
    void    onReject();
    void    onSelectWorkingDir();

private:
    Ui::SelectWorkspace *ui;

    QLineEdit           *workspacePathLine;

    QString             workspacePath;
};

#endif // SELECTWORKSPACE_H
