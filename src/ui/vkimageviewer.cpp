#include "ui/vkimageviewer.h"
#include "core/clusterindicatormodel.h"
#include "core/colorinfomodel.h"
#include "core/notificationmodel.h"
#include "ui/vkimageviewerrenderer.h"

#include <QAction>
#include <QDebug>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSGRendererInterface>
#include <QShowEvent>
#include <QStyleOption>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <QFile>

class RhiImageItem : public QQuickRhiItem {
  public:
    explicit RhiImageItem(QQuickRhiItemRenderer *renderer, QQuickItem *parent = nullptr)
        : QQuickRhiItem(parent), m_renderer(renderer) {
    }

  protected:
    QQuickRhiItemRenderer *createRenderer() override {
        return m_renderer;
    }

  private:
    QQuickRhiItemRenderer *m_renderer = nullptr;
};

VkImageViewer::VkImageViewer(QWidget               *parent,
                             ClusterIndicatorModel *indicatorModel,
                             ColorInfoModel        *colorInfoModel,
                             NotificationModel     *notificationModel)
    : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_renderer = new VkImageViewerRenderer();

    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    m_quickView = new QQuickView();

    m_quickView->engine()->rootContext()->setContextProperty("indicator", indicatorModel);
    m_quickView->engine()->rootContext()->setContextProperty("colorInfo", colorInfoModel);
    m_quickView->engine()->rootContext()->setContextProperty("notificationModel",
                                                             notificationModel);
    m_quickView->engine()->rootContext()->setContextProperty("viewer", this);

    m_quickView->setResizeMode(QQuickView::SizeRootObjectToView);
    m_quickView->setSource(QUrl("qrc:/ui/qml/viewer.qml"));
    if (QQuickItem *root = m_quickView->contentItem()) {
        root->setSize(QSizeF(size()));
        m_imageItem = new RhiImageItem(m_renderer, root);
        m_imageItem->setZ(-1);
    }

    m_container = QWidget::createWindowContainer(m_quickView, this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->addWidget(m_container);

    m_quickView->installEventFilter(this);
}

VkImageViewer::~VkImageViewer() {
    destroyQmlScene();
    if (m_quickView) {
        delete m_quickView;
        m_quickView = nullptr;
    }
}

void VkImageViewer::destroyQmlScene() {
    if (m_quickView && m_quickView->rootObject()) {
        delete m_quickView->rootObject();
    }
}

bool VkImageViewer::eventFilter(QObject *obj, QEvent *event) {
    if (obj != m_quickView && obj != m_container)
        return QObject::eventFilter(obj, event);

    switch (event->type()) {
        case QEvent::MouseButtonPress: {
            if (obj == m_quickView) {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->button() == Qt::LeftButton) {
                    if (isOverColorInfoOverlay(me->position())) {
                        m_clickPassedToQml = true;
                        return false;
                    }

                    if (m_pickingEnabled) {
                        emit colorPickRequested(me->position());
                        return true;
                    }

                    m_isDragging = true;
                    m_lastMousePos = me->position().toPoint();
                    setFocus();
                    return true;
                } else if (me->button() == Qt::MiddleButton) {
                    m_isDragging = true;
                    m_lastMousePos = me->position().toPoint();
                    setFocus();
                    return true;
                } else if (me->button() == Qt::RightButton) {
                    if (isOverColorInfoOverlay(me->position())) {
                        return false;
                    }

                    //  Context Menu
                    QMenu menu(this);

                    // View options
                    auto *scaleWithWindowAction = menu.addAction(tr("Scale with Window"));
                    scaleWithWindowAction->setCheckable(true);
                    scaleWithWindowAction->setChecked(m_scaleWithWindow);

                    auto *resetViewAction = menu.addAction(tr("Reset View"));
                    auto *actualSizeAction = menu.addAction(tr("Actual Size (100%)"));

                    menu.addSeparator();

                    auto *zoomInAction = menu.addAction(tr("Zoom In"));
                    auto *zoomOutAction = menu.addAction(tr("Zoom Out"));

                    menu.addSeparator();

                    // Effects options
                    auto *grayscaleAction = menu.addAction(tr("Grayscale"));
                    grayscaleAction->setCheckable(true);
                    grayscaleAction->setChecked(m_renderer->grayscaleEnabled());

                    auto *mirrorAction = menu.addAction(tr("Mirror"));
                    mirrorAction->setCheckable(true);
                    mirrorAction->setChecked(m_renderer->mirrorEnabled());

                    menu.addSeparator();

                    auto *resetEffectsAction = menu.addAction(tr("Reset All Effects"));

                    QAction *selectedAction = menu.exec(me->globalPosition().toPoint());

                    if (selectedAction == scaleWithWindowAction) {
                        emit scaleWithWindowToggled(scaleWithWindowAction->isChecked());
                        return true;
                    } else if (selectedAction == resetViewAction) {
                        emit resetViewRequested();
                        return true;
                    } else if (selectedAction == actualSizeAction) {
                        emit actualSizeRequested();
                        return true;
                    } else if (selectedAction == zoomInAction) {
                        emit zoomInRequested();
                        return true;
                    } else if (selectedAction == zoomOutAction) {
                        emit zoomOutRequested();
                        return true;
                    } else if (selectedAction == grayscaleAction) {
                        emit grayscaleToggled(grayscaleAction->isChecked());
                        return true;
                    } else if (selectedAction == mirrorAction) {
                        emit mirrorToggled(mirrorAction->isChecked());
                        return true;
                    } else if (selectedAction == resetEffectsAction) {
                        emit resetEffectsRequested();
                        return true;
                    }
                }
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton || me->button() == Qt::MiddleButton) {
                if (m_clickPassedToQml) {
                    m_clickPassedToQml = false;
                    return false;
                }
                if (m_isDragging) {
                    m_isDragging = false;
                } else {
                    emit imageClicked();
                }
                return true;
            }
            break;
        }
        // Pan
        case QEvent::MouseMove: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (!(me->buttons() & (Qt::LeftButton | Qt::MiddleButton))) {
                m_isDragging = false;
            }

            if (m_isDragging && m_hasImage) {
                QPoint delta = me->position().toPoint() - m_lastMousePos;
                emit   panRequested(delta.x(), delta.y());
            }
            m_lastMousePos = me->position().toPoint();
            return false;
        }
        // Zoom
        case QEvent::Wheel: {
            auto *we = static_cast<QWheelEvent *>(event);

            emit zoomRequested(we->angleDelta().y() >= 0, we->modifiers() & Qt::ControlModifier);
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}

bool VkImageViewer::isOverColorInfoOverlay(const QPointF& windowPos) const {
    QQuickItem *root = m_quickView ? m_quickView->contentItem() : nullptr;
    QQuickItem *overlay = root ? root->findChild<QQuickItem *>("colorInfoOverlay") : nullptr;
    if (!overlay || overlay->width() <= 0 || overlay->height() <= 0)
        return false;
    if (!overlay->property("interactive").toBool())
        return false;

    QPointF topLeft = overlay->mapToScene(QPointF(0, 0));
    QRectF  sceneRect = QRectF(topLeft, overlay->size());
    return sceneRect.contains(windowPos);
}

RenderState VkImageViewer::renderState() const {
    return m_renderer ? m_renderer->renderState() : RenderState::Empty;
}

void VkImageViewer::setRenderParams(const RenderParams& params) {
    m_params = params;
    if (m_renderer) {
        m_renderer->m_params = params;
    }
}

void VkImageViewer::notifyViewModelChanged() {
    emit zoomChanged(m_params.zoom * 100.0);
    update();
}

void VkImageViewer::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);

    QSize sz = m_container->size();
    if (!sz.isEmpty()) {
        emit viewportResized(sz.width(), sz.height());
    }

    if (m_imageItem) {
        m_imageItem->update();
    }
}

void VkImageViewer::setImage(const QImage& image) {
    m_hasImage = !image.isNull();
    if (!m_hasImage)
        return;

    m_renderer->setImage(image, ++m_generation);
}

bool VkImageViewer::reconstruct(const ReconstructionSequence& seq) {
    m_hasImage = true;
    return m_renderer->reconstruct(seq, ++m_generation);
}

void VkImageViewer::clear() {
    m_hasImage = false;
    m_renderer->clear();
    m_generation++;
}

void VkImageViewer::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    QSize sz = m_container->size();
    if (sz.isEmpty())
        return;

    if (auto *root = m_quickView->contentItem()) {
        root->setSize(sz);
    }

    if (m_imageItem) {
        m_imageItem->setPosition(QPointF(0, 0));
        m_imageItem->setSize(QSizeF(sz));
    }
    m_renderer->setViewportSize(sz);
    emit viewportResized(sz.width(), sz.height());
}

void VkImageViewer::setPickingEnabled(bool enabled) {
    m_pickingEnabled = enabled;
    if (m_quickView) {
        m_quickView->setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
    }
}

void VkImageViewer::setScaleWithWindowChecked(bool checked) {
    m_scaleWithWindow = checked;
}

void VkImageViewer::onEffectsChanged() {
    if (m_renderer) {
        m_renderer->markUboDirty();
    }
}

void VkImageViewer::handleImageDrop(const QUrl& url) {
    QString path = url.toLocalFile();
    if (!path.isEmpty())
        emit imageOpenRequested(path);
}

void VkImageViewer::setReconstructor(std::shared_ptr<VkSnapshotReconstructor> reconstructor) {
    if (m_renderer) {
        m_renderer->setReconstructor(reconstructor);
    }
    if (m_quickView) {
        m_quickView->requestUpdate();
    }
}

void VkImageViewer::paintEvent(QPaintEvent *event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
