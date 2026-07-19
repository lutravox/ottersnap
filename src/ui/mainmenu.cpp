#include "ui/mainmenu.h"

#include <QFont>
#include <QPushButton>
#include <QVBoxLayout>

MainMenu::MainMenu(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    auto *openBtn = new QPushButton("Open an Image", this);
    openBtn->setFixedWidth(200);
    openBtn->setFixedHeight(60);
    openBtn->setFont({openBtn->font().family(), 12, QFont::Normal});
    connect(openBtn, &QPushButton::clicked, this, &MainMenu::openRequested);

    layout->addWidget(openBtn);
}
