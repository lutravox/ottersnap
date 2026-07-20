#pragma once

#include <QAction>
#include <QObject>

class ImageTab;
class VkImageViewer;

/// @brief Coordinates the synchronization of image effects between the state (ImageTab),
/// the UI (QActions), and the renderer (VkImageViewer).
class EffectsController : public QObject {
    Q_OBJECT

  public:
    explicit EffectsController(QObject *parent = nullptr);

    /// @brief Initialize the controller with the UI elements and viewer.
    void setup(VkImageViewer *viewer, QAction *grayscaleAction, QAction *mirrorAction);

    /// @brief Set the current tab being managed. Syncs UI and viewer to tab state.
    void setTargetTab(ImageTab *tab);

    /// @brief Toggle grayscale mode.
    void toggleGrayscale();

    /// @brief Toggle mirror mode.
    void toggleMirror();

    /// @brief Update the grayscale mode across all synchronized components.
    void setGrayscale(bool enabled);

    /// @brief Update the mirror mode across all synchronized components.
    void setMirror(bool enabled);

  private:
    void syncFromTab();

    VkImageViewer *m_viewer = nullptr;
    ImageTab      *m_currentTab = nullptr;
    QAction       *m_grayscaleAction = nullptr;
    QAction       *m_mirrorAction = nullptr;
};
