#pragma once

#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <memory>
#include "core/vksnapshotreconstructor.h"

class SnapshotItem : public QWidget {
    Q_OBJECT
  public:
    /**
     * @brief Constructs a new SnapshotItem widget.
     * @param index The index of the image in the snapshot list.
     * @param filename The base name of the image file.
     * @param fullPath The absolute path to the image file.
     * @param count The number of snapshots available for this image.
     * @param thumb The thumbnail image to display.
     * @param parent The parent widget.
     */
    SnapshotItem(int            index,
                 const QString& filename,
                 const QString& fullPath,
                 int            count,
                 const QImage&  thumb,
                 QWidget       *parent = nullptr);

    /**
     * @brief Returns the index of the image associated with this item.
     * @return The image index.
     */
    int index() const {
        return m_index;
    }

    /**
     * @brief Updates the selection state of the item and refreshes its style.
     * @param selected True to mark the item as selected, false otherwise.
     */
    void setSelected(bool selected);

  signals:
    /**
     * @brief Emitted when the item is clicked.
     * @param index The index of the image associated with this item.
     */
    void clicked(int index);

  protected:
    /**
     * @brief Handles mouse press events to trigger the clicked signal.
     * @param event The mouse event.
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief Handles the custom painting of the widget to support style sheets.
     * @param event The paint event.
     */
    void paintEvent(QPaintEvent *event) override;

  private:
    /**
     * @brief Updates the visual style based on the selection state.
     * @param selected Whether the item is currently selected.
     */
    void updateStyle(bool selected);
    int  m_index;
};

/// @brief Dialog for managing snapshots across all images.
class SnapshotManagerDialog : public QDialog {
    Q_OBJECT

  public:
    /**
     * @brief Constructs the Snapshot Manager Dialog.
     * @param parent The parent widget.
     */
    explicit SnapshotManagerDialog(QWidget *parent = nullptr);

  signals:
    /**
     * @brief Emitted when snapshots for a specific image are changed.
     * @param imagePath The path to the image whose snapshots were changed.
     */
    void snapshotChanged(const QString& imagePath);

    /**
     * @brief Emitted when a request is made to open a specific snapshot in the viewer.
     * @param path The path to the base image.
     * @param uuid The identity of the snapshot to open.
     */
    void openSnapshotRequested(const QString& path, const QUuid& uuid);

  private slots:
    /**
     * @brief Handles the selection of an image from the grid.
     * @param index The index of the selected image.
     */
    void onImageSelected(int index);

    /**
     * @brief Deletes all snapshots for the currently selected image.
     */
    void onClearImage();

    /**
     * @brief Requests to open the latest snapshot of the selected image in the viewer.
     */
    void onOpenInViewer();

  private:
    /**
     * @brief Populates and refreshes the image grid.
     */
    void updateImageGrid();

    /**
     * @brief Updates the details panel with information about the selected image.
     */
    void updateDetails();

    QScrollArea *m_imageScrollArea;
    QWidget     *m_imageGridWidget;
    QGridLayout *m_imageGrid;

    QLabel      *m_countLabel;
    QLabel      *m_storageLabel;
    QLabel      *m_pathLabel;
    QPushButton *m_btnClearImage;
    QPushButton *m_btnOpenInViewer;

    int                                      m_selectedImageIndex = -1;
    SnapshotItem                            *m_selectedItem = nullptr;
    std::shared_ptr<VkSnapshotReconstructor> m_reconstructor;
};
