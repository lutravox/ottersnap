#include <QCoreApplication>
#include <QtTest>
#include "controllers/viewcontroller.h"
#include "core/imagesession.h"
#include "core/viewer_interfaces.h"
#include "core/viewstate.h"

/// Mock implementation of IViewer to verify ViewController interactions.
class MockViewer : public IViewer {
  public:
    void setImage(const QImage&, bool) override {
    }

    void reconstruct(const ReconstructionSequence&) override {
    }

    void setSession(ImageSession *) override {
    }

    void setViewState(const ViewState& state) override {
        m_lastState = state;
        m_setViewStateCalled = true;
    }

    void setReconstructor(std::shared_ptr<VkSnapshotReconstructor>) override {
    }

    void update() override {
    }

    ViewState getViewState() const override {
        return m_currentState;
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
    bool setViewStateCalled() const {
        return m_setViewStateCalled;
    }
    ViewState lastState() const {
        return m_lastState;
    }
    void reset() {
        m_setViewStateCalled = false;
    }

  private:
    ViewState m_currentState;
    ViewState m_lastState;
    bool      m_setViewStateCalled = false;
};

class TestViewController : public QObject {
    Q_OBJECT

  private slots:
    void testSyncSessionToViewer() {
        ViewController controller;
        ImageSession   session;
        MockViewer     viewer;

        controller.setActiveSession(&session);
        controller.setViewer(&viewer);

        // Initialize session with dummy image size so state changes are accepted
        session.viewState().resetState(1000, 1000);

        // Set a specific state in the session
        ViewState state;
        state.resetState(1000, 1000);
        state.setPercentage(150.0);
        session.viewState() = state;

        controller.syncSessionToViewer();

        QCOMPARE(viewer.setViewStateCalled(), true);
        QCOMPARE(viewer.lastState().percentage(), 150.0);
    }

    void testSyncViewerToSession() {
        ViewController controller;
        ImageSession   session;
        MockViewer     viewer;

        controller.setActiveSession(&session);
        controller.setViewer(&viewer);

        // Initialize session with dummy image size
        session.viewState().resetState(1000, 1000);

        // Set a specific state in the viewer
        ViewState state;
        state.resetState(1000, 1000);
        state.setPercentage(200.0);
        viewer.setState(state);

        controller.syncViewerToSession();

        QCOMPARE(session.viewState().percentage(), 200.0);
    }

    void testFitToWindow() {
        ViewController controller;
        ImageSession   session;
        MockViewer     viewer;

        controller.setActiveSession(&session);
        controller.setViewer(&viewer);

        // Initialize session with dummy image size
        session.viewState().resetState(1000, 1000);

        controller.fitToWindow();

        QCOMPARE(viewer.setViewStateCalled(), true);
        QCOMPARE(session.viewState().percentage(), viewer.lastState().percentage());
    }

    void testHandleViewportResize() {
        ViewController controller;
        ImageSession   session;
        MockViewer     viewer;

        controller.setActiveSession(&session);
        controller.setViewer(&viewer);

        // Initialize session with dummy image size
        session.viewState().resetState(1000, 1000);

        controller.handleViewportResize(1920, 1080);

        QCOMPARE(session.viewState().viewportWidth(), 1920);
        QCOMPARE(session.viewState().viewportHeight(), 1080);
        QCOMPARE(viewer.setViewStateCalled(), true);
    }

    void testNullPointers() {
        ViewController controller;
        // No session or viewer set.
        // These should not crash.
        controller.syncSessionToViewer();
        controller.syncViewerToSession();
        controller.fitToWindow();
        controller.handleViewportResize(800, 600);

        QVERIFY(true); // If we reached here, no crash occurred.
    }

    void testScaleWithWindowToggle() {
        ViewController controller;
        bool           initial = controller.isScaleWithWindowEnabled();

        controller.setScaleWithWindowEnabled(!initial);
        QVERIFY(controller.isScaleWithWindowEnabled() != initial);

        controller.setScaleWithWindowEnabled(initial);
        QVERIFY(controller.isScaleWithWindowEnabled() == initial);
    }

    void testHandleZoomRequested() {
        ViewController controller;
        ImageSession   session;
        MockViewer     viewer;

        controller.setActiveSession(&session);
        controller.setViewer(&viewer);
        session.viewState().resetState(1000, 1000);

        double initialZoom = session.viewState().percentage();
        controller.handleZoomRequested(true, false); // Zoom In
        QVERIFY(session.viewState().percentage() > initialZoom);
        QCOMPARE(viewer.setViewStateCalled(), true);
    }

    void testHandlePanRequested() {
        ViewController controller;
        ImageSession   session;
        MockViewer     viewer;

        controller.setActiveSession(&session);
        controller.setViewer(&viewer);
        session.viewState().resetState(1000, 1000);

        QPointF initialPan = session.viewState().pan();
        controller.handlePanRequested(10, 20);
        QVERIFY(session.viewState().pan() != initialPan);
        QCOMPARE(viewer.setViewStateCalled(), true);
    }

    void testSetZoomPercentage() {
        ViewController controller;
        ImageSession   session;
        MockViewer     viewer;

        controller.setActiveSession(&session);
        controller.setViewer(&viewer);
        session.viewState().resetState(1000, 1000);

        controller.setZoomPercentage(250.0);
        QCOMPARE(session.viewState().percentage(), 250.0);
        QCOMPARE(viewer.setViewStateCalled(), true);
    }
};

QTEST_MAIN(TestViewController)
#include "test_viewcontroller.moc"
