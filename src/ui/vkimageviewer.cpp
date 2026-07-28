#include "ui/vkimageviewer.h"
#include "core/imagecache.h"
#include "core/snapshotstore.h"
#include "core/viewstate.h"
#include "core/vulkancontext.h"
#include "ui/vkimageviewerrenderer.h"

#include <QAction>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QShowEvent>
#include <QUrl>
#include <QVBoxLayout>
#include <QVulkanFunctions>
#include <QWheelEvent>

#include <QFile>

class VkImageViewerWindow : public QVulkanWindow {
  public:
    explicit VkImageViewerWindow(QWindow *parent = nullptr) : QVulkanWindow(parent) {
        QSurfaceFormat format;
        format.setAlphaBufferSize(8);
        setFormat(format);
    }

    void setViewerRenderer(VkImageViewerRenderer *r) {
        m_viewerRenderer = r;
    }

  protected:
    QVulkanWindowRenderer *createRenderer() override {
        if (m_viewerRenderer) {
            m_viewerRenderer->m_vkWindow = this;
            qDebug() << "[VkImageViewer] createRenderer() called, m_vkWindow set";
        }
        return m_viewerRenderer;
    }

  private:
    VkImageViewerRenderer *m_viewerRenderer = nullptr;
};

VkImageViewer::VkImageViewer(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    {
        QFile qss(":/qss/vkimageviewer.qss");
        if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
            setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    m_renderer = new VkImageViewerRenderer();
    auto *vkWindow = new VkImageViewerWindow();
    vkWindow->setViewerRenderer(m_renderer);
    vkWindow->setVulkanInstance(VulkanContext::instance().getInstance());
    vkWindow->setFlags(QVulkanWindow::PersistentResources);
    m_vulkanWindow = vkWindow;

    m_container = QWidget::createWindowContainer(m_vulkanWindow, this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_container);

    // Intercept events on the embedded QVulkanWindow
    m_vulkanWindow->installEventFilter(this);
}

VkImageViewer::~VkImageViewer() {
    VulkanContext::instance().clearDevice();
}

void VkImageViewer::setNotificationCallback(IEffectsRenderer::EffectChangedCallback callback) {
    m_notificationCallback = callback;
}

bool VkImageViewer::eventFilter(QObject *obj, QEvent *event) {
    if (obj != m_vulkanWindow && obj != m_container)
        return QObject::eventFilter(obj, event);

    switch (event->type()) {
        // Offer Image Drop
        case QEvent::DragEnter: {
            if (obj == m_vulkanWindow) {
                auto *de = static_cast<QDragEnterEvent *>(event);
                if (de->mimeData()->hasUrls()) {
                    de->acceptProposedAction();
                    return true;
                }
            }
            break;
        }
        // Open Image on Drop
        case QEvent::Drop: {
            if (obj == m_vulkanWindow) {
                auto            *dp = static_cast<QDropEvent *>(event);
                const QMimeData *mimeData = dp->mimeData();
                if (mimeData && mimeData->hasUrls()) {
                    for (const QUrl& url : mimeData->urls()) {
                        QString path = url.toLocalFile();
                        if (!path.isEmpty()) {
                            emit imageOpenRequested(path);
                        }
                    }
                    dp->acceptProposedAction();
                    return true;
                }
            }
            break;
        }
        case QEvent::MouseButtonPress: {
            if (obj == m_vulkanWindow) {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->button() == Qt::LeftButton) {
                    m_isDragging = true;
                    m_lastMousePos = me->position().toPoint();
                    setFocus();
                    return true;
                } else if (me->button() == Qt::RightButton) {
                    //  Context Menu
                    QMenu menu(this);

                    auto *grayscaleAction = menu.addAction(tr("Grayscale"));
                    grayscaleAction->setCheckable(true);
                    grayscaleAction->setChecked(m_renderer->grayscaleEnabled());

                    auto *mirrorAction = menu.addAction(tr("Mirror"));
                    mirrorAction->setCheckable(true);
                    mirrorAction->setChecked(m_renderer->mirrorEnabled());

                    QAction *selectedAction = menu.exec(me->globalPosition().toPoint());

                    if (selectedAction == grayscaleAction) {
                        bool enabled = grayscaleAction->isChecked();
                        emit grayscaleToggled(enabled);
                        if (m_notificationCallback) {
                            m_notificationCallback(enabled, m_renderer->mirrorEnabled());
                        }
                        return true;
                    } else if (selectedAction == mirrorAction) {
                        bool enabled = mirrorAction->isChecked();
                        emit mirrorToggled(enabled);
                        if (m_notificationCallback) {
                            m_notificationCallback(m_renderer->grayscaleEnabled(), enabled);
                        }
                        return true;
                    }
                }
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
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
            if (!(me->buttons() & Qt::LeftButton)) {
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

void VkImageViewer::setViewState(const ViewState& state) {
    m_currentViewState = state;
    emit zoomChanged(state.percentage());
    if (m_renderer) {
        m_renderer->markUboDirty();
    }
}

void VkImageViewer::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);

    QSize sz = m_container->size();
    if (!sz.isEmpty()) {
        emit viewportResized(sz.width(), sz.height());
    }

    m_vulkanWindow->requestUpdate();
}

void VkImageViewer::setImage(const QImage& image, bool preserveView) {
    m_hasImage = !image.isNull();
    if (!m_hasImage)
        return;

    // Initial state is now managed by the ViewController.
    // We only forward the image to the renderer.
    m_renderer->setImage(image, preserveView);
}

void VkImageViewer::reconstruct(const ReconstructionSequence& seq) {
    m_hasImage = true;
    m_renderer->reconstruct(seq);
}

void VkImageViewer::clear() {
    m_hasImage = false;
    m_renderer->clear();
}

void VkImageViewer::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    QSize sz = m_container->size();
    if (sz.isEmpty())
        return;
    m_renderer->setViewportSize(sz);
    emit viewportResized(sz.width(), sz.height());
}

void VkImageViewer::setSession(ImageSession *session) {
    if (m_renderer) {
        m_renderer->setSession(session);
    }
}

void VkImageViewer::setGrayscale(bool enabled) {
    m_renderer->markUboDirty();
}

void VkImageViewer::setMirror(bool enabled) {
    m_renderer->markUboDirty();
}

void VkImageViewer::setReconstructor(std::shared_ptr<VkSnapshotReconstructor> reconstructor) {
    if (m_renderer) {
        m_renderer->setReconstructor(reconstructor);
    }
    if (m_vulkanWindow) {
        m_vulkanWindow->requestUpdate();
    }
}
