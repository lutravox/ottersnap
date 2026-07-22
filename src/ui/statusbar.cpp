#include "ui/statusbar.h"

#include <QDoubleSpinBox>
#include <QFile>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

static constexpr double c_minZoom = 5.0;
static constexpr double c_maxZoom = 6400.0;
static constexpr double c_defaultZoom = 100.0;
static constexpr double c_zoomStep = 10.0;

StatusBar::StatusBar(QWidget *parent)
    : QWidget(parent), m_timestampLabel(new QLabel(this)), m_dimensionsLabel(new QLabel(this)),
      spinbox(new QDoubleSpinBox(this)), btnFit(new QPushButton(tr("Fit"), this)) {
    spinbox->setRange(c_minZoom, c_maxZoom);
    spinbox->setSingleStep(c_zoomStep);
    spinbox->setValue(c_defaultZoom);
    spinbox->setDecimals(1);
    spinbox->setSuffix("%");
    spinbox->setMinimumWidth(90);

    btnFit->setFixedWidth(60);

    m_timestampLabel->setText("");
    m_timestampLabel->setObjectName("timestampLabel");
    m_timestampLabel->setEnabled(false);
    m_dimensionsLabel->setObjectName("dimensionsLabel");
    m_dimensionsLabel->setEnabled(false);

    QFile qss(":/qss/statusbar.qss");
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);

    layout->addWidget(m_timestampLabel);
    layout->addStretch();
    layout->addWidget(m_dimensionsLabel);
    layout->addWidget(spinbox);
    layout->addWidget(btnFit);

    connect(spinbox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &StatusBar::zoomChanged);
    connect(btnFit, &QPushButton::clicked, this, &StatusBar::fitRequested);
}

double StatusBar::zoom() const {
    return spinbox->value();
}

void StatusBar::setZoom(double pct, bool emitSignal) {
    if (!emitSignal) {
        spinbox->blockSignals(true);
    }
    spinbox->setValue(pct);
    spinbox->blockSignals(false);
}

void StatusBar::setDimensions(int width, int height) {
    m_dimensionsLabel->setText(QString("%1 x %2").arg(width).arg(height));
}

void StatusBar::setTimestamp(const QString& timestamp) {
    m_timestampLabel->setText(timestamp);
}
