#include "core/colorinfomodel.h"

#include <QAction>
#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QMenu>

ColorInfoModel::ColorInfoModel(QObject *parent) : QObject(parent) {
}

void ColorInfoModel::setHexColor(const QString& hex) {
    if (m_hexColor == hex)
        return;
    m_hexColor = hex;
    emit hexColorChanged();
}

void ColorInfoModel::setAlphaValue(qreal alpha) {
    if (m_alphaValue == alpha)
        return;
    m_alphaValue = alpha;
    emit alphaValueChanged();
}

void ColorInfoModel::setVisible(bool visible) {
    if (m_visible == visible)
        return;
    m_visible = visible;
    emit visibleChanged();
}

void ColorInfoModel::setClusters(const QVariantList& clusters) {
    if (m_clusters == clusters)
        return;
    m_clusters = clusters;
    emit clustersChanged();
}

void ColorInfoModel::setSelectedClusterId(int id) {
    if (m_selectedClusterId == id)
        return;
    m_selectedClusterId = id;
    emit selectedClusterIdChanged();
}

void ColorInfoModel::setPickedColor(const QColor& color) {
    setHexColor(color.name());
    setAlphaValue(color.alphaF());
}

void ColorInfoModel::setClusters(const QList<ColorAnalyzer::ColorCluster>& clusters) {
    QVariantList list;
    for (int i = 0; i < clusters.size(); ++i) {
        const ColorAnalyzer::ColorCluster& cluster = clusters[i];

        QVariantMap clusterData;
        clusterData.insert("center", cluster.center);
        clusterData.insert("color", cluster.color);
        clusterData.insert("count", cluster.count);
        clusterData.insert("samplePos", cluster.samplePos);
        clusterData.insert("id", i);

        list.append(clusterData);
    }
    setClusters(list);
}

void ColorInfoModel::resetSelection() {
    setSelectedClusterId(-1);
}

void ColorInfoModel::handleClusterSelected(const QVariantMap& data) {
    setSelectedClusterId(data["id"].toInt());
    emit clusterSelected(data);
}

void ColorInfoModel::copyToClipboard(const QString& text) {
    QGuiApplication::clipboard()->setText(text);
}

void ColorInfoModel::showCopyMenu(const QString& text) {
    QMenu menu;
    QAction *copyAction = menu.addAction(tr("Copy"));
    connect(copyAction, &QAction::triggered, this, [text]() {
        QGuiApplication::clipboard()->setText(text);
    });
    menu.exec(QCursor::pos());
}

