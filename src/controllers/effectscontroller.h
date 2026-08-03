#pragma once

#include <QAction>
#include <QObject>
#include "core/effects_interfaces.h"

/// @brief Adapter that maps IEffectsUI calls to QAction checkboxes.
class EffectsUIAdapter : public IEffectsUI {
  public:
    EffectsUIAdapter(QAction *grayscaleAction, QAction *mirrorAction)
        : m_grayscaleAction(grayscaleAction), m_mirrorAction(mirrorAction) {
    }

    void setGrayscaleChecked(bool checked) override {
        if (m_grayscaleAction)
            m_grayscaleAction->setChecked(checked);
    }
    void setMirrorChecked(bool checked) override {
        if (m_mirrorAction)
            m_mirrorAction->setChecked(checked);
    }
    bool grayscaleChecked() const override {
        return m_grayscaleAction ? m_grayscaleAction->isChecked() : false;
    }
    bool mirrorChecked() const override {
        return m_mirrorAction ? m_mirrorAction->isChecked() : false;
    }

  private:
    QAction *m_grayscaleAction = nullptr;
    QAction *m_mirrorAction = nullptr;
};

class EffectsController : public QObject {
    Q_OBJECT

  public:
    explicit EffectsController(QObject *parent = nullptr);

    /// @brief Initialize the controller with the interface implementations.
    void setup(IEffectsRenderer *renderer, IEffectsUI *ui);

    /// @brief Set the current state being managed. Syncs UI and renderer to state.
    void setTargetState(IEffectsState *state);

    /// @brief Reset all effects.
    void reset();

    /// @brief Toggle grayscale mode.
    void toggleGrayscale();

    /// @brief Toggle mirror mode.
    void toggleMirror();

    /// @brief Update the grayscale mode across all synchronized components.
    void setGrayscale(bool enabled);

    /// @brief Update the mirror mode across all synchronized components.
    void setMirror(bool enabled);

  private:
    void syncFromState();
    void onEffectsChanged(bool grayscale, bool mirror);

    IEffectsRenderer *m_renderer = nullptr;
    IEffectsState    *m_state = nullptr;
    IEffectsUI       *m_ui = nullptr;
};
