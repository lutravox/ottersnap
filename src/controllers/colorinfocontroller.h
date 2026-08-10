#pragma once

#include <QObject>
#include <QPointF>
#include "controllers/imagesessioncontroller.h"
#include "core/imagesession.h"
#include "ui/colorinfo.h"
#include "ui/viewerstate.h"

/// @brief Controller for managing the color analysis overlay and cluster indicator.
class ColorInfoController : public QObject {
    Q_OBJECT

  public:
    explicit ColorInfoController(QObject *parent = nullptr);

    /// @brief Set whether the color info overlay is visible.
    void setVisible(bool visible);

    /// @brief Update the position of the cluster indicator.
    void updateIndicatorPos();

    /// @brief Link the controller to the session coordinator.
    void setSessionController(ImageSessionController *controller);
    void setViewerState(ViewerState *state);

    /// @brief Trigger a refresh of the indicator position.
    void updateIndicatorPosition();

    /// @brief Update the picked color in the overlay.
    void setPickedColor(const QColor& color);

    /// @brief Update the color clusters in the overlay.
    void setClusters(const QList<ColorAnalyzer::ColorCluster>& clusters);

  signals:
    /// @brief Signal emitted when a cluster color is selected.
    void colorSelected(const QColor& color);

  public slots:
    /// @brief Handle cluster selection from the UI.
    void onClusterSelected(const QVariantMap& data);

  public:
    /// @brief Get the current indicator position (normalized).
    QPointF currentIndicatorPos() const { return m_currentIndicatorPos; }

    /// @brief Get the current selected cluster color.
    QColor currentClusterColor() const { return m_currentClusterColor; }

  private slots:
    void onSessionColorClustersChanged();
    void onActiveSessionChanged(ImageSession *session);
    void onActiveSessionEffectsChanged();

  private:
    void resetClusterSelection();

    ImageSessionController *m_sessionController = nullptr;
    ViewerState            *m_viewerState = nullptr;
    ColorInfo              *m_colorInfo = nullptr;

    QPointF m_currentIndicatorPos;
    QColor  m_currentClusterColor;
    bool    m_clusterSelected = false;
    bool    m_visible = false;
};
