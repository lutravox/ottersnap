#pragma once

#include <QObject>
#include <QPointF>
#include "controllers/imagesessioncontroller.h"
#include "core/clusterindicatormodel.h"
#include "core/colorinfomodel.h"
#include "core/imagesession.h"
#include "ui/viewermodel.h"

/// @brief Controller for managing the color analysis overlay and cluster indicator.
class ColorInfoController : public QObject {
    Q_OBJECT

  public:
    ~ColorInfoController() override;
    explicit ColorInfoController(QObject *parent = nullptr);

    /// @brief Set whether the color info overlay is visible.
    void setVisible(bool visible);

    /// @brief Update the position of the cluster indicator.
    void updateIndicatorPos();

    /// @brief Link the controller to the session coordinator.
    void setSessionController(ImageSessionController *controller);
    void setViewerModel(ViewerModel *state);

    /// @brief Trigger a refresh of the indicator position.
    void updateIndicatorPosition();

    /// @brief Update the picked color in the overlay.
    void setPickedColor(const QColor& color);

    /// @brief Update the color clusters in the overlay.
    void setClusters(const QList<ColorAnalyzer::ColorCluster>& clusters);

    /// @brief Returns the cluster indicator model owned by this controller and
    ///        exposed to the viewer's QML overlay.
    ClusterIndicatorModel *indicatorModel() const { return m_indicatorModel; }

    /// @brief Returns the color info model owned by this controller and exposed
    ///        to the viewer's QML overlay.
    ColorInfoModel *colorInfoModel() const { return m_colorInfoModel; }

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
    ViewerModel            *m_viewerState = nullptr;
    ColorInfoModel         *m_colorInfoModel = nullptr;
    ClusterIndicatorModel  *m_indicatorModel = nullptr;

    QMetaObject::Connection m_sessionImageConnection;
    QPointF m_currentIndicatorPos;
    QColor  m_currentClusterColor;
    bool    m_clusterSelected = false;
    bool    m_visible = false;
};
