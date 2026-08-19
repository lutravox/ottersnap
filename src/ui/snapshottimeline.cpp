#include "ui/snapshottimeline.h"
#include "core/imagesession.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
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
#include <QRubberBand>
#include <QWheelEvent>

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
        if (QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) {
            return;
        }

        m_controller->clearSelection();
        m_delegate->setSelectedIndices({});
        m_listView->viewport()->update();
        emit selectionChanged();

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
        bool   hasPos = false;

        if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
            if (auto *me = static_cast<QMouseEvent *>(event)) {
                pos = m_listView->viewport()->mapFromGlobal(me->globalPosition().toPoint());
                hasPos = true;
            }
        } else if (event->type() == QEvent::HoverMove) {
            if (auto *he = static_cast<QHoverEvent *>(event)) {
                pos = m_listView->viewport()->mapFromGlobal(he->globalPosition().toPoint());
                hasPos = true;
            }
        }

        if (event->type() == QEvent::MouseMove) {
            if (hasPos) {
                QModelIndex index = m_listView->indexAt(pos);
                int         newHoverIndex = index.isValid() ? index.row() : -1;
                m_delegate->setHoverIndex(newHoverIndex);
                m_listView->viewport()->update();
            }
        } else if (event->type() == QEvent::HoverMove) {
            if (hasPos) {
                QModelIndex index = m_listView->indexAt(pos);
                int         newHoverIndex = index.isValid() ? index.row() : -1;
                m_delegate->setHoverIndex(newHoverIndex);
                m_listView->viewport()->update();
            }
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave) {
            m_delegate->setHoverIndex(-1);
            m_listView->viewport()->update();
        } else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);

            if (me->button() == Qt::LeftButton) {
                QModelIndex index = m_listView->indexAt(pos);
                if (index.isValid()) {
                    int row = index.row();

                    // Handle multi-selection
                    if (me->modifiers() & Qt::ControlModifier) {
                        m_controller->toggleSelection(row);
                        m_delegate->setSelectedIndices(m_controller->selectedIndices());
                        m_listView->viewport()->update();
                        emit selectionChanged();
                        return true; // Prevent activation
                    } else if (me->modifiers() & Qt::ShiftModifier) {
                        m_controller->selectRange(row, row);
                        m_delegate->setSelectedIndices(m_controller->selectedIndices());
                        m_listView->viewport()->update();
                        emit selectionChanged();
                        return true; // Prevent activation
                    }

                    QUuid uuid = uuidAt(index);
                    m_controller->model()->clearNewStatus(uuid);
                    m_listView->viewport()->update();
                }
            } else if (me->button() == Qt::MiddleButton) {
                // Secondary selection
                QModelIndex index = m_listView->indexAt(pos);
                if (index.isValid()) {
                    QUuid   uuid = uuidAt(index);
                    QString id = uuid.isNull() ? ImageSession::c_currentId
                                               : uuid.toString(QUuid::WithoutBraces);
                    if (id == m_controller->secondarySnapshotId()) {
                        emit secondarySnapshotSelected(QString());
                    } else {
                        emit secondarySnapshotSelected(id);
                    }
                }
            } else if (me->button() == Qt::RightButton) {
                // Context Menu
                QModelIndex index = m_listView->indexAt(pos);
                if (index.isValid()) {
                    int     row = index.row();
                    QUuid   uuid = uuidAt(index);
                    QString id = uuid.isNull() ? ImageSession::c_currentId
                                               : uuid.toString(QUuid::WithoutBraces);

                    QMenu menu(this);

                    QAction *secAct = menu.addAction(id == m_controller->secondarySnapshotId()
                                                         ? tr("Deselect Comparison Snapshot")
                                                         : tr("Set as Comparison Snapshot"));
                    connect(secAct, &QAction::triggered, this, [this, id]() {
                        if (id == m_controller->secondarySnapshotId()) {
                            emit secondarySnapshotSelected(QString());
                        } else {
                            emit secondarySnapshotSelected(id);
                        }
                    });

                    if (!uuid.isNull() || !m_controller->selectedIndices().isEmpty()) {
                        menu.addSeparator();

                        if (!uuid.isNull()) {
                            QAction *deleteAct = menu.addAction(tr("Delete Snapshot"));
                            connect(deleteAct, &QAction::triggered, this, [this, row]() {
                                QModelIndex index = m_controller->model()->index(row);
                                QUuid       uuid = uuidAt(index);
                                emit        snapshotDeletionRequested(uuid);
                            });
                        }

                        if (!m_controller->selectedIndices().isEmpty()) {
                            QString  actionText = (m_controller->selectedIndices().size() == 1)
                                                      ? tr("Delete Selected Snapshot (%1)")
                                                            .arg(m_controller->selectedIndices().size())
                                                      : tr("Delete Selected Snapshots (%1)")
                                                            .arg(m_controller->selectedIndices().size());
                            QAction *multiDeleteAct = menu.addAction(actionText);
                            connect(multiDeleteAct, &QAction::triggered, this, [this]() {
                                QVector<QUuid> uuids = m_controller->selectedUuids();
                                emit multipleSnapshotsDeletionRequested(uuids);
                            });
                        }
                    }

                    menu.exec(me->globalPosition().toPoint());
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_listView->viewport()->update();
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

QUuid SnapshotTimeline::uuidAt(const QModelIndex& index) const {
    if (!index.isValid() || !m_controller)
        return QUuid();

    return m_controller->model()->data(index, SnapshotTimelineModel::UuidRole).value<QUuid>();
}

void SnapshotTimeline::setCreateButtonEnabled(bool enabled) {
    m_createButton->setEnabled(enabled);
}

void SnapshotTimeline::setSecondaryIdentity(const QString& id) {
    int row = -1;
    if (m_controller && !id.isEmpty()) {
        row = m_controller->model()->rowForUuidString(id);
    }

    if (m_delegate) {
        m_delegate->setSecondaryIndex(row);
        m_listView->viewport()->update();
    }
}

void SnapshotTimeline::updateSelection(int newIndex) {
    m_controller->clearSelection();
    emit selectionChanged();

    // Update the delegate's current index for custom rendering
    if (m_delegate) {
        m_delegate->setCurrentIndex(newIndex);
        m_delegate->setSelectedIndices({});
    }

    // Scroll to center the current thumbnail
    if (m_controller && newIndex >= 0 && newIndex < m_controller->model()->rowCount()) {
        m_listView->setCurrentIndex(m_controller->model()->index(newIndex));
    }

    m_listView->viewport()->update();
}

void SnapshotTimeline::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // Position button so part of it extends past the right edge (clipped)

    int btnH = m_createButton->height();
    int x = m_stripContainer->width() - c_clipAmount;
    int y = (m_stripContainer->height() - btnH) / 2;

    m_createButton->move(x, y);
}
