#pragma once

/// @brief Encapsulates the visual effect states for an image.
struct EffectsModel {
    bool grayscale = false;
    bool mirror = false;

    /// @brief Check if the state is different from another state.
    bool operator!=(const EffectsModel& other) const {
        return grayscale != other.grayscale || mirror != other.mirror;
    }

    bool operator==(const EffectsModel& other) const {
        return !(*this != other);
    }
};
