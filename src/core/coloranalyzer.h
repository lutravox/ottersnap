#pragma once

#include <QColor>
#include <QImage>
#include <QPointF>
#include <cmath>

/// @brief Utility for analyzing color distributions in images.
class ColorAnalyzer {
  public:
    struct ColorCluster {
        QPointF center;
        QColor  color;
        int     count;
        QPointF samplePos;
    };

    /// @brief Calculates dominant color clusters in the image using LAB space.
    /// @param thumbnail The image to analyze.
    /// @return A list of detected dominant color clusters.
    static QList<ColorCluster> calculateClusters(const QImage& thumbnail);

  private:
    struct LabColor {
        float L, a, b;
        float distanceTo(const LabColor& other) const {
            float dL = L - other.L;
            float da = a - other.a;
            float db = b - other.b;
            return std::sqrt(dL * dL + da * da + db * db);
        }
    };

    static LabColor rgbToLab(const QColor& color);
    static QColor   labToRgb(const LabColor& lab);
};
