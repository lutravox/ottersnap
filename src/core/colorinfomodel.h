#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QVariantList>
#include "core/coloranalyzer.h"

/// @brief Reactive view-model backing the color info overlay.
///
/// Owns the state the QML overlay displays (picked color, cluster
/// distribution, selection, visibility).
class ColorInfoModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString hexColor READ hexColor WRITE setHexColor NOTIFY hexColorChanged)
    Q_PROPERTY(qreal alphaValue READ alphaValue WRITE setAlphaValue NOTIFY alphaValueChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QVariantList clusters READ clusters WRITE setClusters NOTIFY clustersChanged)
    Q_PROPERTY(int selectedClusterId READ selectedClusterId WRITE setSelectedClusterId NOTIFY
                   selectedClusterIdChanged)

  public:
    /// @brief Construct the model.
    /// @param parent Optional parent object (the owning ColorInfoController).
    explicit ColorInfoModel(QObject *parent = nullptr);

    /// @brief Get the picked color as a hex string (e.g. "#ff8800").
    QString hexColor() const {
        return m_hexColor;
    }

    /// @brief Set the picked color hex string; emits hexColorChanged() on change.
    void setHexColor(const QString& hex);

    /// @brief Get the picked color alpha in the [0, 1] range.
    qreal alphaValue() const {
        return m_alphaValue;
    }

    /// @brief Set the picked color alpha in the [0, 1] range.
    void setAlphaValue(qreal alpha);

    /// @brief Whether the overlay is shown.
    bool visible() const {
        return m_visible;
    }

    /// @brief Show or hide the overlay.
    void setVisible(bool visible);

    /// @brief Cluster rows for the overlay (maps with center, color, count,
    ///        samplePos, id keys).
    QVariantList clusters() const {
        return m_clusters;
    }

    /// @brief Replace the cluster rows; emits clustersChanged() on change.
    void setClusters(const QVariantList& clusters);

    /// @brief Id of the selected cluster, or -1 for none.
    int selectedClusterId() const {
        return m_selectedClusterId;
    }

    /// @brief Set the selected cluster id (-1 clears the selection).
    void setSelectedClusterId(int id);

    /// @brief Set the picked color (hex + alpha) in one call.
    void setPickedColor(const QColor& color);

    /// @brief Populate the cluster distribution from analyzer results.
    void setClusters(const QList<ColorAnalyzer::ColorCluster>& clusters);

    /// @brief Clear the current cluster selection.
    void resetSelection();

    /// @brief Report a cluster click from the overlay.
    Q_INVOKABLE void handleClusterSelected(const QVariantMap& data);

    /// @brief Copy text to the system clipboard.
    Q_INVOKABLE void copyToClipboard(const QString& text);

    /// @brief Show a context menu at the cursor offering to copy @p text.
    Q_INVOKABLE void showCopyMenu(const QString& text);

  signals:
    /// @brief Emitted when the picked color hex string changes.
    void hexColorChanged();

    /// @brief Emitted when the picked color alpha changes.
    void alphaValueChanged();

    /// @brief Emitted when the overlay visibility changes.
    void visibleChanged();

    /// @brief Emitted when the cluster rows change.
    void clustersChanged();

    /// @brief Emitted when the selected cluster id changes.
    void selectedClusterIdChanged();

    /// @brief Emitted when the overlay reports a cluster click.
    void clusterSelected(const QVariantMap& data);

  private:
    QString      m_hexColor = "#FFFFFF";
    qreal        m_alphaValue = 1.0;
    bool         m_visible = false;
    QVariantList m_clusters;
    int          m_selectedClusterId = -1;
};
