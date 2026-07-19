#pragma once

#include <QPixmap>
#include <QVector>
#include <QWidget>

#include "thumbnailbutton.h"

#include <QHBoxLayout>
#include <QScrollArea>
#include <QTimer>
#include <QWheelEvent>

/// @brief Horizontal scrollable strip of image thumbnails.
class ThumbnailStrip : public QWidget {
    Q_OBJECT

  public:
    /// @brief Construct the thumbnail strip.
    /// @param parent Optional parent widget.
    explicit ThumbnailStrip(QWidget *parent = nullptr);

    /// @brief Update the full list of thumbnails.
    /// @param thumbnails Pre-scaled thumbnail pixmaps.
    /// @param labels     Human-readable labels for each thumbnail. Must match
    ///                   thumbnails in size (or be empty).
    void setThumbnails(const QVector<QPixmap>& thumbnails, const QVector<QString>& labels);

    /// @brief Set the active index.
    /// @param index Zero-based version index. Clamped to valid range.
    void setSelectedIndex(int index);

    /// @brief Returns true when no thumbnails are shown.
    bool isEmpty() const;

  protected:
    void wheelEvent(QWheelEvent *event) override;

  signals:
    /// @brief Emitted when a thumbnail is selected (by click or wheel).
    /// @param index Zero-based index of the selected thumbnail.
    void thumbnailSelected(int index);

  private:
    void buildStrip(const QVector<QPixmap>& thumbnails);
    void updateSelection(int oldIndex, int newIndex);
    void setThumbnailState(QLabel *lbl, const char *state);
    void doScrollToCurrent();

    int              m_currentIndex = -1;
    QVector<QString> m_labels;

    QScrollArea               *m_scrollArea = nullptr;
    QWidget                   *m_contentWidget = nullptr;
    QHBoxLayout               *m_contentLayout = nullptr;
    QVector<ThumbnailButton *> m_thumbnailWidgets;
    QTimer                     m_scrollTimer;
};
