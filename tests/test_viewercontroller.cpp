#include <QCoreApplication>
#include <QtTest>
#include <QSignalSpy>
#include "controllers/appsettingscontroller.h"
#include "controllers/imagesessioncontroller.h"
#include "controllers/viewercontroller.h"
#include "core/imagesession.h"
#include "core/viewer_interfaces.h"
#include "core/viewstate.h"
#include "core/snapshotdb.h"
#include "core/vulkancontext.h"

/// Mock implementation of IViewer to verify ViewController interactions.
class MockViewer : public IViewer {
  public:
    void setImage(const QImage&) override {
    }

    void reconstruct(const ReconstructionSequence&) override {
    }

    void setSessionController(ImageSessionController *) override {
    }

    void notifyViewStateChanged() override {
        m_notifyViewStateCalled = true;
    }

    void setReconstructor(std::shared_ptr<VkSnapshotReconstructor>) override {
    }

    void update() override {
    }

    void clear() override {
    }

    RenderState renderState() const override {
        return RenderState::Empty;
    }

    double zoomPercentage() const override {
        return m_currentState.percentage();
    }

    QSize getViewportSize() const override {
        return QSize(800, 600);
    }

    // Helpers for testing
    void setState(const ViewState& state) {
        m_currentState = state;
    }
    bool notifyViewStateCalled() const {
        return m_notifyViewStateCalled;
    }
    void reset() {
        m_notifyViewStateCalled = false;
    }

  private:
    ViewState m_currentState;
    bool      m_notifyViewStateCalled = false;
};

class TestViewerController : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        QString tempDb = QDir::tempPath() + "/test_viewercontroller_" +
                         QString::number(QRandomGenerator::global()->generate()) + ".db";
        SnapshotDatabase::instance().init(tempDb);

        // Initialize Vulkan context to enable GPU-accelerated reconstruction tests.
        if (!VulkanContext::instance().initializeInstance()) {
            qWarning() << "VulkanContext failed to initialize. GPU tests may fail.";
        }
    }

    void cleanupTestCase() {
    }

    void testSyncSessionToViewer() {
        AppSettingsController settings;
        ImageSessionController sessionController(&settings);
        ViewerController      controller(&settings);
        ImageSession          session;
        MockViewer            viewer;

        controller.setSessionController(&sessionController);
        sessionController.setActiveSession(&session);
        controller.setViewer(&viewer);

        // Initialize session with dummy image size so state changes are accepted
        session.viewState().resetState(1000, 1000);

        // Set a specific state in the session
        ViewState state;
        state.resetState(1000, 1000);
        state.setPercentage(150.0);
        session.viewState() = state;

        controller.syncSessionToViewer();

        QCOMPARE(viewer.notifyViewStateCalled(), true);
    }

    void testFitToWindow() {
        AppSettingsController settings;
        ImageSessionController sessionController(&settings);
        ViewerController      controller(&settings);
        ImageSession          session;
        MockViewer            viewer;

        controller.setSessionController(&sessionController);
        sessionController.setActiveSession(&session);
        controller.setViewer(&viewer);

        // Initialize session with dummy image size
        session.viewState().resetState(1000, 1000);

        controller.fitToWindow();

        QCOMPARE(viewer.notifyViewStateCalled(), true);
    }

    void testHandleViewportResize() {
        AppSettingsController settings;
        ImageSessionController sessionController(&settings);
        ViewerController      controller(&settings);
        ImageSession          session;
        MockViewer            viewer;

        controller.setSessionController(&sessionController);
        sessionController.setActiveSession(&session);
        controller.setViewer(&viewer);

        // Initialize session with dummy image size
        session.viewState().resetState(1000, 1000);

        controller.handleViewportResize(1920, 1080);

        QCOMPARE(session.viewState().viewportWidth(), 1920);
        QCOMPARE(session.viewState().viewportHeight(), 1080);
        QCOMPARE(viewer.notifyViewStateCalled(), true);
    }

    void testNullPointers() {
        AppSettingsController settings;
        ViewerController      controller(&settings);
        // No session or viewer set.
        // These should not crash.
        controller.syncSessionToViewer();
        controller.fitToWindow();
        controller.handleViewportResize(800, 600);

        QVERIFY(true); // If we reached here, no crash occurred.
    }

    void testScaleWithWindowToggle() {
        AppSettingsController settings;
        ViewerController      controller(&settings);
        bool                  initial = controller.isScaleWithWindowEnabled();

        controller.setScaleWithWindowEnabled(!initial);
        QVERIFY(controller.isScaleWithWindowEnabled() != initial);

        controller.setScaleWithWindowEnabled(initial);
        QVERIFY(controller.isScaleWithWindowEnabled() == initial);
    }

    void testHandleZoomRequested() {
        AppSettingsController settings;
        ImageSessionController sessionController(&settings);
        ViewerController      controller(&settings);
        ImageSession          session;
        MockViewer            viewer;

        controller.setSessionController(&sessionController);
        sessionController.setActiveSession(&session);
        controller.setViewer(&viewer);
        session.viewState().resetState(1000, 1000);

        double initialZoom = session.viewState().percentage();
        controller.handleZoomRequested(true, false); // Zoom In
        QVERIFY(session.viewState().percentage() > initialZoom);
        QCOMPARE(viewer.notifyViewStateCalled(), true);
    }

    void testHandlePanRequested() {
        AppSettingsController settings;
        ImageSessionController sessionController(&settings);
        ViewerController      controller(&settings);
        ImageSession          session;
        MockViewer            viewer;

        controller.setSessionController(&sessionController);
        sessionController.setActiveSession(&session);
        controller.setViewer(&viewer);
        session.viewState().resetState(1000, 1000);

        QPointF initialPan = session.viewState().pan();
        controller.handlePanRequested(10, 20);
        QVERIFY(session.viewState().pan() != initialPan);
        QCOMPARE(viewer.notifyViewStateCalled(), true);
    }

    void testSecondarySelectionStability() {
        AppSettingsController settings;
        ImageSessionController sessionController(&settings);
        ViewerController      controller(&settings);
        ImageSession          session;

        QString tempDir = QDir::tempPath();
        QString path = tempDir + "/ottersnap_stability_test_" +
                       QString::number(QRandomGenerator::global()->generate()) + ".png";

        SnapshotManager::deleteAllSnapshots(path);
        
        QImage imgA(10, 10, QImage::Format_ARGB32);
        imgA.fill(Qt::red);
        auto snapA = SnapshotManager::saveSnapshot(path, imgA);
        if (!snapA) QFAIL("Failed to save snapshot A");
        QUuid uuidA = snapA->uuid;

        QImage imgB(10, 10, QImage::Format_ARGB32);
        imgB.fill(Qt::blue);
        auto snapB = SnapshotManager::saveSnapshot(path, imgB);
        if (!snapB) QFAIL("Failed to save snapshot B");
        QUuid uuidB = snapB->uuid;

        QImage imgC(10, 10, QImage::Format_ARGB32);
        imgC.fill(Qt::green);
        auto snapC = SnapshotManager::saveSnapshot(path, imgC);
        if (!snapC) QFAIL("Failed to save snapshot C");
        QUuid uuidC = snapC->uuid;
        
        session.openImage(path);

        controller.setSessionController(&sessionController);
        sessionController.setActiveSession(&session);

        // Select B as secondary
        controller.setSecondarySnapshot(uuidB.toString(QUuid::WithoutBraces));
        QCOMPARE(controller.secondarySnapshotId(), uuidB.toString(QUuid::WithoutBraces));

        // Delete A
        SnapshotManager::deleteSnapshot(path, uuidA);
        session.reloadImage(); // Refresh session snapshots list

        // Secondary should still be B
        QCOMPARE(controller.secondarySnapshotId(), uuidB.toString(QUuid::WithoutBraces));

        QFile::remove(path);
    }

    void testCanSwap() {
        AppSettingsController settings;
        ViewerController      controller(&settings);
        ImageSession          session;

        // Case 1: No session
        ViewerController noSessionController(&settings);
        QVERIFY(!noSessionController.canSwap());

        // Case 2: Session set, but no secondary selected
        // We need a session coordinator to set the session now
        ImageSessionController sessionCoordinator(&settings);
        controller.setSessionController(&sessionCoordinator);
        sessionCoordinator.setActiveSession(&session);
        QVERIFY(!controller.canSwap());

        // Case 3: Different secondary selected (should be able to swap)
        controller.setSecondarySnapshot(QUuid::createUuid().toString(QUuid::WithoutBraces));
        QVERIFY(controller.canSwap());

        // Case 4: Secondary is the same as primary (should NOT be able to swap)
        // In a default session with no snapshots, primary is SecondaryCurrent (-1)
        controller.setSecondarySnapshot(QString());
        QVERIFY(!controller.canSwap());
    }

    void testSecondarySnapshotHandling() {
        AppSettingsController settings;
        ImageSessionController sessionController(&settings);
        ViewerController      controller(&settings);
        ImageSession          session;

        controller.setSessionController(&sessionController);
        sessionController.setActiveSession(&session);

        // Test setting and getting
        QUuid testUuid = QUuid::createUuid();
        QString testId = testUuid.toString(QUuid::WithoutBraces);
        controller.setSecondarySnapshot(testId);
        QCOMPARE(controller.secondarySnapshotId(), testId);

        // Test signal emission
        QSignalSpy spy(&controller, &ViewerController::secondarySnapshotChanged);
        QUuid nextUuid = QUuid::createUuid();
        QString nextId = nextUuid.toString(QUuid::WithoutBraces);
        controller.setSecondarySnapshot(nextId);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<QString>(), nextId);
    }

    void testSetZoomPercentage() {
        AppSettingsController settings;
        ImageSessionController sessionController(&settings);
        ViewerController      controller(&settings);
        ImageSession          session;
        MockViewer            viewer;

        controller.setSessionController(&sessionController);
        sessionController.setActiveSession(&session);
        controller.setViewer(&viewer);
        session.viewState().resetState(1000, 1000);

        controller.setZoomPercentage(250.0);
        QCOMPARE(session.viewState().percentage(), 250.0);
        QCOMPARE(viewer.notifyViewStateCalled(), true);
    }
};

QTEST_MAIN(TestViewerController)
#include "test_viewercontroller.moc"
