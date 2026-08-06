#include "ui/dialogutils.h"
#include <QMessageBox>
#include <QFile>
#include <QPushButton>
#include <QAbstractButton>

bool DialogUtils::confirm(QWidget *parent,
                          const QString& title,
                          const QString& text,
                          const QString& yesText,
                          const QString& noText) {
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Warning);

    QFile qssFile(":/qss/messagebox.qss");
    if (qssFile.open(QIODevice::ReadOnly)) {
        msgBox.setStyleSheet(qssFile.readAll());
    }

    QPushButton *yesButton = msgBox.addButton(yesText, QMessageBox::AcceptRole);
    QPushButton *noButton = msgBox.addButton(noText, QMessageBox::RejectRole);

    msgBox.setDefaultButton(noButton);
    msgBox.exec();

    return msgBox.clickedButton() == static_cast<QAbstractButton*>(yesButton);
}
