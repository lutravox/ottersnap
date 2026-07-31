#pragma once

#include <functional>

/// @brief Interface for effects state.
class IEffectsState {
  public:
    virtual ~IEffectsState() = default;
    virtual bool grayscaleEnabled() const = 0;
    virtual void setGrayscale(bool enabled) = 0;
    virtual bool mirrorEnabled() const = 0;
    virtual void setMirror(bool enabled) = 0;
};

/// @brief Interface for the component that actually renders the effects.
class IEffectsRenderer {
  public:
    /// @brief Callback type for effect changes.
    using EffectChangedCallback = std::function<void(bool grayscale, bool mirror)>;

    virtual ~IEffectsRenderer() = default;
};

/// @brief Interface for the UI elements related to effects (e.g., menu actions).
class IEffectsUI {
  public:
    virtual ~IEffectsUI() = default;
    virtual void setGrayscaleChecked(bool checked) = 0;
    virtual void setMirrorChecked(bool checked) = 0;
    virtual bool grayscaleChecked() const = 0;
    virtual bool mirrorChecked() const = 0;
};
