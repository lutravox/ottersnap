#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QStyleOption>

#include <QtConcurrent>
#include "ui/dialogs/snapshotmanagerdialog.h"
#include "core/snapshotdb.h"
#include "core/snapshotmanager.h"
#include "core/thumbnailmanager.h"
#include "ui/dialogutils.h"

SnapshotItem::SnapshotItem(int            index,
                           const QString& filename,
                           const QString& fullPath,
                           int            count,
                           const QImage&  thumb,
                           QWidget       *parent)
    : QWidget(parent), m_index(index) {
    setToolTip(fullPath);
    setObjectName("snapshotItem");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(2);

    auto *imgLabel = new QLabel();
    imgLabel->setFixedSize(80, 80);
    imgLabel->setPixmap(
        QPixmap::fromImage(thumb).scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    imgLabel->setAlignment(Qt::AlignCenter);

    auto *textContainer = new QWidget();
    auto *textLayout = new QVBoxLayout(textContainer);
    textLayout->setContentsMargins(0, 2, 0, 0);
    textLayout->setSpacing(0);

    auto *nameLabel = new QLabel();
    nameLabel->setObjectName("nameLabel");
    nameLabel->setAlignment(Qt::AlignCenter);

    QFontMetrics fm(nameLabel->font());
    int          availableWidth = 120 - 2 * 5;
    nameLabel->setText(fm.elidedText(filename, Qt::ElideRight, availableWidth));

    auto *countLabel = new QLabel(QString("(%1 snapshots)").arg(count));
    countLabel->setObjectName("countLabel");
    countLabel->setAlignment(Qt::AlignCenter);

    textLayout->addWidget(nameLabel);
    textLayout->addWidget(countLabel);

    layout->addWidget(imgLabel, 0, Qt::AlignCenter);
    layout->addWidget(textContainer);

    setFixedSize(120, 130);
}

void SnapshotItem::setSelected(bool selected) {
    setProperty("selected", selected);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void SnapshotItem::paintEvent(QPaintEvent *event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void SnapshotItem::mousePressEvent(QMouseEvent *event) {
    emit clicked(m_index);
    QWidget::mousePressEvent(event);
}

SnapshotManagerDialog::SnapshotManagerDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Manage Snapshots"));
    setMinimumSize(600, 500);

    QFile   qssFile(":/qss/snapshotmanagerdialog.qss");
    QString qssContent;
    if (qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qssContent = qssFile.readAll();
        this->setStyleSheet(qssContent);
    }

    auto *layout = new QHBoxLayout(this);

    // Column 1: Images Grid
    auto *imagesVBox = new QVBoxLayout();

    m_imageScrollArea = new QScrollArea();
    m_imageScrollArea->setFrameShape(QFrame::NoFrame);
    m_imageScrollArea->setWidgetResizable(true);
    m_imageGridWidget = new QWidget();
    m_imageGridWidget->setObjectName("imageGridWidget");
    m_imageGrid = new QGridLayout(m_imageGridWidget);
    m_imageGrid->setSpacing(10);
    m_imageGrid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_imageScrollArea->setWidget(m_imageGridWidget);

    imagesVBox->addWidget(m_imageScrollArea);
    layout->addLayout(imagesVBox, 3);

    // Column 2: Details & Actions
    auto *detailsVBox = new QVBoxLayout();
    detailsVBox->setAlignment(Qt::AlignTop);
    detailsVBox->setContentsMargins(10, 0, 10, 0);

    auto *titleLabel = new QLabel(tr("Image Details"));
    titleLabel->setObjectName("detailsTitle");
    titleLabel->setAlignment(Qt::AlignLeft);
    detailsVBox->addWidget(titleLabel);
    detailsVBox->addSpacing(10);

    m_pathLabel = new QLabel("-");
    m_pathLabel->setObjectName("pathLabel");
    m_pathLabel->setWordWrap(true);
    detailsVBox->addWidget(m_pathLabel);

    m_countLabel = new QLabel(tr("Snapshots: -"));
    m_storageLabel = new QLabel(tr("Total Storage: -"));
    m_countLabel->setObjectName("countLabelDetail");
    m_storageLabel->setObjectName("storageLabelDetail");

    detailsVBox->addWidget(m_countLabel);
    detailsVBox->addWidget(m_storageLabel);
    detailsVBox->addStretch();

    m_btnOpenInViewer = new QPushButton(tr("Open"));
    m_btnClearImage = new QPushButton(tr("Delete All Snapshots"));

    m_btnOpenInViewer->setEnabled(false);
    m_btnClearImage->setEnabled(false);

    detailsVBox->addWidget(m_btnOpenInViewer);
    detailsVBox->addWidget(m_btnClearImage);

    layout->addLayout(detailsVBox, 1);

    connect(m_btnClearImage, &QPushButton::clicked, this, &SnapshotManagerDialog::onClearImage);
    connect(m_btnOpenInViewer, &QPushButton::clicked, this, &SnapshotManagerDialog::onOpenInViewer);

    updateImageGrid();
}

void SnapshotManagerDialog::updateImageGrid() {
    m_selectedItem = nullptr;
    m_selectedImageIndex = -1;
    m_btnOpenInViewer->setEnabled(false);
    m_btnClearImage->setEnabled(false);

    while (m_imageGrid->count()) {
        auto *item = m_imageGrid->takeAt(0);
        if (item->widget())
            delete item->widget();
        delete item;
    }

    auto images = SnapshotDatabase::instance().getAllSnapshottedImages();
    int  colCount = 3;

    for (int i = 0; i < static_cast<int>(images.size()); ++i) {
        const auto& img = images[i];
        auto        snapshots = SnapshotDatabase::instance().getSnapshots(img.key);

        QImage thumb =
            ThumbnailManager::instance().getThumbnail(0, 128, img.path, false, snapshots);

        if (thumb.isNull()) {
            thumb = QImage(128, 128, QImage::Format_RGB32);
            thumb.fill(Qt::darkGray);
        }

        QString filename = QFileInfo(img.path).fileName();
        int     count = snapshots.size();
        auto   *item = new SnapshotItem(i, filename, img.path, count, thumb, this);

        connect(item, &SnapshotItem::clicked, this, &SnapshotManagerDialog::onImageSelected);

        m_imageGrid->addWidget(item, i / colCount, i % colCount, Qt::AlignCenter);
    }
}

void SnapshotManagerDialog::onImageSelected(int index) {
    if (m_selectedItem) {
        m_selectedItem->setSelected(false);
        m_selectedItem = nullptr;
    }

    m_selectedImageIndex = index;

    auto images = SnapshotDatabase::instance().getAllSnapshottedImages();
    if (index < 0 || index >= static_cast<int>(images.size()))
        return;

    // Find the corresponding item in the grid to mark as selected
    for (int i = 0; i < m_imageGrid->count(); ++i) {
        auto *item = m_imageGrid->itemAt(i)->widget();
        if (auto *snapItem = dynamic_cast<SnapshotItem *>(item)) {
            if (snapItem->index() == index) {
                snapItem->setSelected(true);
                m_selectedItem = snapItem;
                break;
            }
        }
    }

    updateDetails();
    m_btnOpenInViewer->setEnabled(true);
    m_btnClearImage->setEnabled(true);
}

void SnapshotManagerDialog::updateDetails() {
    auto images = SnapshotDatabase::instance().getAllSnapshottedImages();
    if (m_selectedImageIndex < 0 || m_selectedImageIndex >= static_cast<int>(images.size())) {
        m_pathLabel->setText("-");
        m_countLabel->setText(tr("Snapshots: -"));
        m_storageLabel->setText(tr("Total Storage: -"));
        m_btnOpenInViewer->setEnabled(false);
        m_btnClearImage->setEnabled(false);
        return;
    }

    const auto& img = images[m_selectedImageIndex];
    auto        snapshots = SnapshotDatabase::instance().getSnapshots(img.key);

    m_pathLabel->setText(img.path);
    m_countLabel->setText(tr("Snapshots: %1").arg(snapshots.size()));

    qint64  bytes = SnapshotManager::calculateStorageUsage(img.path);
    double  size = bytes;
    QString unit = "B";
    while (size >= 1024 && unit != "TB") {
        size /= 1024;
        if (unit == "B")
            unit = "KB";
        else if (unit == "KB")
            unit = "MB";
        else if (unit == "MB")
            unit = "GB";
        else
            unit = "TB";
    }
    m_storageLabel->setText(
        tr("Total Storage: %1 %2").arg(QString::number(size, 'f', 2)).arg(unit));
}

void SnapshotManagerDialog::onClearImage() {
    auto images = SnapshotDatabase::instance().getAllSnapshottedImages();
    if (m_selectedImageIndex < 0 || m_selectedImageIndex >= static_cast<int>(images.size()))
        return;

    const auto& img = images[m_selectedImageIndex];

    if (DialogUtils::confirm(this,
                             tr("Delete All Snapshots"),
                             tr("Are you sure you want to delete ALL snapshots for this image?\nThis "
                                "action cannot be undone."),
                             tr("Delete All"),
                             tr("Cancel"))) {
        const QString path = img.path;
        const QString key = img.key;

        m_btnClearImage->setEnabled(false);
        QApplication::setOverrideCursor(Qt::WaitCursor);
        auto    watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QObject::destroyed, []() { QApplication::restoreOverrideCursor(); });
        connect(watcher, &QFutureWatcher<bool>::finished, [this, watcher, path, key]() {
            bool ok = watcher->result();
            watcher->deleteLater();

            if (ok) {
                SnapshotDatabase::instance().clearImage(key);

                emit snapshotChanged(path);
                updateImageGrid();
                updateDetails();
            }
        });
        watcher->setFuture(QtConcurrent::run([path]() {
            SnapshotManager::deleteAllSnapshots(path);
            return true;
        }));
    }
}

void SnapshotManagerDialog::onOpenInViewer() {
    auto images = SnapshotDatabase::instance().getAllSnapshottedImages();
    if (m_selectedImageIndex < 0 || m_selectedImageIndex >= static_cast<int>(images.size()))
        return;

    const auto& img = images[m_selectedImageIndex];
    auto        snapshots = SnapshotDatabase::instance().getSnapshots(img.key);

    if (snapshots.isEmpty())
        return;

    // Open the latest snapshot (the last one in the list)
    emit openSnapshotRequested(img.path, snapshots.last().uuid);
}
