#include <QApplication>
#include <QPainter>
#include <QPalette>
#include "ui/snapshottimelinedelegate.h"
#include "core/snapshottimelinemodel.h"

SnapshotTimelineDelegate::SnapshotTimelineDelegate(QObject *parent) : QStyledItemDelegate(parent) {
}

void SnapshotTimelineDelegate::paint(QPainter                   *painter,
                                     const QStyleOptionViewItem& option,
                                     const QModelIndex&          index) const {
    if (!index.isValid())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QPixmap pixmap = index.data(SnapshotTimelineModel::ThumbnailRole).value<QPixmap>();
    bool    isNew = index.data(SnapshotTimelineModel::IsNewRole).toBool();

    // Create label
    int     row = index.row();
    bool    isCurrent = index.data(SnapshotTimelineModel::IsCurrentImageRole).toBool();
    QString labelText = isCurrent ? tr("C") : QString::number(row + 1);

    // Dimensions
    const int thumbSize = c_thumbSize;
    const int labelHeight = c_labelHeight;
    const int spacing = c_spacing;
    const int padding = c_padding;
    const int bottomPadding = c_bottomPadding;
    const int totalHeight =
        thumbSize + labelHeight + spacing + (padding * 2) + bottomPadding + (c_borderMargin * 2);

    QRect rect = option.rect;
    int   x = rect.x() + (rect.width() - thumbSize) / 2;
    int   y = rect.y() + (rect.height() - totalHeight) / 2 + padding + c_borderMargin;

    // Background (Selection or Hover)
    if (index.row() == m_currentIndex || index.row() == m_hoverIndex ||
        index.row() == m_secondaryIndex || m_selectedIndices.contains(index.row())) {
        QColor hoverColor = QApplication::palette().midlight().color();
        QColor selectedColor = QApplication::palette().light().color();
        QColor secondaryColor = QApplication::palette().mid().color();
        QColor multiColor = QApplication::palette().highlight().color();
        multiColor.setAlpha(80);

        QColor bgColor = (m_selectedIndices.contains(index.row())) ? multiColor
                         : (index.row() == m_currentIndex)         ? selectedColor
                         : (index.row() == m_secondaryIndex)       ? secondaryColor
                                                                   : hoverColor;
        painter->setBrush(bgColor);

        QColor borderColor = (m_selectedIndices.contains(index.row())) ? multiColor
                             : (index.row() == m_currentIndex)         ? selectedColor
                             : (index.row() == m_secondaryIndex)       ? secondaryColor
                                                                       : hoverColor;
        QPen   borderPen(borderColor);

        borderPen.setWidth(1);
        painter->setPen(borderPen);

        int   contentHeight = thumbSize + labelHeight + spacing;
        QRect bgRect(x - 2, y - padding - 2, thumbSize + 4, contentHeight + padding * 2 + 4);
        painter->drawRoundedRect(bgRect, 4, 4);
    }

    // Draw Label
    if (index.row() == m_currentIndex) {
        painter->setPen(QApplication::palette().highlight().color());
    } else if (index.row() == m_secondaryIndex) {
        painter->setPen(QApplication::palette().color(QPalette::Text));
    } else {
        painter->setPen(QApplication::palette().color(QPalette::Disabled, QPalette::Text));
    }

    QFont font = painter->font();
    font.setPointSize(c_labelFontSize);
    if (index.row() == m_currentIndex) {
        font.setBold(true);
    }
    painter->setFont(font);

    painter->drawText(QRect(x, y, thumbSize, labelHeight), Qt::AlignCenter, labelText);

    // Draw Thumbnail
    QRect thumbRect(x, y + labelHeight + spacing, thumbSize, thumbSize);
    painter->drawPixmap(thumbRect, pixmap);

    // Draw "New" indicator dot above label
    if (isNew) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QApplication::palette().highlight().color());

        int dotRadius = 2;
        int centerX = x + thumbSize / 2;
        int centerY = y - 2; // Positioned slightly above the label top

        painter->drawEllipse(
            QRect(centerX - dotRadius, centerY - dotRadius, dotRadius * 2, dotRadius * 2));
    }

    painter->restore();
}

QSize SnapshotTimelineDelegate::sizeHint(const QStyleOptionViewItem& option,
                                         const QModelIndex&          index) const {
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(c_thumbSize + 4,
                 c_thumbSize + c_labelHeight + c_spacing + (c_padding * 2) + c_bottomPadding +
                     (c_borderMargin * 2));
}
