#include "ui/statusbar.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QPushButton>

static constexpr double c_minZoom = 5.0;
static constexpr double c_maxZoom = 6400.0;
static constexpr double c_defaultZoom = 100.0;
static constexpr double c_zoomStep = 10.0;

StatusBar::StatusBar(QWidget *parent)
    : QWidget(parent), spinbox(new QDoubleSpinBox(this)), btnFit(new QPushButton("Fit", this)) {
    spinbox->setRange(c_minZoom, c_maxZoom);
    spinbox->setSingleStep(c_zoomStep);
    spinbox->setValue(c_defaultZoom);
    spinbox->setDecimals(1);
    spinbox->setSuffix("%");
    spinbox->setMinimumWidth(90);

    btnFit->setFixedWidth(60);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);

    layout->addStretch();
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
