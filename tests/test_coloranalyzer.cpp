#include <QtTest>
#include <QImage>
#include <QColor>
#include "core/coloranalyzer.h"

class TestColorAnalyzer : public QObject {
    Q_OBJECT

  private slots:
    void testCalculateClusters_NullImage();
    void testCalculateClusters_SingleColor();
    void testCalculateClusters_DistinctColors();
    void testCalculateClusters_CoordinateNormalization();
};

void TestColorAnalyzer::testCalculateClusters_NullImage() {
    QImage img;
    auto clusters = ColorAnalyzer::calculateClusters(img);
    QVERIFY(clusters.isEmpty());
}

void TestColorAnalyzer::testCalculateClusters_SingleColor() {
    QImage img(100, 100, QImage::Format_RGB32);
    img.fill(Qt::red);

    auto clusters = ColorAnalyzer::calculateClusters(img);

    // Should find at least one cluster
    QVERIFY(!clusters.isEmpty());
    
    // The dominant color should be close to red
    bool foundRed = false;
    QColor red = QColor(Qt::red);
    for (const auto& c : clusters) {
        if (qAbs(c.color.red() - red.red()) < 5 &&
            qAbs(c.color.green() - red.green()) < 5 &&
            qAbs(c.color.blue() - red.blue()) < 5) {
            foundRed = true;
            break;
        }
    }
    QVERIFY(foundRed);
}

void TestColorAnalyzer::testCalculateClusters_DistinctColors() {
    QImage img(100, 100, QImage::Format_RGB32);
    // Top half red, bottom half blue
    for (int y = 0; y < 100; ++y) {
        QColor color = (y < 50) ? Qt::red : Qt::blue;
        for (int x = 0; x < 100; ++x) {
            img.setPixel(x, y, color.rgb());
        }
    }

    auto clusters = ColorAnalyzer::calculateClusters(img);

    // Should identify at least two distinct clusters
    QVERIFY(clusters.size() >= 2);

    bool foundRed = false;
    bool foundBlue = false;
    QColor red = QColor(Qt::red);
    QColor blue = QColor(Qt::blue);
    for (const auto& c : clusters) {
        if (qAbs(c.color.red() - red.red()) < 5 &&
            qAbs(c.color.green() - red.green()) < 5 &&
            qAbs(c.color.blue() - red.blue()) < 5) foundRed = true;
        if (qAbs(c.color.red() - blue.red()) < 5 &&
            qAbs(c.color.green() - blue.green()) < 5 &&
            qAbs(c.color.blue() - blue.blue()) < 5) foundBlue = true;
    }
    QVERIFY(foundRed);
    QVERIFY(foundBlue);
}

void TestColorAnalyzer::testCalculateClusters_CoordinateNormalization() {
    QImage img(100, 100, QImage::Format_RGB32);
    img.fill(Qt::black);
    // Single white pixel at (25, 75)
    img.setPixel(25, 75, qRgb(255, 255, 255));

    auto clusters = ColorAnalyzer::calculateClusters(img);

    // We expect a cluster for the white pixel (or the black background)
    for (const auto& c : clusters) {
        if (c.color == Qt::white) {
            // Normalized coordinates should be 0.25, 0.75
            QCOMPARE(c.samplePos.x(), 0.25);
            QCOMPARE(c.samplePos.y(), 0.75);
        }
    }
}

QTEST_MAIN(TestColorAnalyzer)
#include "test_coloranalyzer.moc"
