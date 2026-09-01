#include "ui/dialogs/aboutdialog.h"
#include "config/appsettings.h"
#include "config/version.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFont>
#include <QPixmap>
#include <QPushButton>
#include <QUrl>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("About %1").arg(AppSettings::applicationName()));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);
    layout->setAlignment(Qt::AlignCenter);

    auto *iconLabel = new QLabel(this);
    QPixmap icon(":/icons/ottersnap.svg");
    iconLabel->setPixmap(icon.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAlignment(Qt::AlignCenter);

    auto *nameLabel = new QLabel(AppSettings::applicationName(), this);
    nameLabel->setAlignment(Qt::AlignCenter);
    QFont nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(18);
    nameLabel->setFont(nameFont);

    auto *versionLabel = new QLabel(
        tr("Version %1%2").arg(VERSION_STRING).arg(VERSION_SUFFIX), this);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet("color: gray;");

    auto *descLabel = new QLabel(
        tr("A high-performance, otter-friendly tool for capturing and browsing image snapshots."),
        this);
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);

    auto *linkLabel = new QLabel(
        tr("<a href='%1'>GitHub Repository</a>").arg(AppSettings::repositoryUrl()), this);
    linkLabel->setAlignment(Qt::AlignCenter);
    linkLabel->setOpenExternalLinks(true);
    linkLabel->setCursor(Qt::PointingHandCursor);

    auto *closeButton = new QPushButton(tr("Close"), this);
    closeButton->setFixedWidth(100);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    layout->addWidget(iconLabel, 0, Qt::AlignCenter);
    layout->addWidget(nameLabel);
    layout->addWidget(versionLabel);
    layout->addSpacing(10);
    layout->addWidget(descLabel);
    layout->addWidget(linkLabel);
    layout->addStretch();
    layout->addWidget(closeButton, 0, Qt::AlignCenter);

    adjustSize();
}
