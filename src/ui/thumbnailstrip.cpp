#include "ui/thumbnailstrip.h"

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

ThumbnailStrip::ThumbnailStrip(QWidget *parent) : QWidget(parent) {
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
        QFile qss(":/qss/thumbnailstrip.qss");
        if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
            setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    m_scrollTimer.setSingleShot(true);
    connect(&m_scrollTimer, &QTimer::timeout, this, &ThumbnailStrip::doScrollToCurrent);
}

void ThumbnailStrip::wheelEvent(QWheelEvent *event) {
    if (m_thumbnailWidgets.isEmpty())
        return;

    int step = event->angleDelta().y() > 0 ? -1 : 1;
    int next = qBound(0, m_currentIndex + step, static_cast<int>(m_thumbnailWidgets.size() - 1));
    if (next != m_currentIndex) {
        int oldIndex = m_currentIndex;
        m_currentIndex = next;
        updateSelection(oldIndex, next);
        emit thumbnailSelected(next);
    }

    event->accept();
}

void ThumbnailStrip::setThumbnails(const QVector<QPixmap>& thumbnails,
                                   const QVector<QString>& labels) {
    m_labels = labels;

    buildStrip(thumbnails);
}

void ThumbnailStrip::setSelectedIndex(int index) {
    if (m_thumbnailWidgets.isEmpty())
        return;

    int clamped = qBound(0, index, static_cast<int>(m_thumbnailWidgets.size() - 1));
    if (clamped == m_currentIndex)
        return;

    int oldIndex = m_currentIndex;
    m_currentIndex = clamped;
    updateSelection(oldIndex, clamped);
}

bool ThumbnailStrip::isEmpty() const {
    return m_thumbnailWidgets.isEmpty();
}

void ThumbnailStrip::updateSelection(int oldIndex, int newIndex) {
    if (oldIndex >= 0 && oldIndex < static_cast<int>(m_thumbnailWidgets.size()))
        m_thumbnailWidgets[oldIndex]->setSelected(false);

    if (newIndex >= 0 && newIndex < static_cast<int>(m_thumbnailWidgets.size())) {
        ThumbnailButton *current = m_thumbnailWidgets[newIndex];
        current->setSelected(true);
        m_scrollArea->horizontalScrollBar()->setValue(
            current->x() - (m_scrollArea->viewport()->width() / 2) + (c_thumbnailSize / 2));
    }
}

void ThumbnailStrip::buildStrip(const QVector<QPixmap>& thumbnails) {
    // Cancel pending scroll timer — widgets below will be deleted
    m_scrollTimer.stop();

    // Remove old widgets
    QLayoutItem *item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (QWidget *widget = item->widget()) {
            delete widget; // Deletes ThumbnailButton
        }
        delete item; // Deletes layout/stretch spacers
    }
    m_thumbnailWidgets.clear();

    for (int i = 0; i < thumbnails.size(); ++i) {
        ThumbnailButton *lbl = new ThumbnailButton(m_contentWidget);

        lbl->setFixedSize(c_thumbnailSize, c_thumbnailSize);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setPixmap(thumbnails[i]);

        // Highlight the current thumbnail
        lbl->setSelected(i == m_currentIndex);

        lbl->setToolTip(m_labels.isEmpty() ? QString("v%1").arg(i + 1) : m_labels[i]);

        int idx = i;
        connect(
            lbl, &ThumbnailButton::clicked, this, [this, idx]() { emit thumbnailSelected(idx); });

        m_thumbnailWidgets.append(lbl);
        m_contentLayout->addWidget(lbl);
    }

    // adjust to fit container
    m_contentLayout->addStretch(1);
    m_contentWidget->adjustSize();

    // Scroll to center the current thumbnail (deferred so layout is settled)
    if (m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_thumbnailWidgets.size())) {
        m_scrollTimer.start(0);
    }
}

void ThumbnailStrip::doScrollToCurrent() {
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_thumbnailWidgets.size()))
        return;
    ThumbnailButton *current = m_thumbnailWidgets[m_currentIndex];
    if (!current)
        return;
    int targetX = current->x() - (m_scrollArea->viewport()->width() / 2) + (c_thumbnailSize / 2);
    m_scrollArea->horizontalScrollBar()->setValue(targetX);
}
