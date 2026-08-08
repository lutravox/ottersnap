#include "ui/colorinfo.h"

#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QSurfaceFormat>
#include <QWidget>

ColorInfo::ColorInfo(QWidget *parentWidget)
    : QQuickView(parentWidget ? parentWidget->windowHandle() : nullptr),
      m_parentWidget(parentWidget) {
    QSurfaceFormat format = this->format();
    format.setAlphaBufferSize(8);
    setFormat(format);

    setColor(Qt::transparent);

    setSource(QUrl("qrc:/ui/qml/colorinfo.qml"));
    setFlags(Qt::ToolTip | Qt::FramelessWindowHint);

    if (m_parentWidget) {
        m_parentWidget->installEventFilter(this);
    }

    connect(rootObject(), SIGNAL(fadeOutFinished()), this, SLOT(onFadeOutFinished()));

    rootContext()->setContextProperty("colorInfo", this);

    hide();
}

void ColorInfo::setPickedColor(const QColor& color) {
    if (rootObject()) {
        rootObject()->setProperty("hexColor", color.name());
        rootObject()->setProperty("alphaValue", color.alphaF());
    }
}

void ColorInfo::setVisibleState(bool visible) {
    if (rootObject()) {
        rootObject()->setProperty("visibleState", visible);

        if (visible) {
            int w = rootObject()->width();
            int h = rootObject()->height();
            if (w <= 0)
                w = 260;
            if (h <= 0)
                h = 150;
            this->resize(w, h);
        }
    }

    if (visible) {
        if (m_parentWidget) {
            QWindow *topLevelWindow = m_parentWidget->window()->windowHandle();
            if (parent() != topLevelWindow) {
                setParent(topLevelWindow);
            }
        }
        show();
        raise();
        updatePosition();
    }
}

void ColorInfo::updatePosition() {
    if (!m_parentWidget)
        return;

    // Position it in the lower left corner of the viewer
    int marginX = 20;
    int marginY = 20;

    QPoint globalPos = m_parentWidget->mapToGlobal(QPoint(0, m_parentWidget->height()));
    int    x = globalPos.x() + marginX;
    int    y = globalPos.y() - height() - marginY;

    setPosition(QPoint(x, y));
}

bool ColorInfo::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_parentWidget &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
        updatePosition();
    }
    return QQuickView::eventFilter(watched, event);
}

void ColorInfo::onFadeOutFinished() {
    hide();
}

void ColorInfo::copyToClipboard(const QString& text) {
    QGuiApplication::clipboard()->setText(text);
}
