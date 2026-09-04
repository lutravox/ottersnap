#include "ui/emptystate.h"
#include "core/notificationmodel.h"

#include <QFont>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QResizeEvent>
#include <QSGRendererInterface>
#include <QPushButton>
#include <QSurfaceFormat>
#include <QUrl>
#include <QVBoxLayout>

EmptyState::EmptyState(QWidget *parent, NotificationModel *notificationModel)
    : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    auto *openBtn = new QPushButton(tr("Open an Image"), this);
    openBtn->setFixedWidth(200);
    openBtn->setFixedHeight(60);
    openBtn->setFont({openBtn->font().family(), 12, QFont::Normal});
    connect(openBtn, &QPushButton::clicked, this, &EmptyState::openRequested);

    layout->addWidget(openBtn);

    if (notificationModel) {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

        QSurfaceFormat format;
        format.setAlphaBufferSize(8);

        m_notificationView = new QQuickView();
        m_notificationView->setFormat(format);
        m_notificationView->setColor(Qt::transparent);
        m_notificationView->setResizeMode(QQuickView::SizeRootObjectToView);
        m_notificationView->setFlags(Qt::WindowTransparentForInput | Qt::FramelessWindowHint);
        m_notificationView->engine()->rootContext()->setContextProperty("notificationModel", notificationModel);
        m_notificationView->setSource(QUrl("qrc:/ui/qml/notificationlayer.qml"));

        m_notificationContainer = QWidget::createWindowContainer(m_notificationView, this);
        m_notificationContainer->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_notificationContainer->setGeometry(rect());
    }
}

EmptyState::~EmptyState() {
    if (m_notificationView) {
        if (QQuickItem *root = m_notificationView->rootObject()) {
            delete root;
        }
        delete m_notificationView;
        m_notificationView = nullptr;
    }
}

void EmptyState::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_notificationContainer) {
        m_notificationContainer->setGeometry(rect());
    }
}
