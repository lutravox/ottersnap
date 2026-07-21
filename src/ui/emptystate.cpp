#include "ui/emptystate.h"

#include <QFont>
#include <QPushButton>
#include <QVBoxLayout>

EmptyState::EmptyState(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    auto *openBtn = new QPushButton("Open an Image", this);
    openBtn->setFixedWidth(200);
    openBtn->setFixedHeight(60);
    openBtn->setFont({openBtn->font().family(), 12, QFont::Normal});
    connect(openBtn, &QPushButton::clicked, this, &EmptyState::openRequested);

    auto *settingsBtn = new QPushButton("Settings", this);
    settingsBtn->setFixedWidth(200);
    settingsBtn->setFixedHeight(40);
    connect(settingsBtn, &QPushButton::clicked, this, &EmptyState::settingsRequested);

    layout->addWidget(openBtn);
    layout->addWidget(settingsBtn);
}
