#include <QtTest>
#include "core/viewmodel.h"

class TestViewModel : public QObject {
    Q_OBJECT

  private slots:
    // Initialization
    void testDefaults();
    void testResetState();

    // FitScale
    void testFitScaleLandscape();
    void testFitScalePortrait();
    void testFitScaleSquare();

    // Zoom percentage
    void testPercentage();
    void testSetPercentage();
    void testSetPercentageClampLow();
    void testSetPercentageClampHigh();

    // Wheel zoom
    void testWheelZoomIn();
    void testWheelZoomOut();
    void testWheelZoomCtrl();
    void testWheelZoomClampLow();
    void testWheelZoomClampHigh();

    // Pan
    void testPanDelta();
    void testPanZoomIndependent();

    // Screen to Pixel
    void testScreenToPixelBasic();
    void testScreenToPixelZoomed();
    void testScreenToPixelPanned();
    void testScreenToPixelOutOfBounds();
    void testPixelToScreenBasic();
    void testPixelToScreenZoomed();
    void testPixelToScreenPanned();
    void testCoordinateRoundTrip();

    // Fit / Reset
    void testFitToWindow();
    void testFitResetsPan();
    void testResizePreservesZoomRatio();

    // No-image guard
    void testNoImageSetPercentage();
    void testNoImageWheelZoom();
    void testNoImagePan();
    void testNoImageFit();
    void testUpdateImageSize();
    void testUpdateZoomRatio();
    void testNormalizedToScreen();
    void testViewportGetters();
};

// Initialization

void TestViewModel::testDefaults() {
    ViewModel zs;
    QVERIFY(!zs.hasImage());
    QCOMPARE(zs.imageWidth(), 0);
    QCOMPARE(zs.imageHeight(), 0);
    QCOMPARE(zs.zoom(), 1.0f);
    QCOMPARE(zs.fitScale(), 0.0f);
    QCOMPARE(zs.zoomRatio(), 1.0f);
    QCOMPARE(zs.percentage(), 100.0);
}

void TestViewModel::testResetState() {
    ViewModel zs;
    zs.resetState(1920, 1080);
    QVERIFY(zs.hasImage());
    QCOMPARE(zs.imageWidth(), 1920);
    QCOMPARE(zs.imageHeight(), 1080);
    // Should reset state
    QCOMPARE(zs.zoom(), 1.0f);
    QCOMPARE(zs.pan(), QPointF(0, 0));
}

// FitScale

void TestViewModel::testFitScaleLandscape() {
    // Image 1920x1080 (16:9), viewport 800x600 (4:3)
    // width ratio: 800/1920 = 0.4167, height ratio: 600/1080 = 0.5556
    // fitScale = min = 0.4167 (width-limited)
    ViewModel zs;
    zs.resetState(1920, 1080);
    zs.setViewportSize(800, 600);
    QVERIFY(qFuzzyCompare(zs.fitScale(), 800.0f / 1920.0f));
}

void TestViewModel::testFitScalePortrait() {
    // Image 1080x1920 (9:16), viewport 800x600 (4:3)
    // width ratio: 800/1080 = 0.7407, height ratio: 600/1920 = 0.3125
    // fitScale = min = 0.3125 (height-limited)
    ViewModel zs;
    zs.resetState(1080, 1920);
    zs.setViewportSize(800, 600);
    QVERIFY(qFuzzyCompare(zs.fitScale(), 600.0f / 1920.0f));
}

void TestViewModel::testFitScaleSquare() {
    // Image 1000x1000, viewport 800x800 -> fitScale = 0.8
    ViewModel zs;
    zs.resetState(1000, 1000);
    zs.setViewportSize(800, 800);
    QVERIFY(qFuzzyCompare(zs.fitScale(), 0.8f));
}

// Zoom percentage

void TestViewModel::testPercentage() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    zs.setViewportSize(800, 600);
    // fitScale = 0.6, zoom = 0.6, percentage = 60
    QCOMPARE(zs.percentage(), zs.zoom() * 100.0);
}

void TestViewModel::testSetPercentage() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    zs.setViewportSize(800, 600);
    zs.setPercentage(200.0);
    QCOMPARE(zs.zoom(), 2.0f);
    QCOMPARE(zs.percentage(), 200.0);
}

void TestViewModel::testSetPercentageClampLow() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    zs.setPercentage(1.0);
    // Should clamp to 5% (0.05)
    QCOMPARE(zs.zoom(), 0.05f);
}

void TestViewModel::testSetPercentageClampHigh() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    zs.setPercentage(10000.0);
    // Should clamp to 6400% (64.0)
    QCOMPARE(zs.zoom(), 64.0f);
}

// Wheel zoom

void TestViewModel::testWheelZoomIn() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    float initial = zs.zoom();
    zs.applyWheelZoom(true);
    QVERIFY(zs.zoom() > initial);
    QVERIFY(qFuzzyCompare(zs.zoom(), initial * 1.1f));
}

void TestViewModel::testWheelZoomOut() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    float initial = zs.zoom();
    zs.applyWheelZoom(false);
    QVERIFY(zs.zoom() < initial);
    QVERIFY(qFuzzyCompare(zs.zoom(), initial * 0.9f));
}

void TestViewModel::testWheelZoomCtrl() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    float initial = zs.zoom();
    zs.applyWheelZoom(true, true);
    // Ctrl squares the factor: 1.1^2 = 1.21
    QVERIFY(qFuzzyCompare(zs.zoom(), initial * 1.21f));
}

void TestViewModel::testWheelZoomClampLow() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    // Zoom out 100 times — should clamp at 0.05
    for (int i = 0; i < 100; ++i)
        zs.applyWheelZoom(false);
    QCOMPARE(zs.zoom(), 0.05f);
}

void TestViewModel::testWheelZoomClampHigh() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    // Zoom in 100 times — should clamp at 64.0
    for (int i = 0; i < 100; ++i)
        zs.applyWheelZoom(true);
    QCOMPARE(zs.zoom(), 64.0f);
}

// Pan

void TestViewModel::testPanDelta() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    zs.setViewportSize(500, 500);
    // fitScale = 0.5, zoom = 1.0
    zs.applyPanDelta(10, 20);
    // pan.rx() -= 10 * (1/1000) / 1.0 = -0.01
    // pan.ry() -= 20 * (1/1000) / 1.0 = -0.02
    QVERIFY(qFuzzyCompare(static_cast<float>(zs.pan().rx()), -0.01f));
    QVERIFY(qFuzzyCompare(static_cast<float>(zs.pan().ry()), -0.02f));
}

void TestViewModel::testPanZoomIndependent() {
    // Same drag at 1x zoom and 2x zoom should produce
    // different image-space pan (larger at higher zoom),
    // but the visual screen-pixel movement is the same.
    ViewModel zs1;
    zs1.resetState(1000, 1000);
    zs1.setViewportSize(1000, 1000); // fitScale=1, zoom=1
    zs1.setPercentage(100.0);        // zoom = 1.0
    zs1.applyPanDelta(10, 0);

    ViewModel zs2;
    zs2.resetState(1000, 1000);
    zs2.setViewportSize(1000, 1000);
    zs2.setPercentage(200.0); // zoom = 2.0
    zs2.applyPanDelta(10, 0);

    // At 2x zoom, same 10px drag moves half the image-space distance
    QVERIFY(qFuzzyCompare(static_cast<float>(zs2.pan().rx()),
                          static_cast<float>(zs1.pan().rx()) * 0.5f));
}

void TestViewModel::testScreenToPixelBasic() {
    ViewModel zs;
    zs.resetState(100, 100);
    zs.setViewportSize(100, 100); // fitScale = 1.0, zoom = 1.0, pan = (0,0)

    QCOMPARE(zs.screenToPixel(QPointF(50, 50)), QPoint(50, 50));
    QCOMPARE(zs.screenToPixel(QPointF(0, 0)), QPoint(0, 0));
    QCOMPARE(zs.screenToPixel(QPointF(99, 99)), QPoint(99, 99));
}

void TestViewModel::testScreenToPixelZoomed() {
    ViewModel zs;
    zs.resetState(100, 100);
    zs.setViewportSize(100, 100);
    zs.setPercentage(200.0); // zoom = 2.0, fitScale = 1.0

    // center remains center
    QCOMPARE(zs.screenToPixel(QPointF(50, 50)), QPoint(50, 50));
    // (0,0) screen -> 0.25 image UV -> pixel 25
    QCOMPARE(zs.screenToPixel(QPointF(0, 0)), QPoint(25, 25));
    // (100,100) screen -> 0.75 image UV -> pixel 75
    QCOMPARE(zs.screenToPixel(QPointF(100, 100)), QPoint(75, 75));
}

void TestViewModel::testScreenToPixelPanned() {
    ViewModel zs;
    zs.resetState(100, 100);
    zs.setViewportSize(100, 100);
    // pan = (0.1, 0.1)
    // we can't set pan directly, so we applyPanDelta
    // applyPanDelta(dx, dy) -> pan -= dx * invImgW / zoom
    // To get pan = (0.1, 0.1), we need -dx * (1/100) / 1.0 = 0.1 => dx = -10
    zs.applyPanDelta(-10, -10);

    // Screen (50,50) -> UV 0.5 -> 0.5 + 0.1 = 0.6 -> pixel 60
    QCOMPARE(zs.screenToPixel(QPointF(50, 50)), QPoint(60, 60));
}

void TestViewModel::testScreenToPixelOutOfBounds() {
    ViewModel zs;
    zs.resetState(100, 100);
    zs.setViewportSize(100, 100);

    QCOMPARE(zs.screenToPixel(QPointF(-1, -1)), QPoint(-1, -1));
    QCOMPARE(zs.screenToPixel(QPointF(101, 101)), QPoint(-1, -1));
}

void TestViewModel::testPixelToScreenBasic() {
    ViewModel zs;
    zs.resetState(100, 100);
    zs.setViewportSize(100, 100); // fitScale = 1, zoom = 1, pan = (0,0)

    QCOMPARE(zs.pixelToScreen(QPoint(50, 50)), QPointF(50, 50));
    QCOMPARE(zs.pixelToScreen(QPoint(0, 0)), QPointF(0, 0));
    QCOMPARE(zs.pixelToScreen(QPoint(99, 99)), QPointF(99, 99));
}

void TestViewModel::testPixelToScreenZoomed() {
    ViewModel zs;
    zs.resetState(100, 100);
    zs.setViewportSize(100, 100);
    zs.setPercentage(200.0); // zoom = 2.0, fitScale = 1.0

    // center remains center
    QCOMPARE(zs.pixelToScreen(QPoint(50, 50)), QPointF(50, 50));
    // pixel 25 -> 0.25 image UV -> screen 0
    QCOMPARE(zs.pixelToScreen(QPoint(25, 25)), QPointF(0, 0));
    // pixel 75 -> 0.75 image UV -> screen 100
    QCOMPARE(zs.pixelToScreen(QPoint(75, 75)), QPointF(100, 100));
}

void TestViewModel::testPixelToScreenPanned() {
    ViewModel zs;
    zs.resetState(100, 100);
    zs.setViewportSize(100, 100);
    // pan = (0.1, 0.1)
    zs.applyPanDelta(-10, -10);

    QPointF res = zs.pixelToScreen(QPoint(60, 60));
    QVERIFY(std::abs(res.x() - 50.0) < 1e-5);
    QVERIFY(std::abs(res.y() - 50.0) < 1e-5);
}

void TestViewModel::testCoordinateRoundTrip() {
    ViewModel zs;
    zs.resetState(1920, 1080);
    zs.setViewportSize(1280, 720);

    auto checkTrip = [&](int px, int py, double zoom, QPointF panDelta) {
        zs.resetState(1920, 1080);
        zs.setViewportSize(1280, 720);
        zs.setPercentage(zoom);

        zs.applyPanDelta(static_cast<int>(panDelta.x()), static_cast<int>(panDelta.y()));

        QPoint  p(px, py);
        QPointF s = zs.pixelToScreen(p);
        QPoint  p2 = zs.screenToPixel(s);

        QCOMPARE(p, p2);
    };

    checkTrip(0, 0, 100.0, QPointF(-100, -150));
    checkTrip(1919, 1079, 100.0, QPointF(-100, -150));
    checkTrip(960, 540, 200.0, QPointF(-100, -150));
    checkTrip(100, 100, 50.0, QPointF(-100, -150));
}

// Fit / Reset

void TestViewModel::testFitToWindow() {
    ViewModel zs;
    zs.resetState(2000, 1500);
    zs.setViewportSize(800, 600);
    // fitScale = min(800/2000, 600/1500) = 0.4
    zs.setPercentage(200.0); // zoom away from fit
    zs.fitToWindow();
    QCOMPARE(zs.zoomRatio(), 1.0f);
    QVERIFY(qFuzzyCompare(zs.zoom(), zs.fitScale()));
}

void TestViewModel::testFitResetsPan() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    zs.setViewportSize(800, 600);
    zs.applyPanDelta(50, 30);
    QVERIFY(zs.pan() != QPointF(0, 0));
    zs.fitToWindow();
    QCOMPARE(zs.pan(), QPointF(0, 0));
}

void TestViewModel::testResizePreservesZoomRatio() {
    ViewModel zs;
    zs.resetState(2000, 1000);     // 2:1
    zs.setViewportSize(1000, 500); // fitScale = 0.5
    zs.setPercentage(200.0);       // zoom = 2.0, zoomRatio = 4.0
    float ratioBefore = zs.zoomRatio();

    // Resize viewport
    zs.setViewportSize(500, 250); // fitScale = 0.25

    // In the MVC architecture, the controller explicitly calls this
    // when 'Scale with Window' is enabled.
    zs.updateZoomForRelativeScaling();

    QCOMPARE(zs.zoomRatio(), ratioBefore);
    // zoom should have changed to match new fitScale
    QVERIFY(qFuzzyCompare(zs.zoom(), 0.25f * ratioBefore));
}

// No-image guard

void TestViewModel::testNoImageSetPercentage() {
    ViewModel zs;
    float     before = zs.zoom();
    zs.setPercentage(500.0);
    QCOMPARE(zs.zoom(), before); // no-op
}

void TestViewModel::testNoImageWheelZoom() {
    ViewModel zs;
    float     before = zs.zoom();
    zs.applyWheelZoom(true);
    QCOMPARE(zs.zoom(), before); // no-op
}

void TestViewModel::testNoImagePan() {
    ViewModel zs;
    QPointF   before = zs.pan();
    zs.applyPanDelta(100, 100);
    QCOMPARE(zs.pan(), before); // no-op
}

void TestViewModel::testNoImageFit() {
    ViewModel zs;
    zs.fitToWindow(); // should not crash, no-op
}

void TestViewModel::testUpdateImageSize() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    zs.setViewportSize(500, 500); // fitScale = 0.5

    zs.updateImageSize(2000, 2000);
    QCOMPARE(zs.imageWidth(), 2000);
    QCOMPARE(zs.imageHeight(), 2000);
    // fitScale should be updated: 500/2000 = 0.25
    QVERIFY(qFuzzyCompare(zs.fitScale(), 0.25f));
}

void TestViewModel::testUpdateZoomRatio() {
    ViewModel zs;
    zs.resetState(1000, 1000);
    zs.setViewportSize(500, 500); // fitScale = 0.5
    zs.setPercentage(200.0);      // zoom = 2.0

    zs.updateZoomRatio();
    // zoomRatio = zoom / fitScale = 2.0 / 0.5 = 4.0
    QCOMPARE(zs.zoomRatio(), 4.0f);
}

void TestViewModel::testNormalizedToScreen() {
    ViewModel zs;
    zs.resetState(100, 100);
    zs.setViewportSize(100, 100); // fitScale = 1, zoom = 1, pan = (0,0)

    // (0.5, 0.5) norm -> (50, 50)
    QCOMPARE(zs.normalizedToScreen(QPointF(0.5, 0.5)), QPointF(50, 50));
    // (0, 0) norm -> (0, 0) screen
    QCOMPARE(zs.normalizedToScreen(QPointF(0, 0)), QPointF(0, 0));
    // (1, 1) norm -> (100, 100) screen
    QCOMPARE(zs.normalizedToScreen(QPointF(1, 1)), QPointF(100, 100));

    // Test with zoom
    zs.setPercentage(200.0); // zoom = 2.0, fitScale = 1.0
    // (0.5, 0.5) norm -> center -> (50, 50)
    QCOMPARE(zs.normalizedToScreen(QPointF(0.5, 0.5)), QPointF(50, 50));
    // (0, 0) norm -> relX = -0.5 -> screenOffsetX = -0.5 * (2 * 100) = -100 -> screenX = 50 - 100 =
    // -50
    QCOMPARE(zs.normalizedToScreen(QPointF(0, 0)), QPointF(-50, -50));
}

void TestViewModel::testViewportGetters() {
    ViewModel zs;
    zs.resetState(100, 100);
    zs.setViewportSize(1280, 720);
    QCOMPARE(zs.viewportWidth(), 1280);
    QCOMPARE(zs.viewportHeight(), 720);
}

QTEST_MAIN(TestViewModel)
#include "test_viewmodel.moc"
