#include "ui/statusbar.h"

#include <QAction>
#include <QDoubleSpinBox>
#include <QFile>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QWidget>

static constexpr double c_minZoom = 5.0;
static constexpr double c_maxZoom = 6400.0;
static constexpr double c_defaultZoom = 100.0;
static constexpr double c_zoomStep = 10.0;

StatusBar::StatusBar(QWidget *parent)
    : QWidget(parent), m_timestampLabel(new QLabel(this)), m_dimensionsLabel(new QLabel(this)),
      zoomSpinbox(new QDoubleSpinBox(this)), resetButton(new QPushButton(tr("Reset"), this)),
      m_colorButton(new QPushButton(this)) {
    zoomSpinbox->setRange(c_minZoom, c_maxZoom);
    zoomSpinbox->setSingleStep(c_zoomStep);
    zoomSpinbox->setValue(c_defaultZoom);
    zoomSpinbox->setDecimals(1);
    zoomSpinbox->setSuffix("%");
    zoomSpinbox->setMinimumWidth(90);
    zoomSpinbox->setToolTip(tr("Zoom level"));

    zoomSpinbox->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(zoomSpinbox,
            &QWidget::customContextMenuRequested,
            this,
            &StatusBar::onZoomSpinboxContextMenuRequested);

    resetButton->setFixedWidth(60);
    resetButton->setToolTip(tr("Reset view to fit window"));

    m_timestampLabel->setText("");
    m_timestampLabel->setObjectName("timestampLabel");
    m_timestampLabel->setEnabled(false);
    m_dimensionsLabel->setObjectName("dimensionsLabel");
    m_dimensionsLabel->setEnabled(false);

    m_colorButton->setFixedSize(32, 32);
    m_colorButton->setObjectName("colorButton");
    m_colorButton->setEnabled(false);
    m_colorButton->setToolTip(tr("Show color information"));

    QFile qss(":/qss/statusbar.qss");
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);

    layout->addWidget(m_colorButton);
    layout->addWidget(m_timestampLabel);
    layout->addStretch();
    layout->addWidget(m_dimensionsLabel);
    layout->addWidget(zoomSpinbox);
    layout->addWidget(resetButton);

    connect(zoomSpinbox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &StatusBar::zoomChanged);
    connect(resetButton, &QPushButton::clicked, this, &StatusBar::fitRequested);
    connect(m_colorButton, &QPushButton::clicked, this, [this]() {
        m_colorInfoVisible = !m_colorInfoVisible;
        m_colorButton->setToolTip(m_colorInfoVisible ? tr("Hide color information") : tr("Show color information"));
        emit colorInfoToggled(m_colorInfoVisible);
    });
}

double StatusBar::zoom() const {
    return zoomSpinbox->value();
}

void StatusBar::setZoom(double pct, bool emitSignal) {
    if (!emitSignal) {
        zoomSpinbox->blockSignals(true);
    }
    zoomSpinbox->setValue(pct);
    zoomSpinbox->blockSignals(false);
}

void StatusBar::setDimensions(int width, int height) {
    m_dimensionsLabel->setText(QString("%1 x %2").arg(width).arg(height));
}

void StatusBar::setTimestamp(const QString& timestamp) {
    m_timestampLabel->setText(timestamp);
}

void StatusBar::setColor(const QColor& color) {
    m_colorButton->setEnabled(true);
    m_colorButton->setStyleSheet(QString("QPushButton#colorButton { background-color: %1; }").arg(color.name()));
}

void StatusBar::onZoomSpinboxContextMenuRequested(const QPoint& pos) {
    QMenu menu(this);

    struct ZoomOption {
        QString label;
        double  value;
    };

    const QVector<ZoomOption> options = {
        {tr("5%"), 5.0},
        {tr("10%"), 10.0},
        {tr("25%"), 25.0},
        {tr("50%"), 50.0},
        {tr("100%"), 100.0},
        {tr("200%"), 200.0},
        {tr("400%"), 400.0},
        {tr("800%"), 800.0},
        {tr("1600%"), 1600.0},
    };

    for (const auto& opt : options) {
        auto *action = menu.addAction(opt.label);
        connect(action, &QAction::triggered, this, [this, val = opt.value]() {
            zoomSpinbox->setValue(val);
        });
    }

    menu.exec(zoomSpinbox->mapToGlobal(pos));
}
