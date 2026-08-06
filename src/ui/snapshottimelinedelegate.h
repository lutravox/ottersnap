#pragma once

#include <QStyledItemDelegate>

/**
 * @class SnapshotTimelineDelegate
 * @brief Custom item delegate for rendering snapshots in the timeline.
 *
 * This delegate handles the custom painting of thumbnails and their labels,
 * including hover and selection highlights.
 */
class SnapshotTimelineDelegate : public QStyledItemDelegate {
    Q_OBJECT

  public:
    explicit SnapshotTimelineDelegate(QObject *parent = nullptr);

    /**
     * @brief Renders the snapshot item.
     * @param painter The painter used to draw the item.
     * @param option Style options for the item.
     * @param index The model index of the item being painted.
     */
    void paint(QPainter                   *painter,
               const QStyleOptionViewItem& option,
               const QModelIndex&          index) const override;

    /**
     * @brief Returns the recommended size for a snapshot item.
     */
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /**
     * @brief Updates the currently selected index for custom rendering.
     * @param index The index of the selected snapshot.
     */
    void setCurrentIndex(int index) {
        m_currentIndex = index;
    }

    /**
     * @brief Updates the currently hovered index for custom rendering.
     * @param index The index of the hovered snapshot.
     */
    void setHoverIndex(int index) {
        m_hoverIndex = index;
    }

    /**
     * @brief Updates the secondary snapshot index for custom rendering.
     * @param index The index of the secondary snapshot.
     */
    void setSecondaryIndex(int index) {
        m_secondaryIndex = index;
    }

  private:
    static constexpr int c_thumbSize = 48;
    static constexpr int c_labelHeight = 16;
    static constexpr int c_spacing = 4;
    static constexpr int c_padding = 4;
    static constexpr int c_bottomPadding = 4;
    static constexpr int c_labelFontSize = 8;
    static constexpr int c_borderMargin = 4;

    int m_currentIndex = -1;
    int m_hoverIndex = -1;
    int m_secondaryIndex = -1;
};
