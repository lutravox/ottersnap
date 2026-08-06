#pragma once

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>

/**
 * @class AboutDialog
 * @brief A dialog that displays information about the application.
 */
class AboutDialog : public QDialog {
    Q_OBJECT

  public:
    explicit AboutDialog(QWidget *parent = nullptr);
};
