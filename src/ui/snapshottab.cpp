#include "snapshottab.h"

#include <QFile>
#include <QIODevice>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOption>
#include <QStylePainter>

SnapshotTab::SnapshotTab(QWidget *parent) : QLabel(parent), m_selected(false) {
    setAttribute(Qt::WA_Hover);
    setProperty("state", "unselected");
    QFile qss(":/qss/snapshottab.qss");
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
        setStyleSheet(QString::fromUtf8(qss.readAll()));
}

bool SnapshotTab::isSelected() const {
    return m_selected;
}

void SnapshotTab::setSelected(bool selected) {
    if (m_selected != selected) {
        m_selected = selected;
        setProperty("state", selected ? "selected" : "unselected");
        style()->unpolish(this);
        style()->polish(this);
    }
}

void SnapshotTab::mousePressEvent(QMouseEvent *event) {
    emit clicked();
}
