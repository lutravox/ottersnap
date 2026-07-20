#include "ui/snapshottimeline.h"

#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
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
    setFixedHeight(c_thumbnailSize + 8);

    {
        QFile qss(":/qss/snapshottimeline.qss");
        if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
            setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    m_scrollTimer.setSingleShot(true);
    connect(&m_scrollTimer, &QTimer::timeout, this, &SnapshotTimeline::doScrollToCurrent);
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
    if (oldIndex >= 0 && oldIndex < static_cast<int>(m_snapshottabs.size()))
        m_snapshottabs[oldIndex]->setSelected(false);

    if (newIndex >= 0 && newIndex < static_cast<int>(m_snapshottabs.size())) {
        SnapshotTab *current = m_snapshottabs[newIndex];
        current->setSelected(true);
        m_scrollArea->horizontalScrollBar()->setValue(
            current->x() - (m_scrollArea->viewport()->width() / 2) + (c_thumbnailSize / 2));
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

    for (int i = 0; i < thumbnails.size(); ++i) {
        SnapshotTab *lbl = new SnapshotTab(m_contentWidget);

        lbl->setFixedSize(c_thumbnailSize, c_thumbnailSize);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setPixmap(thumbnails[i]);

        // Highlight the current thumbnail
        lbl->setSelected(i == m_currentIndex);

        lbl->setToolTip(m_labels.isEmpty() ? QString("v%1").arg(i + 1) : m_labels[i]);

        int idx = i;
        connect(lbl, &SnapshotTab::clicked, this, [this, idx]() { emit snapshotSelected(idx); });

        m_snapshottabs.append(lbl);
        m_contentLayout->addWidget(lbl);
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
    SnapshotTab *current = m_snapshottabs[m_currentIndex];
    if (!current)
        return;
    int targetX = current->x() - (m_scrollArea->viewport()->width() / 2) + (c_thumbnailSize / 2);
    m_scrollArea->horizontalScrollBar()->setValue(targetX);
}
