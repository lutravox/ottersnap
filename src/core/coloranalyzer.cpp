#include <QDebug>
#include <QPointF>
#include <algorithm>
#include <cmath>
#include <vector>
#include "core/coloranalyzer.h"

ColorAnalyzer::LabColor ColorAnalyzer::rgbToLab(const QColor& color) {
    float r = color.redF();
    float g = color.greenF();
    float b = color.blueF();

    auto linearize = [](float v) {
        return (v > 0.04045f) ? std::pow((v + 0.055f) / 1.055f, 2.4f) : (v / 12.92f);
    };

    r = linearize(r) * 100.0f;
    g = linearize(g) * 100.0f;
    b = linearize(b) * 100.0f;

    float x = r * 0.4124f + g * 0.3576f + b * 0.1805f;
    float y = r * 0.2126f + g * 0.7152f + b * 0.0722f;
    float z = r * 0.0193f + g * 0.1192f + b * 0.9505f;

    x /= 95.047f;
    y /= 100.0f;
    z /= 108.883f;

    auto f = [](float v) {
        return (v > 0.008856f) ? std::pow(v, 1.0f / 3.0f) : (7.787f * v) + (16.0f / 116.0f);
    };

    float L = (116.0f * f(y)) - 16.0f;
    float a = 500.0f * (f(x) - f(y));
    float b_val = 200.0f * (f(y) - f(z));

    return {L, a, b_val};
}

QColor ColorAnalyzer::labToRgb(const LabColor& lab) {
    float L = lab.L;
    float a = lab.a;
    float b = lab.b;

    float y = (L + 16.0f) / 116.0f;
    float x = a / 500.0f + y;
    float z = y - b / 200.0f;

    auto f = [](float v) {
        return (std::pow(v, 3) > 0.008856f) ? std::pow(v, 3) : (v - 16.0f / 116.0f) / 7.787f;
    };

    x = f(x) * 95.047f / 100.0f;
    z = f(z) * 108.883f / 100.0f;
    y = f(y);

    float r = x * 3.2406f + y * -1.5372f + z * -0.4986f;
    float g = x * -0.9689f + y * 1.8758f + z * 0.0415f;
    float b_val = x * 0.0557f + y * -0.2040f + z * 1.0570f;

    auto delinearize = [](float v) {
        v = std::clamp(v, 0.0f, 1.0f);
        return (v > 0.0031308f) ? 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f : v * 12.92f;
    };

    return QColor::fromRgbF(delinearize(r), delinearize(g), delinearize(b_val));
}

QList<ColorAnalyzer::ColorCluster> ColorAnalyzer::calculateClusters(const QImage& thumbnail) {
    if (thumbnail.isNull()) {
        return QList<ColorCluster>();
    }

    QImage img = thumbnail.convertToFormat(QImage::Format_ARGB32);
    int    width = img.width();
    int    height = img.height();

    struct Cluster {
        LabColor center;
        struct Member {
            LabColor lab;
            QPoint   pos;
        };
        std::vector<Member> members;
    };

    QList<Cluster> clusters;
    const int      sampleStep = 10;
    const float    distThreshold = 20.0f;

    for (int y = 0; y < height; y += sampleStep) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.scanLine(y));
        for (int x = 0; x < width; x += sampleStep) {
            QRgb rgb = line[x];
            if (qAlpha(rgb) == 0)
                continue;
            LabColor lab = rgbToLab(QColor(rgb));

            bool found = false;
            for (auto& cluster : clusters) {
                if (lab.distanceTo(cluster.center) < distThreshold) {
                    cluster.members.push_back({lab, QPoint(x, y)});
                    found = true;
                    break;
                }
            }

            if (!found) {
                clusters.append({lab, {{lab, QPoint(x, y)}}});
            }
        }
    }

    QList<ColorCluster> results;
    for (const auto& cluster : clusters) {
        if (cluster.members.size() < 2)
            continue;

        float sumL = 0, sumA = 0, sumB = 0;
        for (const auto& m : cluster.members) {
            sumL += m.lab.L;
            sumA += m.lab.a;
            sumB += m.lab.b;
        }

        LabColor avgLab = {sumL / cluster.members.size(),
                           sumA / cluster.members.size(),
                           sumB / cluster.members.size()};
        QColor   avgColor = labToRgb(avgLab);

        // Find the member closest to the average color to use as the representative samplePos
        float  minDist = std::numeric_limits<float>::max();
        QPoint closestPos = cluster.members[0].pos;
        for (const auto& m : cluster.members) {
            float dist = m.lab.distanceTo(avgLab);
            if (dist < minDist) {
                minDist = dist;
                closestPos = m.pos;
            }
        }

        // Calculate polar center for the wheel
        float   h = avgColor.hsvHueF();
        float   s = avgColor.hsvSaturationF();
        float   angle = (h - 0.5f) * 2.0f * M_PI;
        QPointF center(s * std::cos(angle), s * std::sin(angle));

        results.append({center,
                        avgColor,
                        static_cast<int>(cluster.members.size()),
                        QPointF(static_cast<float>(closestPos.x()) / width,
                                static_cast<float>(closestPos.y()) / height)});
    }

    qDebug() << "[ColorAnalyzer] Calculated" << results.length() << "color clusters";
    return results;
}
