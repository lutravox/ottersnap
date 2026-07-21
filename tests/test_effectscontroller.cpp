#include <QObject>
#include <QtTest>
#include "controllers/effectscontroller.h"
#include "ui/effects_interfaces.h"

// Mocks

class MockEffectsState : public IEffectsState {
  public:
    bool m_grayscale = false;
    bool m_mirror = false;

    bool grayscaleEnabled() const override {
        return m_grayscale;
    }
    void setGrayscale(bool enabled) override {
        m_grayscale = enabled;
    }
    bool mirrorEnabled() const override {
        return m_mirror;
    }
    void setMirror(bool enabled) override {
        m_mirror = enabled;
    }
};

class MockEffectsRenderer : public IEffectsRenderer {
  public:
    bool                  m_grayscale = false;
    bool                  m_mirror = false;
    EffectChangedCallback m_callback = nullptr;

    void setGrayscale(bool enabled) override {
        m_grayscale = enabled;
    }
    void setMirror(bool enabled) override {
        m_mirror = enabled;
    }
    void setNotificationCallback(EffectChangedCallback callback) override {
        m_callback = callback;
    }
};

class MockEffectsUI : public IEffectsUI {
  public:
    bool m_grayscale = false;
    bool m_mirror = false;

    void setGrayscaleChecked(bool checked) override {
        m_grayscale = checked;
    }
    void setMirrorChecked(bool checked) override {
        m_mirror = checked;
    }
    bool grayscaleChecked() const override {
        return m_grayscale;
    }
    bool mirrorChecked() const override {
        return m_mirror;
    }
};

// Test Suite

class TestEffectsController : public QObject {
    Q_OBJECT

  private slots:
    void init() {
        m_controller = new EffectsController();
        m_state = new MockEffectsState();
        m_renderer = new MockEffectsRenderer();
        m_ui = new MockEffectsUI();

        m_controller->setup(m_renderer, m_ui);
    }

    void cleanup() {
        delete m_controller;
        delete m_state;
        delete m_renderer;
        delete m_ui;
    }

    void testInitialSync() {
        m_state->m_grayscale = true;
        m_state->m_mirror = false;

        m_controller->setTargetState(m_state);

        QCOMPARE(m_ui->m_grayscale, true);
        QCOMPARE(m_ui->m_mirror, false);
        QCOMPARE(m_renderer->m_grayscale, true);
        QCOMPARE(m_renderer->m_mirror, false);
    }

    void testSetGrayscale() {
        m_controller->setTargetState(m_state);

        m_controller->setGrayscale(true);
        QCOMPARE(m_state->m_grayscale, true);
        QCOMPARE(m_ui->m_grayscale, true);
        QCOMPARE(m_renderer->m_grayscale, true);

        m_controller->setGrayscale(false);
        QCOMPARE(m_state->m_grayscale, false);
        QCOMPARE(m_ui->m_grayscale, false);
        QCOMPARE(m_renderer->m_grayscale, false);
    }

    void testSetMirror() {
        m_controller->setTargetState(m_state);

        m_controller->setMirror(true);
        QCOMPARE(m_state->m_mirror, true);
        QCOMPARE(m_ui->m_mirror, true);
        QCOMPARE(m_renderer->m_mirror, true);

        m_controller->setMirror(false);
        QCOMPARE(m_state->m_mirror, false);
        QCOMPARE(m_ui->m_mirror, false);
        QCOMPARE(m_renderer->m_mirror, false);
    }

    void testToggleGrayscale() {
        m_controller->setTargetState(m_state);

        m_state->m_grayscale = false;
        m_controller->toggleGrayscale();
        QCOMPARE(m_state->m_grayscale, true);
        QCOMPARE(m_ui->m_grayscale, true);
        QCOMPARE(m_renderer->m_grayscale, true);

        m_controller->toggleGrayscale();
        QCOMPARE(m_state->m_grayscale, false);
        QCOMPARE(m_ui->m_grayscale, false);
        QCOMPARE(m_renderer->m_grayscale, false);
    }

    void testToggleMirror() {
        m_controller->setTargetState(m_state);

        m_state->m_mirror = false;
        m_controller->toggleMirror();
        QCOMPARE(m_state->m_mirror, true);
        QCOMPARE(m_ui->m_mirror, true);
        QCOMPARE(m_renderer->m_mirror, true);

        m_controller->toggleMirror();
        QCOMPARE(m_state->m_mirror, false);
        QCOMPARE(m_ui->m_mirror, false);
        QCOMPARE(m_renderer->m_mirror, false);
    }

    void testNoStateSync() {
        // Set target state to null
        m_controller->setTargetState(nullptr);

        // When no state, setGrayscale should still update UI and Renderer
        m_controller->setGrayscale(true);
        QCOMPARE(m_ui->m_grayscale, true);
        QCOMPARE(m_renderer->m_grayscale, true);

        m_controller->setMirror(true);
        QCOMPARE(m_ui->m_mirror, true);
        QCOMPARE(m_renderer->m_mirror, true);
    }

  private:
    EffectsController   *m_controller = nullptr;
    MockEffectsState    *m_state = nullptr;
    MockEffectsRenderer *m_renderer = nullptr;
    MockEffectsUI       *m_ui = nullptr;
};

QTEST_MAIN(TestEffectsController)
#include "test_effectscontroller.moc"
