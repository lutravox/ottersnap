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

#include "core/imagesession.h"

constexpr int c_thumbnailSize = 48;

SnapshotTimeline::SnapshotTimeline(QWidget *parent) : QWidget(parent) {
    m_delegate = new SnapshotTimelineDelegate(this);

    m_listView = new QListView(this);
    m_listView->setItemDelegate(m_delegate);

    // Configure for horizontal strip look
    m_listView->setFlow(QListView::LeftToRight);
    m_listView->setWrapping(false);
    m_listView->setMovement(QListView::Static);
    m_listView->setSpacing(3);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setSelectionMode(QAbstractItemView::NoSelection);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView->setFrameStyle(QFrame::NoFrame);

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
        if (m_controller) {
            m_controller->selectSnapshot(row);
        }
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
        QPoint pos;

        if (event->type() == QEvent::MouseMove || event->type() == QEvent::HoverMove ||
            event->type() == QEvent::MouseButtonPress) {
            if (auto *me = dynamic_cast<QMouseEvent *>(event)) {
                pos = m_listView->viewport()->mapFromGlobal(me->globalPosition().toPoint());
            } else if (auto *he = dynamic_cast<QHoverEvent *>(event)) {
                pos = m_listView->viewport()->mapFromGlobal(he->globalPosition().toPoint());
            }
        }

        if (event->type() == QEvent::MouseMove) {
            QModelIndex index = m_listView->indexAt(pos);
            int         newHoverIndex = index.isValid() ? index.row() : -1;
            m_delegate->setHoverIndex(newHoverIndex);
            m_listView->viewport()->update();
        } else if (event->type() == QEvent::HoverMove) {
            QModelIndex index = m_listView->indexAt(pos);
            int         newHoverIndex = index.isValid() ? index.row() : -1;
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
                QModelIndex index = m_listView->indexAt(pos);
                if (index.isValid()) {
                    int snapshotIdx = m_controller->model()
                                          ->data(index, SnapshotTimelineModel::IndexRole)
                                          .toInt();
                    m_controller->model()->clearNewStatus(snapshotIdx);
                    m_listView->viewport()->update();
                }
            } else if (me->button() == Qt::MiddleButton) {
                QModelIndex index = m_listView->indexAt(pos);
                if (index.isValid()) {
                    int snapshotIdx = m_controller->model()
                                          ->data(index, SnapshotTimelineModel::IndexRole)
                                          .toInt();
                    if (snapshotIdx == m_controller->secondarySnapshotDbId()) {
                        emit secondarySnapshotSelected(ImageSession::SecondaryNone);
                    } else {
                        emit secondarySnapshotSelected(snapshotIdx);
                    }
                }
            }

            if (me->button() == Qt::RightButton) {
                QModelIndex index = m_listView->indexAt(pos);
                if (index.isValid()) {
                    int row = index.row();
                    int snapshotIdx = m_controller->model()
                                          ->data(index, SnapshotTimelineModel::IndexRole)
                                          .toInt();

                    QMenu menu(this);

                    QAction *secAct =
                        menu.addAction(snapshotIdx == m_controller->secondarySnapshotDbId()
                                           ? tr("Deselect Comparison Snapshot")
                                           : tr("Set as Comparison Snapshot"));
                    connect(secAct, &QAction::triggered, this, [this, snapshotIdx]() {
                        if (snapshotIdx == m_controller->secondarySnapshotDbId()) {
                            emit secondarySnapshotSelected(ImageSession::SecondaryNone);
                        } else {
                            emit secondarySnapshotSelected(snapshotIdx);
                        }
                    });

                    if (snapshotIdx != -1) {
                        menu.addSeparator();
                        QAction *deleteAct = menu.addAction(tr("Delete Snapshot"));
                        connect(deleteAct, &QAction::triggered, this, [this, row]() {
                            emit snapshotDeletionRequested(row);
                        });
                    }

                    menu.exec(me->globalPosition().toPoint());
                    return true;
                }
            }
        } else if (event->type() == QEvent::Wheel) {
            QWheelEvent *we = static_cast<QWheelEvent *>(event);
            if (m_controller && m_controller->model()->rowCount() == 0) {
                return QWidget::eventFilter(obj, event);
            }

            int delta = we->angleDelta().y();
            if (delta == 0) {
                return QWidget::eventFilter(obj, event);
            }

            int step = delta > 0 ? -1 : 1;
            int current = m_controller ? m_controller->currentSelectedIndex() : -1;
            int next = qBound(
                0, current + step, (m_controller ? m_controller->model()->rowCount() : 0) - 1);
            if (next != current) {
                if (m_controller) {
                    m_controller->selectSnapshot(next);
                }
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void SnapshotTimeline::setController(SnapshotTimelineController *controller) {
    m_controller = controller;
    if (m_controller) {
        m_listView->setModel(m_controller->model());
        connect(m_controller,
                &SnapshotTimelineController::snapshotSelected,
                this,
                &SnapshotTimeline::updateSelection);
    }
}

void SnapshotTimeline::setCreateButtonEnabled(bool enabled) {
    m_createButton->setEnabled(enabled);
}

void SnapshotTimeline::setSecondaryIndex(int dbId) {
    int row = m_controller ? m_controller->rowForDbId(dbId) : -1;

    if (m_delegate) {
        m_delegate->setSecondaryIndex(row);
        m_listView->viewport()->update();
    }
}

void SnapshotTimeline::updateSelection(int newIndex) {
    // Update the delegate's current index for custom rendering
    if (m_delegate) {
        m_delegate->setCurrentIndex(newIndex);
    }

    // Scroll to center the current thumbnail
    if (m_controller && newIndex >= 0 && newIndex < m_controller->model()->rowCount()) {
        m_listView->setCurrentIndex(m_controller->model()->index(newIndex));
    }

    // Force a viewport update to clear any painting artifacts (e.g., the "new" highlight
    // border)
    m_listView->viewport()->update();
}

void SnapshotTimeline::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // Position button so part of it extends past the right edge (clipped)

    int btnH = m_createButton->height();
    int x = m_stripContainer->width() - 23; // crop ~27px of the right edge
    int y = (m_stripContainer->height() - btnH) / 2;

    m_createButton->move(x, y);
}
