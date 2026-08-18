#include <QColor>
#include <QImage>
#include <QtTest>
#include "controllers/colorinfocontroller.h"
#include "controllers/imagesessioncontroller.h"
#include "controllers/appsettingscontroller.h"
#include "core/imagesession.h"
#include "ui/viewermodel.h"

class TestColorInfoController : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void testSetup();
    void testSetVisible();
    void testSetClusters();
    void testOnClusterSelected();
};

void TestColorInfoController::initTestCase() {
}

void TestColorInfoController::testSetup() {
    AppSettingsController settings;
    ImageSessionController sessionController(&settings);
    ColorInfoController    controller;
    ImageSession           session;
    ViewerModel            state;

    controller.setSessionController(&sessionController);
    sessionController.setActiveSession(&session);
    controller.setViewerModel(&state);

    // No crash and basic setup done
}

void TestColorInfoController::testSetVisible() {
    ColorInfoController controller;
    ViewerModel         state;
    controller.setViewerModel(&state);

    controller.setVisible(true);
    // Verify it doesn't crash.
    controller.setVisible(false);
}

void TestColorInfoController::testSetClusters() {
    ColorInfoController controller;
    ViewerModel         state;
    controller.setViewerModel(&state);
    controller.setVisible(true);

    QList<ColorAnalyzer::ColorCluster> clusters;
    clusters.append({QPointF(0, 0), Qt::red, 10, QPointF(0, 0)});

    controller.setClusters(clusters);
    // Verify it doesn't crash.
}

void TestColorInfoController::testOnClusterSelected() {
    AppSettingsController settings;
    ImageSessionController sessionController(&settings);
    ColorInfoController    controller;
    ImageSession           session;
    ViewerModel            state;

    controller.setSessionController(&sessionController);
    sessionController.setActiveSession(&session);
    controller.setViewerModel(&state);

    QVariantMap data;
    data.insert("samplePos", QPointF(0.5, 0.5));
    data.insert("color", QColor(Qt::blue));

    controller.onClusterSelected(data);

    QCOMPARE(controller.currentIndicatorPos(), QPointF(0.5, 0.5));
    QCOMPARE(controller.currentClusterColor(), QColor(Qt::blue));
}

QTEST_MAIN(TestColorInfoController)
#include "test_colorinfocontroller.moc"
