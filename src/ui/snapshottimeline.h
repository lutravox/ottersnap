#pragma once

#include <QPixmap>
#include <QPushButton>
#include <QVector>
#include <QWidget>

#include "snapshottab.h"

#include <QHBoxLayout>
#include <QScrollArea>
#include <QTimer>
#include <QWheelEvent>

/// @brief Horizontal scrollable strip of snapshot tabs.
class SnapshotTimeline : public QWidget {
    Q_OBJECT

  public:
    /// @brief Construct the snapshot timeline.
    /// @param parent Optional parent widget.
    explicit SnapshotTimeline(QWidget *parent = nullptr);

    /// @brief Update the full list of snapshot thumbnails.
    /// @param thumbnails Pre-scaled thumbnail pixmaps.
    /// @param labels     Human-readable labels for each thumbnail. Must match
    ///                   thumbnails in size (or be empty).
    void setThumbnails(const QVector<QPixmap>& thumbnails, const QVector<QString>& labels);

    /// @brief Set the active index.
    /// @param index Zero-based version index. Clamped to valid range.
    void setSelectedIndex(int index);

    /// @brief Update a single thumbnail without rebuilding the timeline.
    void updateThumbnail(int index, const QPixmap& pixmap);

    /// @brief Returns true when no thumbnails are shown.
    bool isEmpty() const;

    /// @brief Handle events from child widgets.
    bool eventFilter(QObject *obj, QEvent *event) override;

  protected:
    void wheelEvent(QWheelEvent *event) override;

  signals:
    /// @brief Emitted when a snapshot is selected (by click or wheel).
    /// @param index Zero-based index of the selected snapshot.
    void snapshotSelected(int index);

    /// @brief Emitted when the create snapshot button is clicked.
    void createSnapshotRequested();

    /// @brief Emitted when a snapshot deletion is requested via context menu.
    /// @param index Zero-based index of the snapshot to delete.
    void snapshotDeletionRequested(int index);

  private:
    void buildStrip(const QVector<QPixmap>& thumbnails);
    void updateSelection(int oldIndex, int newIndex);
    void updateTabState(int index, bool selected);
    void setThumbnailState(QLabel *lbl, const char *state);
    void doScrollToCurrent();

    int              m_currentIndex = -1;
    QVector<QString> m_labels;

    QScrollArea           *m_scrollArea = nullptr;
    QWidget               *m_contentWidget = nullptr;
    QHBoxLayout           *m_contentLayout = nullptr;
    QVector<SnapshotTab *> m_snapshottabs;
    QVector<QLabel *>      m_snapshotLabels;
    QVector<QWidget *>     m_containers;
    QTimer                 m_scrollTimer;
    QWidget               *m_createButtonWrapper = nullptr;
    QPushButton           *m_createButton = nullptr;
};
