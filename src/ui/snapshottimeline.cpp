#include "ui/snapshottimeline.h"

#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

constexpr int c_thumbnailSize = 48;
constexpr int c_thumbnailSpacing = 3;

SnapshotTimeline::SnapshotTimeline(QWidget *parent) : QWidget(parent) {
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_scrollArea->viewport()->installEventFilter(this);

    m_contentWidget = new QWidget(m_scrollArea);

    m_contentLayout = new QHBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(6, 3, 6, 3);
    m_contentLayout->setSpacing(c_thumbnailSpacing);
    m_contentLayout->setAlignment(Qt::AlignLeft);

    m_scrollArea->setWidget(m_contentWidget);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_scrollArea);

    // Keep the strip to a compact, fixed height
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(c_thumbnailSize + 24);

    {
        QFile qss(":/qss/snapshottimeline.qss");
        if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
            setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    m_scrollTimer.setSingleShot(true);
    connect(&m_scrollTimer, &QTimer::timeout, this, &SnapshotTimeline::doScrollToCurrent);
}

bool SnapshotTimeline::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_scrollArea->viewport() && event->type() == QEvent::Wheel) {
        wheelEvent(static_cast<QWheelEvent *>(event));
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void SnapshotTimeline::wheelEvent(QWheelEvent *event) {
    if (m_snapshottabs.isEmpty())
        return;

    int step = event->angleDelta().y() > 0 ? -1 : 1;
    int next = qBound(0, m_currentIndex + step, static_cast<int>(m_snapshottabs.size() - 1));
    if (next != m_currentIndex) {
        int oldIndex = m_currentIndex;
        m_currentIndex = next;
        updateSelection(oldIndex, next);
        emit snapshotSelected(next);
    }

    event->accept();
}

void SnapshotTimeline::setThumbnails(const QVector<QPixmap>& thumbnails,
                                     const QVector<QString>& labels) {
    m_labels = labels;

    buildStrip(thumbnails);
}

void SnapshotTimeline::setSelectedIndex(int index) {
    if (m_snapshottabs.isEmpty())
        return;

    int clamped = qBound(0, index, static_cast<int>(m_snapshottabs.size() - 1));
    if (clamped == m_currentIndex)
        return;

    int oldIndex = m_currentIndex;
    m_currentIndex = clamped;
    updateSelection(oldIndex, clamped);
}

bool SnapshotTimeline::isEmpty() const {
    return m_snapshottabs.isEmpty();
}

void SnapshotTimeline::updateSelection(int oldIndex, int newIndex) {
    updateTabState(oldIndex, false);
    updateTabState(newIndex, true);

    if (newIndex >= 0 && newIndex < static_cast<int>(m_containers.size())) {
        QWidget *container = m_containers[newIndex];
        m_scrollArea->horizontalScrollBar()->setValue(
            container->x() - (m_scrollArea->viewport()->width() / 2) + (c_thumbnailSize / 2));
    }
}

void SnapshotTimeline::updateTabState(int index, bool selected) {
    if (index < 0 || index >= static_cast<int>(m_snapshottabs.size()))
        return;

    m_snapshottabs[index]->setSelected(selected);

    if (index < static_cast<int>(m_snapshotLabels.size())) {
        QLabel *label = m_snapshotLabels[index];
        label->setText(index == static_cast<int>(m_snapshottabs.size()) - 1
                           ? "C"
                           : QString::number(index + 1));
        label->setProperty("selected", selected ? "true" : "false");
        label->style()->unpolish(label);
        label->style()->polish(label);
    }
}

void SnapshotTimeline::buildStrip(const QVector<QPixmap>& thumbnails) {
    // Cancel pending scroll timer — widgets below will be deleted
    m_scrollTimer.stop();

    // Remove old widgets
    QLayoutItem *item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (QWidget *widget = item->widget()) {
            delete widget; // Deletes SnapshotTab
        }
        delete item; // Deletes layout/stretch spacers
    }
    m_snapshottabs.clear();
    m_snapshotLabels.clear();
    m_containers.clear();

    for (int i = 0; i < thumbnails.size(); ++i) {
        QWidget *container = new QWidget(m_contentWidget);
        container->setFixedWidth(c_thumbnailSize);
        container->setFixedHeight(c_thumbnailSize + 20);

        QLabel *snapshotLabel = new QLabel(container);
        snapshotLabel->setObjectName("snapshotLabel");
        snapshotLabel->setAlignment(Qt::AlignCenter);
        snapshotLabel->setText(i == thumbnails.size() - 1 ? "C" : QString::number(i + 1));
        snapshotLabel->setGeometry(0, 0, c_thumbnailSize, 16);

        bool isSelected = (i == m_currentIndex);
        snapshotLabel->setProperty("selected", isSelected ? "true" : "false");

        SnapshotTab *lbl = new SnapshotTab(container);
        lbl->setFixedSize(c_thumbnailSize, c_thumbnailSize);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setPixmap(thumbnails[i]);
        lbl->setGeometry(0, 16, c_thumbnailSize, c_thumbnailSize);
        lbl->setContentsMargins(0, 0, 0, 0);

        // Select the current thumbnail
        lbl->setSelected(isSelected);

        lbl->setToolTip(m_labels.isEmpty() ? QString("v%1").arg(i + 1) : m_labels[i]);

        int idx = i;
        connect(lbl, &SnapshotTab::clicked, this, [this, idx]() { emit snapshotSelected(idx); });

        m_snapshottabs.append(lbl);
        m_snapshotLabels.append(snapshotLabel);
        m_containers.append(container);
        m_contentLayout->addWidget(container);
    }

    // adjust to fit container
    m_contentLayout->addStretch(1);
    m_contentWidget->adjustSize();

    // Scroll to center the current thumbnail (deferred so layout is settled)
    if (m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_snapshottabs.size())) {
        m_scrollTimer.start(0);
    }
}

void SnapshotTimeline::doScrollToCurrent() {
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_snapshottabs.size()))
        return;
    if (m_currentIndex >= static_cast<int>(m_containers.size()))
        return;
    QWidget *container = m_containers[m_currentIndex];
    if (!container)
        return;
    int targetX = container->x() - (m_scrollArea->viewport()->width() / 2) + (c_thumbnailSize / 2);
    m_scrollArea->horizontalScrollBar()->setValue(targetX);
}
