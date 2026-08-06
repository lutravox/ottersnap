#include "ui/snapshottimeline.h"

#include <QAbstractItemView>
#include <QAction>
#include <QEvent>
#include <QFile>
#include <QFont>
#include <QHBoxLayout>
#include <QHoverEvent>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPushButton>
#include <QWheelEvent>

constexpr int c_thumbnailSize = 48;

SnapshotTimeline::SnapshotTimeline(QWidget *parent) : QWidget(parent) {
    m_model = new SnapshotModel(this);
    m_delegate = new SnapshotTimelineDelegate(this);

    m_listView = new QListView(this);
    m_listView->setModel(m_model);
    m_listView->setItemDelegate(m_delegate);

    // Configure for horizontal strip look
    m_listView->setFlow(QListView::LeftToRight);
    m_listView->setWrapping(false);
    m_listView->setMovement(QListView::Static);
    m_listView->setSpacing(3);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setSelectionMode(QAbstractItemView::NoSelection);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_listView->setAttribute(Qt::WA_Hover);
    m_listView->setMouseTracking(true);

    auto *viewport = m_listView->viewport();
    viewport->setAttribute(Qt::WA_Hover);
    viewport->setMouseTracking(true);
    viewport->setAutoFillBackground(true);
    viewport->installEventFilter(this);

    m_listView->installEventFilter(this);
    m_listView->setAutoFillBackground(true);

    connect(m_listView, &QListView::clicked, this, [this](const QModelIndex& index) {
        int row = index.row();
        if (row == m_currentIndex)
            return;
        int oldIndex = m_currentIndex;
        m_currentIndex = row;
        updateSelection(row);
        emit snapshotSelected(row);
    });

    m_createButton = new QPushButton(tr("+"), this);
    m_createButton->setObjectName("createSnapshotButton");
    m_createButton->setFixedHeight(c_thumbnailSize + 16);
    m_createButton->setFixedWidth(28);
    QFont font = m_createButton->font();
    font.setBold(true);
    m_createButton->setFont(font);
    m_createButton->setToolTip(tr("Create a new snapshot"));
    connect(
        m_createButton, &QPushButton::clicked, this, &SnapshotTimeline::createSnapshotRequested);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stripContainer = new QWidget(this);
    auto *stripLayout = new QHBoxLayout(m_stripContainer);
    stripLayout->setContentsMargins(c_startPadding, 0, 0, 0);
    stripLayout->setSpacing(0);
    stripLayout->addWidget(m_listView, 1);

    root->addWidget(m_stripContainer, 0);

    // m_createButton is positioned manually in resizeEvent for edge clipping
    m_createButton->setParent(m_stripContainer);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    int baseH = m_delegate->sizeHint(QStyleOptionViewItem(), QModelIndex()).height();
    setFixedHeight(baseH);

    // Load timeline stylesheet
    QFile qss(":/qss/snapshottimeline.qss");
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString styleSheet = QString::fromUtf8(qss.readAll());
        setStyleSheet(styleSheet);
        m_listView->setStyleSheet(styleSheet);
        m_listView->viewport()->setStyleSheet(styleSheet);
    }
}

bool SnapshotTimeline::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_listView->viewport() || obj == m_listView) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            QModelIndex  index = m_listView->indexAt(me->pos());
            int          newHoverIndex = index.isValid() ? index.row() : -1;
            m_delegate->setHoverIndex(newHoverIndex);
            m_listView->viewport()->update();
        } else if (event->type() == QEvent::HoverMove) {
            QHoverEvent *he = static_cast<QHoverEvent *>(event);
            QModelIndex  index = m_listView->indexAt(he->position().toPoint());
            int          newHoverIndex = index.isValid() ? index.row() : -1;
            m_delegate->setHoverIndex(newHoverIndex);
            m_listView->viewport()->update();
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave) {
            m_delegate->setHoverIndex(-1);
            m_listView->viewport()->update();
        } else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);

            // 1. Clear "new" status immediately on any left-click press to remove the highlight
            // border
            if (me->button() == Qt::LeftButton) {
                QModelIndex index = m_listView->indexAt(me->pos());
                if (index.isValid()) {
                    int snapshotIdx = m_model->data(index, SnapshotModel::IndexRole).toInt();
                    m_model->clearNewStatus(snapshotIdx);
                    m_listView->viewport()->update();
                }
            }

            if (me->button() == Qt::RightButton) {
                QModelIndex index = m_listView->indexAt(me->pos());
                if (index.isValid()) {
                    int row = index.row();
                    // Don't allow deleting the current disk image (the last tab)
                    // In snapshot-only mode, all items are deletable.
                    if (m_model->isSnapshotOnly() || row < m_model->rowCount() - 1) {
                        QMenu    menu(this);
                        QAction *deleteAct = menu.addAction(tr("Delete Snapshot"));
                        connect(deleteAct, &QAction::triggered, this, [this, row]() {
                            emit snapshotDeletionRequested(row);
                        });
                        menu.exec(me->globalPosition().toPoint());
                        return true;
                    }
                }
            }
        } else if (event->type() == QEvent::Wheel) {
            QWheelEvent *we = static_cast<QWheelEvent *>(event);
            if (m_model->rowCount() == 0) {
                return QWidget::eventFilter(obj, event);
            }

            int delta = we->angleDelta().y();
            if (delta == 0) {
                return QWidget::eventFilter(obj, event);
            }

            int step = delta > 0 ? -1 : 1;
            int next = qBound(0, m_currentIndex + step, m_model->rowCount() - 1);
            if (next != m_currentIndex) {
                int oldIndex = m_currentIndex;
                m_currentIndex = next;
                updateSelection(next);
                emit snapshotSelected(next);
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void SnapshotTimeline::setThumbnails(const QVector<QPixmap>& thumbnails,
                                     const QVector<QString>& labels,
                                     const QVector<int>&     indices) {
    m_model->setThumbnails(thumbnails, labels, indices);
}

void SnapshotTimeline::markSnapshotAsNew(int snapshotIndex) {
    m_model->markSnapshotAsNew(snapshotIndex);
}

void SnapshotTimeline::setSelectedIndex(int index) {
    if (m_model->rowCount() == 0)
        return;

    int clamped = qBound(0, index, m_model->rowCount() - 1);
    if (clamped == m_currentIndex)
        return;

    int oldIndex = m_currentIndex;
    m_currentIndex = clamped;
    updateSelection(clamped);
    m_listView->setCurrentIndex(m_model->index(clamped));
}

void SnapshotTimeline::setCreateButtonEnabled(bool enabled) {
    m_createButton->setEnabled(enabled);
}

bool SnapshotTimeline::isEmpty() const {
    return m_model->rowCount() == 0;
}

void SnapshotTimeline::updateSelection(int newIndex) {
    // We can use the model to clear the "new" status
    if (newIndex >= 0 && newIndex < m_model->rowCount()) {
        int snapshotIdx = m_model->data(m_model->index(newIndex), SnapshotModel::IndexRole).toInt();
        m_model->clearNewStatus(snapshotIdx);
    }

    // Update the delegate's current index for custom rendering
    if (m_delegate) {
        m_delegate->setCurrentIndex(newIndex);
    }

    // Scroll to center the current thumbnail
    if (newIndex >= 0 && newIndex < m_model->rowCount()) {
        m_listView->setCurrentIndex(m_model->index(newIndex));
    }

    // Force a viewport update to clear any painting artifacts (e.g., the "new" highlight
    // border)
    m_listView->viewport()->update();
}

void SnapshotTimeline::updateThumbnail(int index, const QPixmap& pixmap) {
    if (index < 0 || index >= m_model->rowCount())
        return;
    m_model->updateThumbnail(index, pixmap);
}

void SnapshotTimeline::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // Position button so part of it extends past the right edge (clipped)

    int btnH = m_createButton->height();
    int x = m_stripContainer->width() - 23; // crop ~27px of the right edge
    int y =
        (m_stripContainer->height() - btnH) / 2;

    m_createButton->move(x, y);
}
